module board;

import types;
import core;
import <vector>;
import <string>;
import <memory>;
import <algorithm>;

Board::Board() : squares_(40) {}

Square &Board::getSquare(int pos) { return *squares_[pos]; }
const Square &Board::getSquare(int pos) const { return *squares_[pos]; }

int Board::positionOf(const std::string &name) const {
    for (int i = 0; i < 40; ++i) {
        if (squares_[i] && squares_[i]->name() == name) return i;
    }
    return -1;
}

void Board::setSquare(int pos, std::unique_ptr<Square> sq) {
    squares_[pos] = std::move(sq);
}

void Board::addBlock(MonopolyBlock block) {
    blocks_.push_back(std::move(block));
}

Property *Board::findProperty(const std::string &name) {
    for (int i = 0; i < 40; ++i) {
        if (squares_[i] && squares_[i]->name() == name && squares_[i]->isProperty()) {
            return static_cast<Property *>(squares_[i].get());
        }
    }
    return nullptr;
}

// builds all 40 squares and 10 monopoly blocks per the spec
void buildDefaultBoard(Board &board) {
    // create monopoly blocks first so buildings can reference them
    board.addBlock(MonopolyBlock{"Arts1"});
    board.addBlock(MonopolyBlock{"Arts2"});
    board.addBlock(MonopolyBlock{"Eng"});
    board.addBlock(MonopolyBlock{"Health"});
    board.addBlock(MonopolyBlock{"Env"});
    board.addBlock(MonopolyBlock{"Sci1"});
    board.addBlock(MonopolyBlock{"Sci2"});
    board.addBlock(MonopolyBlock{"Math"});
    // index 0-7 above; we'll access by reference

    auto &blocks = board.monopolyBlocks();
    MonopolyBlock *arts1   = &blocks[0];
    MonopolyBlock *arts2   = &blocks[1];
    MonopolyBlock *eng     = &blocks[2];
    MonopolyBlock *health  = &blocks[3];
    MonopolyBlock *env     = &blocks[4];
    MonopolyBlock *sci1    = &blocks[5];
    MonopolyBlock *sci2    = &blocks[6];
    MonopolyBlock *math    = &blocks[7];

    // helper to create an academic building and register it with its block
    auto makeAcad = [&](std::string name, int idx, int cost, int impCost,
                        std::vector<int> tuition, MonopolyBlock *blk) {
        auto sq = std::make_unique<AcademicBuilding>(
            std::move(name), idx, cost, impCost, std::move(tuition), blk);
        blk->addBuilding(sq.get());
        board.setSquare(idx, std::move(sq));
    };

    // square 0: Collect OSAP
    board.setSquare(0, std::make_unique<CollectOSAP>(0));

    // square 1: AL (Arts1)
    makeAcad("AL", 1, 40, 50, {2, 10, 30, 90, 160, 250}, arts1);

    // square 2: SLC
    board.setSquare(2, std::make_unique<SLCSquare>("SLC", 2));

    // square 3: ML (Arts1)
    makeAcad("ML", 3, 60, 50, {4, 20, 60, 180, 320, 450}, arts1);

    // square 4: Tuition
    board.setSquare(4, std::make_unique<TuitionSquare>(4));

    // square 5: MKV (Residence)
    board.setSquare(5, std::make_unique<Residence>("MKV", 5));

    // square 6: ECH (Arts2)
    makeAcad("ECH", 6, 100, 50, {6, 30, 90, 270, 400, 550}, arts2);

    // square 7: Needles Hall
    board.setSquare(7, std::make_unique<NeedlesHallSquare>("Needles Hall", 7));

    // square 8: PAS (Arts2)
    makeAcad("PAS", 8, 100, 50, {6, 30, 90, 270, 400, 550}, arts2);

    // square 9: HH (Arts2)
    makeAcad("HH", 9, 120, 50, {8, 40, 100, 300, 450, 600}, arts2);

    // square 10: DC Tims Line
    board.setSquare(10, std::make_unique<DCTimsLine>(10));

    // square 11: RCH (Eng)
    makeAcad("RCH", 11, 140, 100, {10, 50, 150, 450, 625, 750}, eng);

    // square 12: PAC (Gym)
    board.setSquare(12, std::make_unique<Gym>("PAC", 12));

    // square 13: DWE (Eng)
    makeAcad("DWE", 13, 140, 100, {10, 50, 150, 450, 625, 750}, eng);

    // square 14: CPH (Eng)
    makeAcad("CPH", 14, 160, 100, {12, 60, 180, 500, 700, 900}, eng);

    // square 15: UWP (Residence)
    board.setSquare(15, std::make_unique<Residence>("UWP", 15));

    // square 16: LHI (Health)
    makeAcad("LHI", 16, 180, 100, {14, 70, 200, 550, 750, 950}, health);

    // square 17: SLC
    board.setSquare(17, std::make_unique<SLCSquare>("SLC", 17));

    // square 18: BMH (Health)
    makeAcad("BMH", 18, 180, 100, {14, 70, 200, 550, 750, 950}, health);

    // square 19: OPT (Health)
    makeAcad("OPT", 19, 200, 100, {16, 80, 220, 600, 800, 1000}, health);

    // square 20: Goose Nesting
    board.setSquare(20, std::make_unique<GooseNesting>(20));

    // square 21: EV1 (Env)
    makeAcad("EV1", 21, 220, 150, {18, 90, 250, 700, 875, 1050}, env);

    // square 22: Needles Hall
    board.setSquare(22, std::make_unique<NeedlesHallSquare>("Needles Hall", 22));

    // square 23: EV2 (Env)
    makeAcad("EV2", 23, 220, 150, {18, 90, 250, 700, 875, 1050}, env);

    // square 24: EV3 (Env)
    makeAcad("EV3", 24, 240, 150, {20, 100, 300, 750, 925, 1100}, env);

    // square 25: V1 (Residence)
    board.setSquare(25, std::make_unique<Residence>("V1", 25));

    // square 26: PHYS (Sci1)
    makeAcad("PHYS", 26, 260, 150, {22, 110, 330, 800, 975, 1150}, sci1);

    // square 27: B1 (Sci1)
    makeAcad("B1", 27, 260, 150, {22, 110, 330, 800, 975, 1150}, sci1);

    // square 28: CIF (Gym)
    board.setSquare(28, std::make_unique<Gym>("CIF", 28));

    // square 29: B2 (Sci1)
    makeAcad("B2", 29, 280, 150, {24, 120, 360, 850, 1025, 1200}, sci1);

    // square 30: Go to Tims
    board.setSquare(30, std::make_unique<GoToTims>(30));

    // square 31: EIT (Sci2)
    makeAcad("EIT", 31, 300, 200, {26, 130, 390, 900, 1100, 1275}, sci2);

    // square 32: ESC (Sci2)
    makeAcad("ESC", 32, 300, 200, {26, 130, 390, 900, 1100, 1275}, sci2);

    // square 33: Needles Hall
    board.setSquare(33, std::make_unique<NeedlesHallSquare>("Needles Hall", 33));

    // square 34: C2 (Sci2)
    makeAcad("C2", 34, 320, 200, {28, 150, 450, 1000, 1200, 1400}, sci2);

    // square 35: REV (Residence)
    board.setSquare(35, std::make_unique<Residence>("REV", 35));

    // square 36: Needles Hall
    board.setSquare(36, std::make_unique<NeedlesHallSquare>("Needles Hall", 36));

    // square 37: MC (Math)
    makeAcad("MC", 37, 350, 200, {35, 175, 500, 1100, 1300, 1500}, math);

    // square 38: Coop Fee
    board.setSquare(38, std::make_unique<CoopFee>(38));

    // square 39: DC (Math)
    makeAcad("DC", 39, 400, 200, {50, 200, 600, 1400, 1700, 2000}, math);
}
