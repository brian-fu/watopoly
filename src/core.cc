export module core;

import types;
import <string>;
import <vector>;
import <array>;
import <iostream>;

// forward declarations within the module
export class Player;
export class Property;
export class MonopolyBlock;
export class AcademicBuilding;

// IGameContext - the narrow interface that all squares call back through
// WatopolyGame implements this; squares never see the concrete game class
export class IGameContext {
public:
    virtual ~IGameContext() = default;

    // actions triggered by squares
    virtual void sendPlayerToTims(Player &player) = 0;
    virtual void promptPurchase(Player &player, Property &property) = 0;
    virtual void handleDebt(Player &debtor, int amount, Player *creditor) = 0;
    virtual void handleTuition(Player &player) = 0;
    virtual DiceResult rollForGym() = 0;
    virtual int activeCupCount() const = 0;
    virtual void awardCup(Player &player) = 0;
    virtual void resolveLanding(Player &player, int squareIndex) = 0;

    // rng delegated from squares so core doesn't import dice
    virtual SLCEvent generateSLCEvent() = 0;
    virtual int generateNeedlesHallDelta() = 0;
    virtual bool checkRollUpTheRim() = 0;

    // state queries
    virtual TurnPhase getCurrentPhase() const = 0;
    virtual Player &currentPlayer() = 0;
    virtual std::vector<Player *> getActivePlayers() = 0;
    virtual Player *findPlayer(const std::string &name) = 0;
};

// Player - mutable player state, owned by WatopolyGame via unique_ptr
export class Player {
    std::string name_;
    PieceType token_;
    int cash_;
    int position_;
    std::vector<Property *> ownedProperties_;
    int timsCups_;
    bool inTimsLine_;
    int timsTurns_;
    bool bankrupt_;
    DiceResult lastRoll_;

public:
    Player(std::string name, PieceType token, int cash = 1500);

    const std::string &getName() const { return name_; }
    char getToken() const { return toChar(token_); }
    PieceType getTokenType() const { return token_; }
    int getCash() const { return cash_; }
    int getPosition() const { return position_; }
    const std::vector<Property *> &properties() const { return ownedProperties_; }
    bool isInTimsLine() const { return inTimsLine_; }
    int timsTurns() const { return timsTurns_; }
    int timsCups() const { return timsCups_; }
    bool isBankrupt() const { return bankrupt_; }
    DiceResult getLastRoll() const { return lastRoll_; }

    void addCash(int amount);
    bool deductCash(int amount);
    void moveBy(int steps);
    void moveTo(int position);
    void addProperty(Property *prop);
    void removeProperty(Property *prop);
    int getTotalWorth() const;
    void enterTimsLine();
    void leaveTimsLine();
    void incrementTimsTurns();
    void addTimsCup();
    bool useTimsCup();
    void declareBankruptcy();
    void setLastRoll(DiceResult r);

    // used by save/load
    void setPosition(int pos);
    void setCash(int c);
    void setTimsCups(int n);
    void setTimsTurns(int n);
    void setInTimsLine(bool v);
};

// Square - abstract base for all 40 board positions
export class Square {
    std::string name_;
    int index_;
public:
    Square(std::string name, int index);
    virtual ~Square() = default;

    const std::string &name() const { return name_; }
    int index() const { return index_; }

    virtual void landOn(Player &player, IGameContext &game) = 0;
    virtual bool isProperty() const { return false; }
    virtual bool isAcademic() const { return false; }
};

// Property - abstract ownable square (template method: landOn calls calculateFee)
export class Property : public Square {
    Player *owner_ = nullptr;
    int purchaseCost_;
    bool mortgaged_ = false;
    bool transferredMortgaged_ = false;

public:
    Property(std::string name, int index, int cost);

    Player *getOwner() const { return owner_; }
    void setOwner(Player *p) { owner_ = p; }
    int purchaseCost() const { return purchaseCost_; }
    bool isMortgaged() const { return mortgaged_; }
    void mortgage();
    void unmortgage();
    bool wasTransferredMortgaged() const { return transferredMortgaged_; }
    void setTransferredMortgaged(bool v) { transferredMortgaged_ = v; }
    bool isProperty() const override { return true; }

    void landOn(Player &player, IGameContext &game) override;
    virtual int calculateFee(Player &visitor, IGameContext &game) = 0;
};

// MonopolyBlock - groups 2-3 academic buildings that form a monopoly
export class MonopolyBlock {
    std::string name_;
    std::vector<AcademicBuilding *> buildings_;
public:
    explicit MonopolyBlock(std::string name);

    const std::string &name() const { return name_; }
    void addBuilding(AcademicBuilding *b);
    const std::vector<AcademicBuilding *> &buildings() const { return buildings_; }
    bool isOwnedBy(const Player &player) const;
    bool hasAnyImprovements() const;
};

// AcademicBuilding - ownable, improvements 0-5, tuition lookup table
export class AcademicBuilding : public Property {
    MonopolyBlock *block_;
    int improvementCost_;
    int numImprovements_ = 0;
    std::array<int, 6> tuition_;

public:
    AcademicBuilding(std::string name, int index, int cost,
                     int improvementCost, std::array<int, 6> tuition,
                     MonopolyBlock *block);

    MonopolyBlock *getBlock() const { return block_; }
    int improvementCost() const { return improvementCost_; }
    int improvements() const { return numImprovements_; }
    void setImprovements(int n) { numImprovements_ = n; }
    bool canBuyImprovement() const;
    bool buyImprovement();
    bool sellImprovement();
    int calculateFee(Player &visitor, IGameContext &game) override;
    bool isAcademic() const override { return true; }
};

// Residence - rent scales with number of residences owner holds
export class Residence : public Property {
public:
    Residence(std::string name, int index);
    int calculateFee(Player &visitor, IGameContext &game) override;
};

// Gym - fee = dice sum * multiplier based on gyms owned
export class Gym : public Property {
public:
    Gym(std::string name, int index);
    int calculateFee(Player &visitor, IGameContext &game) override;
};

// non-property squares below

export class CollectOSAP : public Square {
public:
    CollectOSAP(int index);
    void landOn(Player &player, IGameContext &game) override;
};

export class DCTimsLine : public Square {
public:
    DCTimsLine(int index);
    void landOn(Player &player, IGameContext &game) override;
};

export class GoToTims : public Square {
public:
    GoToTims(int index);
    void landOn(Player &player, IGameContext &game) override;
};

export class GooseNesting : public Square {
public:
    GooseNesting(int index);
    void landOn(Player &player, IGameContext &game) override;
};

export class TuitionSquare : public Square {
public:
    TuitionSquare(int index);
    void landOn(Player &player, IGameContext &game) override;
};

export class CoopFee : public Square {
public:
    CoopFee(int index);
    void landOn(Player &player, IGameContext &game) override;
};

export class SLCSquare : public Square {
public:
    SLCSquare(std::string name, int index);
    void landOn(Player &player, IGameContext &game) override;
};

export class NeedlesHallSquare : public Square {
public:
    NeedlesHallSquare(std::string name, int index);
    void landOn(Player &player, IGameContext &game) override;
};
