export module board;

import types;
import core;
import <vector>;
import <string>;
import <memory>;

// Board - owns the 40 squares and 10 monopoly blocks
export class Board {
    std::vector<std::unique_ptr<Square>> squares_;
    std::vector<MonopolyBlock> blocks_;

public:
    Board();

    Square &getSquare(int pos);
    const Square &getSquare(int pos) const;
    int positionOf(const std::string &name) const;
    int numSquares() const { return 40; }

    // access blocks for buildDefaultBoard and display
    std::vector<MonopolyBlock> &monopolyBlocks() { return blocks_; }
    const std::vector<MonopolyBlock> &monopolyBlocks() const { return blocks_; }

    // square storage for board construction
    void setSquare(int pos, std::unique_ptr<Square> sq);
    void addBlock(MonopolyBlock block);

    // find property by name (returns null if not found or not a property)
    Property *findProperty(const std::string &name);
};

// builds all 40 squares and 10 monopoly blocks with correct parameters
export void buildDefaultBoard(Board &board);
