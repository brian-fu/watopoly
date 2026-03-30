module game;

import types;
import core;
import board;
import dice;
import display;
import <string>;
import <vector>;
import <map>;
import <memory>;
import <iostream>;
import <sstream>;
import <fstream>;
import <algorithm>;

Command::~Command() = default;

// helper: string-to-int using istringstream (course-approved conversion)
static bool tryParseInt(const std::string &s, int &result) {
    std::istringstream iss{s};
    return static_cast<bool>(iss >> result);
}

// helper: int-to-string using ostringstream (course-approved conversion)
static std::string intToStr(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

// CommandInterpreter
void CommandInterpreter::registerCommand(std::unique_ptr<Command> cmd) {
    std::string name = cmd->cmdName();
    commands_[name] = std::move(cmd);
}

bool CommandInterpreter::executeLine(const std::string &line, WatopolyGame &game) {
    std::istringstream iss{line};
    std::string cmdName;
    if (!(iss >> cmdName)) return false;

    // collect remaining args
    std::vector<std::string> args;
    std::string tok;
    while (iss >> tok) args.push_back(tok);

    // find command (prefix matching for convenience)
    Command *match = nullptr;
    int matchCount = 0;
    for (auto &[name, cmd] : commands_) {
        if (name.find(cmdName) == 0) {
            match = cmd.get();
            ++matchCount;
        }
    }

    if (matchCount == 0) {
        std::cout << "Unknown command: " << cmdName << std::endl;
        return false;
    }
    if (matchCount > 1) {
        // try exact match
        auto it = commands_.find(cmdName);
        if (it != commands_.end()) {
            match = it->second.get();
        } else {
            std::cout << "Ambiguous command: " << cmdName << std::endl;
            return false;
        }
    }

    if (!match->isValidInPhase(game.turnContext().phase)) {
        std::cout << "Cannot use '" << match->cmdName() << "' right now." << std::endl;
        return false;
    }

    return match->execute(game, args);
}

// WatopolyGame
WatopolyGame::WatopolyGame(bool testingMode, unsigned seed, bool enableLog, bool enableReplay)
    : rng_{seed}, dice_{rng_, testingMode}, testingMode_{testingMode}, replayEnabled_{enableReplay} {
    logger_.setEnabled(enableLog);
    displays_.attach(&boardDisplay_);
}

void WatopolyGame::setupNewGame() {
    buildDefaultBoard(board_);
    setupCommands();

    int numPlayers = 0;
    while (numPlayers < 2 || numPlayers > 6) {
        std::cout << "Enter number of players (2-6): ";
        if (!(std::cin >> numPlayers)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }
    }

    std::vector<char> usedTokens;
    std::vector<std::string> usedNames;
    for (int i = 0; i < numPlayers; ++i) {
        std::string name;
        char token;

        // get a valid name first
        bool nameOk = false;
        while (!nameOk) {
            std::cout << "Player " << (i + 1) << " name: ";
            std::cin >> name;
            if (name == "BANK") {
                std::cout << "Name 'BANK' is reserved. Choose another." << std::endl;
                continue;
            }
            if (std::find(usedNames.begin(), usedNames.end(), name) != usedNames.end()) {
                std::cout << "That player name is already taken." << std::endl;
                continue;
            }
            nameOk = true;
        }

        // then get a valid piece separately
        bool pieceOk = false;
        while (!pieceOk) {
            std::cout << "Choose a piece (G, B, D, P, S, $, L, T): ";
            std::cin >> token;
            if (!isValidPieceChar(token)) {
                std::cout << "Invalid piece character." << std::endl;
                continue;
            }
            if (std::find(usedTokens.begin(), usedTokens.end(), token) != usedTokens.end()) {
                std::cout << "That piece is already taken." << std::endl;
                continue;
            }
            pieceOk = true;
        }
        usedNames.push_back(name);
        usedTokens.push_back(token);
        players_.push_back(std::make_unique<Player>(name, fromChar(token)));
    }
}

void WatopolyGame::setupCommands() {
    interpreter_.registerCommand(std::make_unique<RollCommand>());
    interpreter_.registerCommand(std::make_unique<NextCommand>());
    interpreter_.registerCommand(std::make_unique<TradeCommand>());
    interpreter_.registerCommand(std::make_unique<ImproveCommand>());
    interpreter_.registerCommand(std::make_unique<MortgageCommand>());
    interpreter_.registerCommand(std::make_unique<UnmortgageCommand>());
    interpreter_.registerCommand(std::make_unique<BankruptCommand>());
    interpreter_.registerCommand(std::make_unique<AssetsCommand>());
    interpreter_.registerCommand(std::make_unique<AllCommand>());
    interpreter_.registerCommand(std::make_unique<SaveCommand>());
    interpreter_.registerCommand(std::make_unique<HistoryCommand>());
    interpreter_.registerCommand(std::make_unique<ReplayCommand>());
}

void WatopolyGame::run() {
    redraw();
    while (running_ && !std::cin.eof()) {
        if (isGameOver()) {
            announceWinner();
            break;
        }
        processTurn();
    }
}

void WatopolyGame::processTurn() {
    Player &player = currentPlayer();
    if (player.isBankrupt()) {
        turn_.currentPlayerIndex = (turn_.currentPlayerIndex + 1) % static_cast<int>(players_.size());
        return;
    }

    std::cout << "\n--- " << player.getName() << "'s turn ---" << std::endl;
    turn_.phase = TurnPhase::PreRoll;
    turn_.consecutiveDoubles = 0;
    dice_.resetConsecutiveDoubles();

    // handle tims line at start of turn
    if (player.isInTimsLine()) {
        handleTimsLine(player);
        if (player.isInTimsLine()) {
            // still stuck, go to next player
            turn_.phase = TurnPhase::PostRoll;
        } else {
            // player left tims and moved — redraw before entering command loop
            redraw();
        }
    }

    // main command loop for this turn
    while (running_ && !std::cin.eof()) {
        std::cout << "> ";
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        interpreter_.executeLine(line, *this);
        redraw();

        if (turn_.phase == TurnPhase::PostRoll || turn_.phase == TurnPhase::PreRoll) {
            // check if player went bankrupt this turn
            if (player.isBankrupt()) break;
        }

        // "next" command sets phase beyond PostRoll to signal turn end
        if (turn_.phase == TurnPhase::Rolling) break;
    }

    // advance to next non-bankrupt player
    do {
        turn_.currentPlayerIndex = (turn_.currentPlayerIndex + 1) % static_cast<int>(players_.size());
    } while (players_[turn_.currentPlayerIndex]->isBankrupt() && !isGameOver());
}

void WatopolyGame::handleTimsLine(Player &player) {
    std::cout << player.getName() << " is in DC Tims Line (turn "
              << (player.timsTurns() + 1) << " of 3)." << std::endl;

    auto moveAfterLeaving = [&](int steps) {
        player.leaveTimsLine();
        player.moveBy(steps);
        resolveLanding(player, player.getPosition());
        turn_.phase = TurnPhase::PostRoll;
    };

    // helper: roll dice, respecting testing mode if extra args on the line
    auto doTimsRoll = [&](std::istringstream &rest) -> DiceResult {
        if (testingMode_) {
            int d1 = 0, d2 = 0;
            if (rest >> d1 >> d2) {
                return dice_.roll(d1, d2);
            }
        }
        return dice_.roll();
    };

    bool isThirdTurn = (player.timsTurns() >= 2);
    if (isThirdTurn) std::cout << "Final turn in DC Tims Line. You must leave this turn." << std::endl;

    while (true) {
        std::cout << "Options: roll (try doubles), pay ($50), cup (use Roll Up the Rim)" << std::endl;
        std::string line;
        std::cout << "> ";
        if (!std::getline(std::cin, line)) return;
        std::istringstream iss{line};
        std::string choice;
        iss >> choice;

        if (choice == "roll") {
            DiceResult r = doTimsRoll(iss);
            player.setLastRoll(r);
            std::cout << "Rolled: " << r.die1 << " + " << r.die2 << " = " << r.sum << std::endl;
            if (r.isDoubles) {
                std::cout << "Doubles! " << player.getName() << " leaves DC Tims Line." << std::endl;
                logger_.log(player.getName() + " left Tims by rolling doubles");
                moveAfterLeaving(r.sum);
                return;
            }
            if (!isThirdTurn) {
                player.incrementTimsTurns();
                std::cout << "No doubles. Still in DC Tims Line." << std::endl;
                return;
            }
            // third turn: no doubles means forced pay or cup
            std::cout << "No doubles. You must pay $50 or use a Roll Up the Rim cup." << std::endl;
            bool hasCup = player.timsCups() > 0;
            std::string forcedChoice = (hasCup ? "cup/pay" : "pay");
            std::cout << "Choose " << forcedChoice << ": ";
            std::string forced;
            while (true) {
                std::cin >> forced;
                std::cin.ignore(10000, '\n');
                if (forced == "pay") break;
                if (forced == "cup" && hasCup) break;
                if (forced == "cup" && !hasCup)
                    std::cout << "You don't have any Roll Up the Rim cups. Enter 'pay': ";
                else
                    std::cout << "Invalid input. Choose " << forcedChoice << ": ";
            }
            if (forced == "cup") {
                player.useTimsCup();
                std::cout << player.getName() << " used a Roll Up the Rim cup." << std::endl;
                logger_.log(player.getName() + " left Tims using a cup");
            } else {
                handleDebt(player, 50, nullptr);
                if (player.isBankrupt()) return;
                std::cout << player.getName() << " paid $50 to leave DC Tims Line." << std::endl;
                logger_.log(player.getName() + " left Tims by paying $50");
            }
            moveAfterLeaving(r.sum);
            return;
        }

        if (choice == "pay") {
            handleDebt(player, 50, nullptr);
            if (player.isBankrupt()) return;
            std::cout << player.getName() << " paid $50 to leave DC Tims Line." << std::endl;
            logger_.log(player.getName() + " left Tims by paying $50");
            DiceResult r = dice_.roll();
            player.setLastRoll(r);
            std::cout << "Rolled: " << r.die1 << " + " << r.die2 << " = " << r.sum << std::endl;
            moveAfterLeaving(r.sum);
            return;
        }

        if (choice == "cup") {
            if (player.useTimsCup()) {
                std::cout << player.getName() << " used a Roll Up the Rim cup." << std::endl;
                logger_.log(player.getName() + " left Tims using a cup");
                DiceResult r = dice_.roll();
                player.setLastRoll(r);
                std::cout << "Rolled: " << r.die1 << " + " << r.die2 << " = " << r.sum << std::endl;
                moveAfterLeaving(r.sum);
                return;
            }
            std::cout << "You don't have any Roll Up the Rim cups." << std::endl;
            continue;
        }

        std::cout << "Invalid option. Please enter 'roll', 'pay', or 'cup'." << std::endl;
    }
}

GameSnapshot WatopolyGame::snapshot() const {
    GameSnapshot snap;
    for (int i = 0; i < 40; ++i) {
        auto &sq = board_.getSquare(i);
        snap.squares[i].name = sq.name();
        snap.squares[i].isProperty = sq.isProperty();
        snap.squares[i].isAcademic = sq.isAcademic();
        if (sq.isProperty()) {
            auto *prop = static_cast<const Property *>(&sq);
            if (prop->getOwner()) {
                snap.squares[i].ownerName = prop->getOwner()->getName();
            }
            snap.squares[i].mortgaged = prop->isMortgaged();
            if (sq.isAcademic()) {
                auto *ab = static_cast<const AcademicBuilding *>(&sq);
                snap.squares[i].improvements = ab->improvements();
            }
        }
    }
    for (auto &p : players_) {
        PlayerInfo pi;
        pi.name = p->getName();
        pi.token = p->getToken();
        pi.cash = p->getCash();
        pi.position = p->getPosition();
        pi.timsCups = p->timsCups();
        pi.inTimsLine = p->isInTimsLine();
        pi.bankrupt = p->isBankrupt();
        snap.players.push_back(pi);
    }
    return snap;
}

void WatopolyGame::redraw() {
    displays_.publish(snapshot());
}

bool WatopolyGame::isGameOver() const {
    int alive = 0;
    for (auto &p : players_) {
        if (!p->isBankrupt()) ++alive;
    }
    return alive <= 1;
}

void WatopolyGame::announceWinner() const {
    for (auto &p : players_) {
        if (!p->isBankrupt()) {
            std::cout << "\n*** " << p->getName() << " wins! ***" << std::endl;
            return;
        }
    }
}

void WatopolyGame::printAssets(const Player &player) const {
    std::cout << player.getName() << " (" << player.getToken() << "):" << std::endl;
    std::cout << "  Cash: $" << player.getCash() << std::endl;
    std::cout << "  Roll Up the Rim cups: " << player.timsCups() << std::endl;
    std::cout << "  Properties:" << std::endl;
    for (auto *prop : player.properties()) {
        std::cout << "    " << prop->name();
        if (prop->isMortgaged()) std::cout << " [MORTGAGED]";
        if (prop->isAcademic()) {
            auto *ab = static_cast<AcademicBuilding *>(prop);
            if (ab->improvements() > 0)
                std::cout << " (" << ab->improvements() << " improvements)";
        }
        std::cout << std::endl;
    }
    std::cout << "  Total worth: $" << player.getTotalWorth() << std::endl;
}

// IGameContext implementations
void WatopolyGame::sendPlayerToTims(Player &player) {
    player.moveTo(10);
    player.enterTimsLine();
    logger_.log(player.getName() + " sent to DC Tims Line");
}

void WatopolyGame::promptPurchase(Player &player, Property &property) {
    std::cout << property.name() << " is unowned. Cost: $" << property.purchaseCost() << std::endl;
    std::cout << "Would you like to buy it? (yes/no): ";
    std::string response;
    while (true) {
        std::cin >> response;
        std::cin.ignore(10000, '\n');
        if (response == "yes" || response == "Yes" || response == "y" || response == "Y" ||
            response == "no"  || response == "No"  || response == "n" || response == "N") break;
        std::cout << "Invalid input. Please enter 'yes' or 'no': ";
    }

    if (response == "yes" || response == "Yes" || response == "y" || response == "Y") {
        if (player.getCash() >= property.purchaseCost()) {
            player.deductCash(property.purchaseCost());
            property.setOwner(&player);
            player.addProperty(&property);
            std::cout << player.getName() << " bought " << property.name()
                      << " for $" << property.purchaseCost() << "." << std::endl;
            logger_.log(player.getName() + " bought " + property.name());
        } else {
            std::cout << "Not enough money! Property goes to auction." << std::endl;
            runAuction(property);
        }
    } else {
        std::cout << property.name() << " goes to auction." << std::endl;
        runAuction(property);
    }
}

void WatopolyGame::handleDebt(Player &debtor, int amount, Player *creditor) {
    if (debtor.getCash() >= amount) {
        debtor.deductCash(amount);
        if (creditor) creditor->addCash(amount);
        return;
    }

    // can't afford it, enter debt resolution
    turn_.debtCreditor = creditor;
    turn_.debtAmount = amount;
    turn_.phase = TurnPhase::DebtResolution;
    std::cout << debtor.getName() << " owes $" << amount << " but only has $"
              << debtor.getCash() << ". Raise funds or declare bankrupt." << std::endl;

    // let the player try to raise funds through the command loop
    while (turn_.phase == TurnPhase::DebtResolution && !std::cin.eof()) {
        if (debtor.getCash() >= amount) {
            debtor.deductCash(amount);
            if (creditor) creditor->addCash(amount);
            turn_.phase = TurnPhase::PostRoll;
            std::cout << "Debt paid!" << std::endl;
            return;
        }
        std::cout << "[debt] > ";
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        interpreter_.executeLine(line, *this);
    }
}

void WatopolyGame::handleTuition(Player &player) {
    turn_.phase = TurnPhase::TuitionDecision;
    std::cout << "Pay $300 tuition or 10% of your total worth ($"
              << (player.getTotalWorth() / 10) << ")?" << std::endl;
    std::cout << "Enter '1' for $300, '2' for 10%: ";

    std::string choice;
    while (true) {
        std::cin >> choice;
        std::cin.ignore(10000, '\n');
        if (choice == "1" || choice == "2") break;
        std::cout << "Invalid input. Please enter '1' or '2': ";
    }

    int amount;
    if (choice == "2") {
        amount = player.getTotalWorth() / 10;
        std::cout << player.getName() << " pays 10% of worth: $" << amount << std::endl;
    } else {
        amount = 300;
        std::cout << player.getName() << " pays $300 tuition." << std::endl;
    }

    turn_.phase = TurnPhase::PostRoll;
    handleDebt(player, amount, nullptr);
    logger_.log(player.getName() + " paid $" + intToStr(amount) + " tuition");
}

DiceResult WatopolyGame::rollForGym() {
    // gym fee uses rng directly, not through dice (no doubles tracking)
    DiceResult r;
    r.die1 = rng_.rollDie();
    r.die2 = rng_.rollDie();
    r.sum = r.die1 + r.die2;
    r.isDoubles = (r.die1 == r.die2);
    return r;
}

int WatopolyGame::activeCupCount() const {
    int total = 0;
    for (auto &p : players_) {
        if (!p->isBankrupt()) total += p->timsCups();
    }
    return total;
}

void WatopolyGame::awardCup(Player &player) {
    if (activeCupCount() < 4) {
        player.addTimsCup();
        logger_.log(player.getName() + " received a Roll Up the Rim cup");
    }
}

void WatopolyGame::resolveLanding(Player &player, int squareIndex) {
    board_.getSquare(squareIndex).landOn(player, *this);
}

void WatopolyGame::logEvent(const std::string &msg) {
    logger_.log(msg);
}

SLCEvent WatopolyGame::generateSLCEvent() { return rng_.generateSLCEvent(); }
int WatopolyGame::generateNeedlesHallDelta() { return rng_.generateNeedlesHallDelta(); }
bool WatopolyGame::checkRollUpTheRim() { return rng_.rollUpTheRimCheck(activeCupCount()); }

TurnPhase WatopolyGame::getCurrentPhase() const { return turn_.phase; }

Player &WatopolyGame::currentPlayer() {
    return *players_[turn_.currentPlayerIndex];
}

std::vector<Player *> WatopolyGame::getActivePlayers() {
    std::vector<Player *> active;
    for (auto &p : players_) {
        if (!p->isBankrupt()) active.push_back(p.get());
    }
    return active;
}

Player *WatopolyGame::findPlayer(const std::string &name) {
    for (auto &p : players_) {
        if (p->getName() == name) return p.get();
    }
    return nullptr;
}

// auction: each player can raise or withdraw, last one standing wins
void WatopolyGame::runAuction(Property &property) {
    auto bidders = getActivePlayers();
    int currentBid = 0;
    Player *leader = nullptr;
    std::vector<bool> withdrawn(bidders.size(), false);

    std::cout << "Auction for " << property.name() << "!" << std::endl;

    while (true) {
        int activeCount = 0;
        for (int i = 0; i < static_cast<int>(bidders.size()); ++i) {
            if (!withdrawn[i]) ++activeCount;
        }
        if (activeCount <= 1) break;

        for (int i = 0; i < static_cast<int>(bidders.size()); ++i) {
            if (withdrawn[i]) continue;
            // recount since someone might have withdrawn
            activeCount = 0;
            for (int j = 0; j < static_cast<int>(bidders.size()); ++j) {
                if (!withdrawn[j]) ++activeCount;
            }
            if (activeCount <= 1) break;

            std::cout << bidders[i]->getName() << ", current bid: $" << currentBid
                      << ". Enter bid or 'withdraw': ";
            std::string input;
            if (!(std::cin >> input)) {
                // if input ends, close auction cleanly
                std::cin.clear();
                if (std::cin.eof()) {
                    for (int k = 0; k < static_cast<int>(withdrawn.size()); ++k) {
                        withdrawn[k] = true;
                    }
                    break;
                }
                std::cin.ignore(10000, '\n');
                withdrawn[i] = true;
                continue;
            }
            std::cin.ignore(10000, '\n');

            if (input == "withdraw") {
                withdrawn[i] = true;
            } else {
                int bid = 0;
                if (tryParseInt(input, bid)) {
                    if (bid > currentBid && bid <= bidders[i]->getCash()) {
                        currentBid = bid;
                        leader = bidders[i];
                    } else {
                        std::cout << "Invalid bid. Must be > $" << currentBid
                                  << " and within your cash." << std::endl;
                        --i; // retry
                    }
                } else {
                    std::cout << "Invalid input." << std::endl;
                    --i;
                }
            }
        }
    }

    // if nobody explicitly bid, the last remaining player still wins at $0
    Player *winner = leader;
    if (!winner) {
        for (int i = 0; i < static_cast<int>(bidders.size()); ++i) {
            if (!withdrawn[i]) { winner = bidders[i]; break; }
        }
    }

    if (winner) {
        if (currentBid > 0) winner->deductCash(currentBid);
        property.setOwner(winner);
        winner->addProperty(&property);
        std::cout << winner->getName() << " won the auction for " << property.name()
                  << " at $" << currentBid << "." << std::endl;
        logger_.log(winner->getName() + " won auction for " + property.name());
    } else {
        std::cout << "No one bid. " << property.name() << " remains unowned." << std::endl;
    }
}

// save game to file
void WatopolyGame::saveGame(const std::string &filename) const {
    std::ofstream out{filename};
    if (!out) {
        std::cout << "Could not open file: " << filename << std::endl;
        return;
    }

    out << players_.size() << "\n";
    // spec requires current player listed first
    auto writePl = [&](const std::unique_ptr<Player> &p) {
        out << p->getName() << " " << p->getToken() << " " << p->timsCups()
            << " " << p->getCash() << " " << p->getPosition();
        if (p->getPosition() == 10) {
            if (p->isInTimsLine()) {
                out << " 1 " << p->timsTurns();
            } else {
                out << " 0";
            }
        }
        out << "\n";
    };
    int cur = turn_.currentPlayerIndex;
    int n = static_cast<int>(players_.size());
    for (int i = 0; i < n; ++i) {
        writePl(players_[(cur + i) % n]);
    }

    // properties in board order
    int propertyIndices[] = {1,3,5,6,8,9,11,12,13,14,15,16,18,19,21,23,24,25,26,27,28,29,31,32,34,35,37,39};
    for (int idx : propertyIndices) {
        auto &sq = board_.getSquare(idx);
        auto *prop = static_cast<const Property *>(&sq);
        out << sq.name() << " ";
        if (prop->getOwner()) {
            out << prop->getOwner()->getName() << " ";
        } else {
            out << "BANK ";
        }
        if (prop->isMortgaged()) {
            out << -1;
        } else if (sq.isAcademic()) {
            auto *ab = static_cast<const AcademicBuilding *>(&sq);
            out << ab->improvements();
        } else {
            out << 0;
        }
        out << "\n";
    }

    std::cout << "Game saved to " << filename << "." << std::endl;

    if (logger_.isEnabled()) {
        std::ofstream replayOut{filename + ".replay"};
        if (replayOut) {
            for (auto const &event : logger_.events()) {
                replayOut << event << "\n";
            }
        }
    }
}

void WatopolyGame::replayEventsFromFile(const std::string &filename) const {
    std::ifstream in{filename};
    if (!in) {
        std::cout << "Could not open replay file: " << filename << std::endl;
        return;
    }
    std::cout << "--- replay: " << filename << " ---" << std::endl;
    std::string line;
    int i = 1;
    while (std::getline(in, line)) {
        std::cout << "[" << i << "] " << line << std::endl;
        ++i;
    }
    if (i == 1) {
        std::cout << "(no events)" << std::endl;
    }
}

// load game from file
void WatopolyGame::loadGame(const std::string &filename) {
    buildDefaultBoard(board_);
    setupCommands();
    players_.clear();

    std::ifstream in{filename};
    if (!in) {
        std::cout << "Could not open file: " << filename << std::endl;
        return;
    }

    int numPlayers;
    if (!(in >> numPlayers) || numPlayers < 2 || numPlayers > 6) {
        std::cout << "Invalid save file: bad player count." << std::endl;
        return;
    }

    std::vector<std::string> usedNames;
    std::vector<char> usedTokens;
    int totalCups = 0;

    for (int i = 0; i < numPlayers; ++i) {
        std::string name;
        char token;
        int cups, money, position;
        if (!(in >> name >> token >> cups >> money >> position)) {
            std::cout << "Invalid save file: malformed player line." << std::endl;
            return;
        }

        if (name == "BANK" ||
            std::find(usedNames.begin(), usedNames.end(), name) != usedNames.end()) {
            std::cout << "Invalid save file: duplicate/reserved player name." << std::endl;
            return;
        }
        if (!isValidPieceChar(token) ||
            std::find(usedTokens.begin(), usedTokens.end(), token) != usedTokens.end()) {
            std::cout << "Invalid save file: duplicate/invalid token." << std::endl;
            return;
        }
        if (cups < 0 || money < 0 || position < 0 || position > 39 || position == 30) {
            std::cout << "Invalid save file: bad player state values." << std::endl;
            return;
        }

        auto player = std::make_unique<Player>(name, fromChar(token), money);
        player->setPosition(position);
        player->setTimsCups(cups);
        totalCups += cups;

        if (position == 10) {
            int timsFlag;
            if (!(in >> timsFlag) || (timsFlag != 0 && timsFlag != 1)) {
                std::cout << "Invalid save file: bad tims flag." << std::endl;
                return;
            }
            if (timsFlag == 1) {
                int timsTurns;
                if (!(in >> timsTurns) || timsTurns < 0 || timsTurns > 2) {
                    std::cout << "Invalid save file: bad tims turn count." << std::endl;
                    return;
                }
                player->setInTimsLine(true);
                player->setTimsTurns(timsTurns);
            }
        }

        usedNames.push_back(name);
        usedTokens.push_back(token);
        players_.push_back(std::move(player));
    }

    if (totalCups > 4) {
        std::cout << "Invalid save file: total cups exceed 4." << std::endl;
        return;
    }

    // load property states
    int propertyIndices[] = {1,3,5,6,8,9,11,12,13,14,15,16,18,19,21,23,24,25,26,27,28,29,31,32,34,35,37,39};
    for (int idx : propertyIndices) {
        std::string propName, ownerName;
        int improvements;
        if (!(in >> propName >> ownerName >> improvements)) {
            std::cout << "Invalid save file: malformed property line." << std::endl;
            return;
        }

        auto *prop = board_.findProperty(propName);
        if (!prop || prop->index() != idx) {
            std::cout << "Invalid save file: property order/name mismatch." << std::endl;
            return;
        }

        if (ownerName != "BANK") {
            Player *owner = findPlayer(ownerName);
            if (!owner) {
                std::cout << "Invalid save file: unknown property owner." << std::endl;
                return;
            }
            prop->setOwner(owner);
            owner->addProperty(prop);
        }

        if (improvements == -1) {
            prop->mortgage();
            // mortgage() adds cash, undo that since we loaded the cash from file
            if (prop->getOwner()) {
                prop->getOwner()->deductCash(prop->purchaseCost() / 2);
            }
        } else if (prop->isAcademic()) {
            if (improvements < 0 || improvements > 5) {
                std::cout << "Invalid save file: academic improvements out of range." << std::endl;
                return;
            }
            static_cast<AcademicBuilding *>(prop)->setImprovements(improvements);
        } else {
            if (improvements != 0) {
                std::cout << "Invalid save file: non-academic must have 0 improvements." << std::endl;
                return;
            }
        }
    }
}

// concrete command implementations

// RollCommand
bool RollCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::PreRoll;
}

bool RollCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    Player &player = game.currentPlayer();
    if (player.isInTimsLine()) {
        std::cout << "You are in DC Tims Line. Cannot roll normally." << std::endl;
        return false;
    }

    DiceResult r;
    if (game.isTestingMode() && args.size() >= 2) {
        int d1 = 0;
        int d2 = 0;
        if (!tryParseInt(args[0], d1) || !tryParseInt(args[1], d2)) {
            std::cout << "Testing roll expects integers: roll <die1> <die2>" << std::endl;
            return false;
        }
        if (d1 < 0 || d2 < 0) {
            std::cout << "Testing dice must be non-negative." << std::endl;
            return false;
        }
        r = game.getDice().roll(d1, d2);
    } else {
        r = game.getDice().roll();
    }

    player.setLastRoll(r);
    std::cout << player.getName() << " rolled " << r.die1 << " + " << r.die2
              << " = " << r.sum;
    if (r.isDoubles) std::cout << " (doubles!)";
    std::cout << std::endl;

    game.eventLogger().log(player.getName() + " rolled " + intToStr(r.die1) +
                           "+" + intToStr(r.die2) + "=" + intToStr(r.sum));

    // triple doubles -> sent to tims
    if (game.getDice().consecutiveDoubles() >= 3) {
        std::cout << "Three doubles in a row! " << player.getName()
                  << " is sent to DC Tims Line." << std::endl;
        game.sendPlayerToTims(player);
        game.turnContext().phase = TurnPhase::PostRoll;
        return true;
    }

    int posBefore = player.getPosition();
    player.moveBy(r.sum);
    int posAfter = player.getPosition();
    // detect OSAP crossing (wrapped around but didn't land on 0)
    if (posAfter > 0 && posAfter < posBefore) {
        game.eventLogger().log(player.getName() + " crossed OSAP, collected $200");
    }
    std::cout << player.getName() << " lands on " << game.getBoard().getSquare(player.getPosition()).name()
              << " (square " << player.getPosition() << ")." << std::endl;
    game.resolveLanding(player, player.getPosition());

    if (r.isDoubles && !player.isInTimsLine() && !player.isBankrupt()) {
        game.turnContext().phase = TurnPhase::PreRoll;
        std::cout << "Doubles! Roll again." << std::endl;
    } else {
        game.turnContext().phase = TurnPhase::PostRoll;
    }
    return true;
}

// NextCommand
bool NextCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::PostRoll;
}

bool NextCommand::execute(WatopolyGame &game, const std::vector<std::string> &) {
    game.turnContext().phase = TurnPhase::Rolling; // signals turn end
    return true;
}

// TradeCommand
bool TradeCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::PreRoll || phase == TurnPhase::PostRoll;
}

bool TradeCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    if (args.size() < 3) {
        std::cout << "Usage: trade <name> <give> <receive>" << std::endl;
        return false;
    }

    Player &offerer = game.currentPlayer();
    Player *target = game.findPlayer(args[0]);
    if (!target) {
        std::cout << "Player not found: " << args[0] << std::endl;
        return false;
    }

    // determine if give/receive are money or property
    bool giveIsMoney = true, receiveIsMoney = true;
    int giveCash = 0, receiveCash = 0;
    Property *giveProp = nullptr;
    Property *receiveProp = nullptr;

    if (!tryParseInt(args[1], giveCash)) {
        giveIsMoney = false;
        giveProp = game.getBoard().findProperty(args[1]);
    }

    if (!tryParseInt(args[2], receiveCash)) {
        receiveIsMoney = false;
        receiveProp = game.getBoard().findProperty(args[2]);
    }

    // reject money-for-money
    if (giveIsMoney && receiveIsMoney) {
        std::cout << "Cannot trade money for money." << std::endl;
        return false;
    }
    if (giveIsMoney && giveCash <= 0) {
        std::cout << "Cash offer must be positive." << std::endl;
        return false;
    }
    if (receiveIsMoney && receiveCash <= 0) {
        std::cout << "Cash request must be positive." << std::endl;
        return false;
    }
    if (!giveIsMoney && !giveProp) {
        std::cout << "Unknown property in give field." << std::endl;
        return false;
    }
    if (!receiveIsMoney && !receiveProp) {
        std::cout << "Unknown property in receive field." << std::endl;
        return false;
    }

    // validate property ownership
    if (giveProp) {
        if (giveProp->getOwner() != &offerer) {
            std::cout << "You don't own " << giveProp->name() << "." << std::endl;
            return false;
        }
        if (giveProp->isAcademic()) {
            auto *ab = static_cast<AcademicBuilding *>(giveProp);
            if (ab->getBlock()->hasAnyImprovements()) {
                std::cout << "Cannot trade: monopoly has improvements." << std::endl;
                return false;
            }
        }
    }
    if (receiveProp) {
        if (receiveProp->getOwner() != target) {
            std::cout << target->getName() << " doesn't own " << receiveProp->name() << "." << std::endl;
            return false;
        }
        if (receiveProp->isAcademic()) {
            auto *ab = static_cast<AcademicBuilding *>(receiveProp);
            if (ab->getBlock()->hasAnyImprovements()) {
                std::cout << "Cannot trade: monopoly has improvements." << std::endl;
                return false;
            }
        }
    }

    // present trade
    std::cout << offerer.getName() << " offers ";
    if (giveIsMoney) std::cout << "$" << giveCash;
    else if (giveProp) std::cout << giveProp->name();
    std::cout << " to " << target->getName() << " for ";
    if (receiveIsMoney) std::cout << "$" << receiveCash;
    else if (receiveProp) std::cout << receiveProp->name();
    std::cout << ". Accept? (accept/reject): ";

    std::string response;
    while (true) {
        std::cin >> response;
        std::cin.ignore(10000, '\n');
        if (response == "accept" || response == "reject") break;
        std::cout << "Invalid input. Please type 'accept' or 'reject': ";
    }

    if (response != "accept") {
        std::cout << "Trade rejected." << std::endl;
        return false;
    }

    // execute trade
    if (giveIsMoney) {
        if (!offerer.deductCash(giveCash)) {
            std::cout << "You do not have enough cash for this trade." << std::endl;
            return false;
        }
        target->addCash(giveCash);
    } else if (giveProp) {
        offerer.removeProperty(giveProp);
        giveProp->setOwner(target);
        target->addProperty(giveProp);
    }

    if (receiveIsMoney) {
        if (!target->deductCash(receiveCash)) {
            // rollback give side
            if (giveIsMoney) {
                target->deductCash(giveCash);
                offerer.addCash(giveCash);
            } else if (giveProp) {
                target->removeProperty(giveProp);
                giveProp->setOwner(&offerer);
                offerer.addProperty(giveProp);
            }
            std::cout << target->getName() << " does not have enough cash for this trade." << std::endl;
            return false;
        }
        offerer.addCash(receiveCash);
    } else if (receiveProp) {
        target->removeProperty(receiveProp);
        receiveProp->setOwner(&offerer);
        offerer.addProperty(receiveProp);
    }

    // mortgaged property transfer: new owner pays 10% to bank immediately
    auto handleMortgageTransfer = [](Property *prop, Player *newOwner) {
        if (prop && prop->isMortgaged()) {
            int fee = prop->purchaseCost() / 10;
            if (newOwner->getCash() >= fee) {
                newOwner->deductCash(fee);
            }
            prop->setTransferredMortgaged(true);
        }
    };
    if (giveProp) handleMortgageTransfer(giveProp, target);
    if (receiveProp) handleMortgageTransfer(receiveProp, &offerer);

    std::cout << "Trade completed!" << std::endl;
    game.eventLogger().log(offerer.getName() + " traded with " + target->getName());
    return true;
}

// ImproveCommand
bool ImproveCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::PreRoll || phase == TurnPhase::PostRoll;
}

bool ImproveCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    if (args.size() < 2) {
        std::cout << "Usage: improve <property> buy/sell" << std::endl;
        return false;
    }

    Property *prop = game.getBoard().findProperty(args[0]);
    if (!prop || !prop->isAcademic()) {
        std::cout << "Not an academic building." << std::endl;
        return false;
    }

    auto *ab = static_cast<AcademicBuilding *>(prop);
    if (ab->getOwner() != &game.currentPlayer()) {
        std::cout << "You don't own this property." << std::endl;
        return false;
    }

    if (args[1] == "buy") {
        if (ab->buyImprovement()) {
            std::cout << "Improvement bought on " << ab->name()
                      << ". Now has " << ab->improvements() << " improvements." << std::endl;
            game.eventLogger().log("improvement bought on " + ab->name());
        } else {
            std::cout << "Cannot buy improvement." << std::endl;
        }
    } else if (args[1] == "sell") {
        if (ab->sellImprovement()) {
            std::cout << "Improvement sold on " << ab->name()
                      << ". Now has " << ab->improvements() << " improvements." << std::endl;
            game.eventLogger().log("improvement sold on " + ab->name());
        } else {
            std::cout << "No improvements to sell." << std::endl;
        }
    }
    return true;
}

// MortgageCommand
bool MortgageCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::PreRoll || phase == TurnPhase::PostRoll
        || phase == TurnPhase::DebtResolution;
}

bool MortgageCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    if (args.empty()) {
        std::cout << "Usage: mortgage <property>" << std::endl;
        return false;
    }

    Property *prop = game.getBoard().findProperty(args[0]);
    if (!prop) { std::cout << "Property not found." << std::endl; return false; }
    if (prop->getOwner() != &game.currentPlayer()) { std::cout << "Not yours." << std::endl; return false; }
    if (prop->isMortgaged()) { std::cout << "Already mortgaged." << std::endl; return false; }

    // must sell all improvements in the block first
    if (prop->isAcademic()) {
        auto *ab = static_cast<AcademicBuilding *>(prop);
        if (ab->getBlock()->hasAnyImprovements()) {
            std::cout << "Sell improvements first." << std::endl;
            return false;
        }
    }

    prop->mortgage();
    std::cout << prop->name() << " mortgaged. You received $"
              << prop->purchaseCost() / 2 << "." << std::endl;
    game.eventLogger().log(game.currentPlayer().getName() + " mortgaged " + prop->name());
    return true;
}

// UnmortgageCommand
bool UnmortgageCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::PreRoll || phase == TurnPhase::PostRoll;
}

bool UnmortgageCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    if (args.empty()) {
        std::cout << "Usage: unmortgage <property>" << std::endl;
        return false;
    }

    Property *prop = game.getBoard().findProperty(args[0]);
    if (!prop) { std::cout << "Property not found." << std::endl; return false; }
    if (prop->getOwner() != &game.currentPlayer()) { std::cout << "Not yours." << std::endl; return false; }
    if (!prop->isMortgaged()) { std::cout << "Not mortgaged." << std::endl; return false; }

    // cost is 60% of purchase, or 70% if transferred mortgaged from bankruptcy
    int cost = prop->purchaseCost() * 6 / 10;
    if (prop->wasTransferredMortgaged()) {
        cost = prop->purchaseCost() * 7 / 10;
    }

    if (game.currentPlayer().getCash() < cost) {
        std::cout << "Not enough cash ($" << cost << " needed)." << std::endl;
        return false;
    }

    game.currentPlayer().deductCash(cost);
    prop->unmortgage();
    std::cout << prop->name() << " unmortgaged for $" << cost << "." << std::endl;
    game.eventLogger().log(game.currentPlayer().getName() + " unmortgaged " + prop->name());
    return true;
}

// BankruptCommand
bool BankruptCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::DebtResolution;
}

bool BankruptCommand::execute(WatopolyGame &game, const std::vector<std::string> &) {
    Player &player = game.currentPlayer();
    Player *creditor = game.turnContext().debtCreditor;

    if (creditor) {
        // bankrupt to another player: transfer all assets
        std::cout << player.getName() << " declares bankruptcy to " << creditor->getName() << "!" << std::endl;

        creditor->addCash(player.getCash());
        // transfer cups
        while (player.timsCups() > 0) {
            player.useTimsCup();
            creditor->addTimsCup();
        }

        // transfer properties
        auto props = player.properties();
        for (auto *prop : props) {
            // mortgaged property: creditor pays 10% immediately
            if (prop->isMortgaged()) {
                int fee = prop->purchaseCost() / 10;
                if (creditor->getCash() >= fee) {
                    creditor->deductCash(fee);
                } else {
                    // creditor can't afford, property stays mortgaged
                }
                prop->setTransferredMortgaged(true);
            }
            prop->setOwner(creditor);
            creditor->addProperty(prop);
            player.removeProperty(prop);
        }
    } else {
        // bankrupt to the bank: properties go to auction
        std::cout << player.getName() << " declares bankruptcy to the Bank!" << std::endl;
        player.declareBankruptcy();
        while (player.timsCups() > 0) player.useTimsCup();
        auto props = player.properties(); // copy the list
        for (auto *prop : props) {
            prop->setOwner(nullptr);
            // unmortgage for auction
            if (prop->isMortgaged()) {
                prop->unmortgage();
            }
            if (prop->isAcademic()) {
                static_cast<AcademicBuilding *>(prop)->setImprovements(0);
            }
            player.removeProperty(prop);
            game.runAuction(*prop);
        }
        // cups are destroyed
    }

    if (!player.isBankrupt()) {
        player.declareBankruptcy();
    }
    game.turnContext().phase = TurnPhase::PostRoll;
    game.eventLogger().log(player.getName() + " declared bankruptcy");
    return true;
}

// AssetsCommand
bool AssetsCommand::isValidInPhase(TurnPhase phase) const {
    return phase != TurnPhase::TuitionDecision;
}

bool AssetsCommand::execute(WatopolyGame &game, const std::vector<std::string> &) {
    game.printAssets(game.currentPlayer());
    return true;
}

// AllCommand
bool AllCommand::isValidInPhase(TurnPhase phase) const {
    return phase != TurnPhase::TuitionDecision;
}

bool AllCommand::execute(WatopolyGame &game, const std::vector<std::string> &) {
    for (auto &p : game.playerList()) {
        if (!p->isBankrupt()) game.printAssets(*p);
    }
    return true;
}

// SaveCommand
bool SaveCommand::isValidInPhase(TurnPhase phase) const {
    return phase == TurnPhase::PreRoll || phase == TurnPhase::PostRoll;
}

bool SaveCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    if (args.empty()) {
        std::cout << "Usage: save <filename>" << std::endl;
        return false;
    }
    game.saveGame(args[0]);
    return true;
}

// HistoryCommand (enhancement)
bool HistoryCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    if (!game.eventLogger().isEnabled()) {
        std::cout << "History logging is disabled. Start the game with -enablelog to enable it." << std::endl;
        return false;
    }
    int n = -1;
    if (!args.empty()) {
        tryParseInt(args[0], n);
    }
    game.eventLogger().printHistory(n);
    return true;
}

bool ReplayCommand::execute(WatopolyGame &game, const std::vector<std::string> &args) {
    if (!game.isReplayEnabled()) {
        std::cout << "Replay mode is disabled. Start with -enable-replay." << std::endl;
        return false;
    }

    if (args.empty()) {
        game.eventLogger().printHistory();
        return true;
    }

    const std::string &filename = args[0];
    if (filename.size() < 7 || filename.substr(filename.size() - 7) != ".replay") {
        std::cout << "Replay file must have a .replay extension." << std::endl;
        return false;
    }
    game.replayEventsFromFile(filename);
    return true;
}

// WatopolyApp
int WatopolyApp::run(int argc, char **argv) {
    bool testing = false;
    bool enableLog = false;
    bool enableReplay = false;
    unsigned seed = 0;
    bool hasSeed = false;
    std::string loadFile;

    for (int i = 1; i < argc; ++i) {
        std::string arg{argv[i]};
        if (arg == "-testing") {
            testing = true;
        } else if (arg == "-load" && i + 1 < argc) {
            loadFile = argv[++i];
        } else if (arg == "-seed" && i + 1 < argc) {
            int seedVal = 0;
            std::string seedArg{argv[++i]};
            if (tryParseInt(seedArg, seedVal)) seed = static_cast<unsigned>(seedVal);
            hasSeed = true;
        } else if (arg == "-enablelog" || arg == "-enable-log") {
            enableLog = true;
        } else if (arg == "-enable-replay") {
            enableReplay = true;
        }
    }

    if (!hasSeed) {
        seed = 42;
    }

    WatopolyGame game{testing, seed, enableLog, enableReplay};

    if (!loadFile.empty()) {
        game.loadGame(loadFile);
    } else {
        game.setupNewGame();
    }

    game.run();
    return 0;
}
