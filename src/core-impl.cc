module core;

import types;
import <string>;
import <vector>;
import <algorithm>;
import <iostream>;

// Player
Player::Player(std::string name, PieceType token, int cash)
    : name_{std::move(name)}, token_{token}, cash_{cash}, position_{0},
      timsCups_{0}, inTimsLine_{false}, timsTurns_{0}, bankrupt_{false} {}

void Player::addCash(int amount) { cash_ += amount; }

bool Player::deductCash(int amount) {
    if (cash_ >= amount) {
        cash_ -= amount;
        return true;
    }
    return false;
}

void Player::moveBy(int steps) {
    int oldPos = position_;
    position_ = (position_ + steps) % 40;
    if (position_ < 0) position_ += 40;
    // award osap only for strict crossing (not landing exactly on 0)
    if (steps > 0 && position_ != 0 && position_ < oldPos) {
        addCash(200);
        std::cout << getName() << " collected $200 for passing Collect OSAP." << std::endl;
    }
}

void Player::moveTo(int position) { position_ = position; }

void Player::addProperty(Property *prop) {
    ownedProperties_.push_back(prop);
}

void Player::removeProperty(Property *prop) {
    auto it = std::find(ownedProperties_.begin(), ownedProperties_.end(), prop);
    if (it != ownedProperties_.end()) ownedProperties_.erase(it);
}

int Player::getTotalWorth() const {
    int worth = cash_;
    for (auto *prop : ownedProperties_) {
        worth += prop->purchaseCost();
        if (prop->isAcademic()) {
            auto *ab = static_cast<AcademicBuilding *>(prop);
            worth += ab->improvements() * ab->improvementCost();
        }
    }
    return worth;
}

void Player::enterTimsLine() {
    inTimsLine_ = true;
    timsTurns_ = 0;
}

void Player::leaveTimsLine() {
    inTimsLine_ = false;
    timsTurns_ = 0;
}

void Player::incrementTimsTurns() { ++timsTurns_; }

void Player::addTimsCup() { ++timsCups_; }

bool Player::useTimsCup() {
    if (timsCups_ > 0) {
        --timsCups_;
        return true;
    }
    return false;
}

void Player::declareBankruptcy() { bankrupt_ = true; }

void Player::setLastRoll(DiceResult r) { lastRoll_ = r; }
void Player::setPosition(int pos) { position_ = pos; }
void Player::setCash(int c) { cash_ = c; }
void Player::setTimsCups(int n) { timsCups_ = n; }
void Player::setTimsTurns(int n) { timsTurns_ = n; }
void Player::setInTimsLine(bool v) { inTimsLine_ = v; }

// Square
Square::Square(std::string name, int index)
    : name_{std::move(name)}, index_{index} {}

// Property
Property::Property(std::string name, int index, int cost)
    : Square{std::move(name), index}, purchaseCost_{cost} {}

void Property::mortgage() {
    mortgaged_ = true;
    if (owner_) owner_->addCash(purchaseCost_ / 2);
}

void Property::unmortgage() {
    mortgaged_ = false;
    transferredMortgaged_ = false;
}

// template method: determines landing outcome, delegates decisions to game
void Property::landOn(Player &player, IGameContext &game) {
    if (!owner_) {
        game.promptPurchase(player, *this);
    } else if (owner_ == &player) {
        // landed on own property, nothing happens
    } else if (mortgaged_) {
        std::cout << name() << " is mortgaged. No fee owed." << std::endl;
    } else {
        int fee = calculateFee(player, game);
        std::cout << player.getName() << " owes $" << fee
                  << " to " << owner_->getName() << "." << std::endl;
        game.handleDebt(player, fee, owner_);
    }
}

// MonopolyBlock
MonopolyBlock::MonopolyBlock(std::string name) : name_{std::move(name)} {}

void MonopolyBlock::addBuilding(AcademicBuilding *b) { buildings_.push_back(b); }

bool MonopolyBlock::isOwnedBy(const Player &player) const {
    if (buildings_.empty()) return false;
    for (auto *b : buildings_) {
        if (b->getOwner() != &player) return false;
    }
    return true;
}

bool MonopolyBlock::hasAnyImprovements() const {
    for (auto *b : buildings_) {
        if (b->improvements() > 0) return true;
    }
    return false;
}

// AcademicBuilding
AcademicBuilding::AcademicBuilding(std::string name, int index, int cost,
                                   int improvementCost, std::vector<int> tuition,
                                   MonopolyBlock *block)
    : Property{std::move(name), index, cost},
      block_{block}, improvementCost_{improvementCost}, tuition_{std::move(tuition)} {}

bool AcademicBuilding::canBuyImprovement() const {
    if (!getOwner()) return false;
    if (numImprovements_ >= 5) return false;
    if (!block_->isOwnedBy(*getOwner())) return false;
    if (getOwner()->getCash() < improvementCost_) return false;
    return true;
}

bool AcademicBuilding::buyImprovement() {
    if (!canBuyImprovement()) return false;
    getOwner()->deductCash(improvementCost_);
    ++numImprovements_;
    return true;
}

bool AcademicBuilding::sellImprovement() {
    if (numImprovements_ <= 0) return false;
    --numImprovements_;
    getOwner()->addCash(improvementCost_ / 2);
    return true;
}

int AcademicBuilding::calculateFee(Player &, IGameContext &) {
    if (numImprovements_ > 0) {
        return tuition_[numImprovements_];
    }
    // no improvements but owner has full monopoly -> double base tuition
    if (block_->isOwnedBy(*getOwner())) {
        return tuition_[0] * 2;
    }
    return tuition_[0];
}

// Residence
Residence::Residence(std::string name, int index)
    : Property{std::move(name), index, 200} {}

int Residence::calculateFee(Player &, IGameContext &) {
    int count = 0;
    for (auto *prop : getOwner()->properties()) {
        if (dynamic_cast<Residence *>(prop)) ++count;
    }
    switch (count) {
        case 1: return 25;
        case 2: return 50;
        case 3: return 100;
        case 4: return 200;
        default: return 25;
    }
}

// Gym
Gym::Gym(std::string name, int index)
    : Property{std::move(name), index, 150} {}

int Gym::calculateFee(Player &, IGameContext &game) {
    // gym fee uses separate roll (not Dice, avoids doubles contamination)
    DiceResult r = game.rollForGym();
    std::cout << "Gym fee roll: " << r.die1 << " + " << r.die2
              << " = " << r.sum << std::endl;
    int gymsOwned = 0;
    for (auto *prop : getOwner()->properties()) {
        if (dynamic_cast<Gym *>(prop)) ++gymsOwned;
    }
    return r.sum * (gymsOwned >= 2 ? 10 : 4);
}

// CollectOSAP
CollectOSAP::CollectOSAP(int index) : Square{"Collect OSAP", index} {}

void CollectOSAP::landOn(Player &player, IGameContext &) {
    player.addCash(200);
    std::cout << player.getName() << " landed on Collect OSAP and received $200." << std::endl;
}

// DCTimsLine
DCTimsLine::DCTimsLine(int index) : Square{"DC Tims Line", index} {}

void DCTimsLine::landOn(Player &, IGameContext &) {
    // landing on dc tims line does nothing
    // being SENT here is handled by game.sendPlayerToTims
}

// GoToTims
GoToTims::GoToTims(int index) : Square{"Go to Tims", index} {}

void GoToTims::landOn(Player &player, IGameContext &game) {
    std::cout << player.getName() << " is sent to DC Tims Line!" << std::endl;
    game.sendPlayerToTims(player);
}

// GooseNesting
GooseNesting::GooseNesting(int index) : Square{"Goose Nesting", index} {}

void GooseNesting::landOn(Player &player, IGameContext &) {
    std::cout << player.getName() << " has been attacked by a flock of nesting geese!" << std::endl;
}

// TuitionSquare
TuitionSquare::TuitionSquare(int index) : Square{"Tuition", index} {}

void TuitionSquare::landOn(Player &player, IGameContext &game) {
    game.handleTuition(player);
}

// CoopFee
CoopFee::CoopFee(int index) : Square{"Coop Fee", index} {}

void CoopFee::landOn(Player &player, IGameContext &game) {
    std::cout << player.getName() << " pays $150 coop fee." << std::endl;
    game.handleDebt(player, 150, nullptr);
}

// SLCSquare
SLCSquare::SLCSquare(std::string name, int index)
    : Square{std::move(name), index} {}

void SLCSquare::landOn(Player &player, IGameContext &game) {
    // 1% cup check (if < 4 active), replaces normal effect
    if (game.checkRollUpTheRim()) {
        std::cout << player.getName() << " received a Roll Up the Rim cup!" << std::endl;
        game.awardCup(player);
        return;
    }

    SLCEvent event = game.generateSLCEvent();
    int offset = 0;
    switch (event) {
        case SLCEvent::Back3: offset = -3; break;
        case SLCEvent::Back2: offset = -2; break;
        case SLCEvent::Back1: offset = -1; break;
        case SLCEvent::Forward1: offset = 1; break;
        case SLCEvent::Forward2: offset = 2; break;
        case SLCEvent::Forward3: offset = 3; break;
        case SLCEvent::GoToTims:
            std::cout << player.getName() << " is sent to DC Tims Line by SLC!" << std::endl;
            game.sendPlayerToTims(player);
            return;
        case SLCEvent::AdvanceToOSAP:
            std::cout << player.getName() << " advances to Collect OSAP!" << std::endl;
            player.moveTo(0);
            game.resolveLanding(player, 0);
            return;
    }

    int newPos = (player.getPosition() + offset + 40) % 40;
    std::cout << player.getName() << " moves " << offset << " squares to square "
              << newPos << "." << std::endl;
    player.moveTo(newPos);
    game.resolveLanding(player, newPos);
}

// NeedlesHallSquare
NeedlesHallSquare::NeedlesHallSquare(std::string name, int index)
    : Square{std::move(name), index} {}

void NeedlesHallSquare::landOn(Player &player, IGameContext &game) {
    if (game.checkRollUpTheRim()) {
        std::cout << player.getName() << " received a Roll Up the Rim cup!" << std::endl;
        game.awardCup(player);
        return;
    }

    int delta = game.generateNeedlesHallDelta();
    if (delta >= 0) {
        std::cout << player.getName() << " gains $" << delta << " from Needles Hall." << std::endl;
        player.addCash(delta);
    } else {
        std::cout << player.getName() << " loses $" << -delta << " from Needles Hall." << std::endl;
        game.handleDebt(player, -delta, nullptr);
    }
}
