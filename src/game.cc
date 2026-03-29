export module game;

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

// forward declaration
class WatopolyGame;

// TurnContext - per-turn mutable state
struct TurnContext {
    int currentPlayerIndex = 0;
    TurnPhase phase = TurnPhase::PreRoll;
    int consecutiveDoubles = 0;
    DiceResult lastRoll;
    Player *debtCreditor = nullptr;
    int debtAmount = 0;
};

// Command - abstract base for all game commands
class Command {
public:
    virtual ~Command();
    virtual std::string cmdName() const = 0;
    virtual bool execute(WatopolyGame &game, const std::vector<std::string> &args) = 0;
    virtual bool isValidInPhase(TurnPhase phase) const = 0;
};

// CommandInterpreter - parses input, dispatches to commands
class CommandInterpreter {
    std::map<std::string, std::unique_ptr<Command>> commands_;
public:
    void registerCommand(std::unique_ptr<Command> cmd);
    bool executeLine(const std::string &line, WatopolyGame &game);
};

// WatopolyGame - central controller, implements IGameContext
class WatopolyGame : public IGameContext {
    Board board_;
    std::vector<std::unique_ptr<Player>> players_;
    RandomEventGenerator rng_;
    Dice dice_;
    CommandInterpreter interpreter_;
    DisplayHub displays_;
    BoardDisplay boardDisplay_;
    EventLogger logger_;
    TurnContext turn_;
    bool testingMode_;
    bool replayEnabled_;
    bool running_ = true;

public:
    WatopolyGame(bool testingMode, unsigned seed, bool enableLog, bool enableReplay);

    // game lifecycle
    void setupNewGame();
    void loadGame(const std::string &filename);
    void run();

    // accessors for commands (not part of IGameContext)
    Board &getBoard() { return board_; }
    const Board &getBoard() const { return board_; }
    TurnContext &turnContext() { return turn_; }
    Dice &getDice() { return dice_; }
    bool isTestingMode() const { return testingMode_; }
    bool isReplayEnabled() const { return replayEnabled_; }
    EventLogger &eventLogger() { return logger_; }
    const std::vector<std::unique_ptr<Player>> &playerList() const { return players_; }

    // snapshot for display
    GameSnapshot snapshot() const;
    void redraw();

    // auction
    void runAuction(Property &property);

    // save/load
    void saveGame(const std::string &filename) const;
    void replayEventsFromFile(const std::string &filename) const;

    // IGameContext implementation
    void sendPlayerToTims(Player &player) override;
    void promptPurchase(Player &player, Property &property) override;
    void handleDebt(Player &debtor, int amount, Player *creditor) override;
    void handleTuition(Player &player) override;
    DiceResult rollForGym() override;
    int activeCupCount() const override;
    void awardCup(Player &player) override;
    void resolveLanding(Player &player, int squareIndex) override;
    void logEvent(const std::string &msg) override;
    SLCEvent generateSLCEvent() override;
    int generateNeedlesHallDelta() override;
    bool checkRollUpTheRim() override;
    TurnPhase getCurrentPhase() const override;
    Player &currentPlayer() override;
    std::vector<Player *> getActivePlayers() override;
    Player *findPlayer(const std::string &name) override;

    void printAssets(const Player &player) const;

private:
    void setupCommands();
    void processTurn();
    void handleTimsLine(Player &player);
    bool isGameOver() const;
    void announceWinner() const;
};

// concrete commands
class RollCommand : public Command {
public:
    std::string cmdName() const override { return "roll"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class NextCommand : public Command {
public:
    std::string cmdName() const override { return "next"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class TradeCommand : public Command {
public:
    std::string cmdName() const override { return "trade"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class ImproveCommand : public Command {
public:
    std::string cmdName() const override { return "improve"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class MortgageCommand : public Command {
public:
    std::string cmdName() const override { return "mortgage"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class UnmortgageCommand : public Command {
public:
    std::string cmdName() const override { return "unmortgage"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class BankruptCommand : public Command {
public:
    std::string cmdName() const override { return "bankrupt"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class AssetsCommand : public Command {
public:
    std::string cmdName() const override { return "assets"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class AllCommand : public Command {
public:
    std::string cmdName() const override { return "all"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class SaveCommand : public Command {
public:
    std::string cmdName() const override { return "save"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase phase) const override;
};

class HistoryCommand : public Command {
public:
    std::string cmdName() const override { return "history"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase) const override { return true; }
};

class ReplayCommand : public Command {
public:
    std::string cmdName() const override { return "replay"; }
    bool execute(WatopolyGame &game, const std::vector<std::string> &args) override;
    bool isValidInPhase(TurnPhase) const override { return true; }
};

// main application entry (exported for main.cc)
export class WatopolyApp {
public:
    int run(int argc, char **argv);
};
