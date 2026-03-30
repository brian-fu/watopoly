module display;

import types;
import core;
import board;
import <vector>;
import <string>;
import <iostream>;
import <algorithm>;
import <sstream>;

// DisplayHub
void DisplayHub::attach(DisplayObserver *obs) { observers_.push_back(obs); }

void DisplayHub::detach(DisplayObserver *obs) {
    auto it = std::find(observers_.begin(), observers_.end(), obs);
    if (it != observers_.end()) observers_.erase(it);
}

void DisplayHub::publish(const GameSnapshot &snap) {
    for (auto *obs : observers_) obs->update(snap);
}

// EventLogger
void EventLogger::log(const std::string &msg) {
    if (enabled_) events_.push_back(msg);
}

void EventLogger::printHistory(int n) const {
    int start = 0;
    if (n > 0 && n < static_cast<int>(events_.size())) {
        start = static_cast<int>(events_.size()) - n;
    }
    for (int i = start; i < static_cast<int>(events_.size()); ++i) {
        std::cout << "[" << (i + 1) << "] " << events_[i] << "\n";
    }
    if (events_.empty()) std::cout << "no events logged yet.\n";
}

// BoardDisplay helpers
std::string BoardDisplay::pad(const std::string &s, int width) const {
    if (static_cast<int>(s.size()) >= width) return s.substr(0, width);
    return s + std::string(width - static_cast<int>(s.size()), ' ');
}

std::string BoardDisplay::renderImprovements(int n) const {
    std::string s;
    for (int i = 0; i < n; ++i) s += 'I';
    return s;
}

std::string BoardDisplay::getPlayersOnSquare(const GameSnapshot &snap, int sq) const {
    std::string tokens;
    for (auto &p : snap.players) {
        if (!p.bankrupt && p.position == sq) tokens += p.token;
    }
    return tokens;
}

// main board rendering
// the board layout is:
//   top row:    squares 20..30 (left to right)
//   left col:   squares 19..11 (top to bottom)
//   right col:  squares 31..39 (top to bottom)
//   bottom row: squares 10..0  (left to right)
// each cell is 7 chars wide, 4 content lines
void BoardDisplay::update(const GameSnapshot &snap) {
    const int W = 7; // cell content width
    const int COLS = 11;

    // display name mappings for non-property/special squares
    auto displayName = [](int sq, const std::string &name) -> std::pair<std::string, std::string> {
        // return two display lines for each square
        if (name == "Collect OSAP")  return {"COLLECT", "OSAP"};
        if (name == "DC Tims Line")  return {"DC Tims", "Line"};
        if (name == "Go to Tims")    return {"GO TO", "TIMS"};
        if (name == "Goose Nesting") return {"Goose", "Nesting"};
        if (name == "Needles Hall")  return {"NEEDLES", "HALL"};
        if (name == "Coop Fee")      return {"COOP", "FEE"};
        if (name == "Tuition")       return {"TUITION", ""};
        if (name == "SLC")           return {"SLC", ""};
        (void)sq;
        return {name, ""};
    };

    // render one cell as 4 lines of W chars
    auto renderCell = [&](int sq) -> std::vector<std::string> {
        std::vector<std::string> lines(4);
        auto &si = snap.squares[sq];
        std::string tokens = getPlayersOnSquare(snap, sq);

        if (si.isProperty) {
            // line 0: improvement markers
            std::string impr = renderImprovements(si.improvements);
            if (si.mortgaged) impr = "M";
            lines[0] = pad(impr, W);
            // line 1: separator
            lines[1] = std::string(W, '-');
            // line 2: name
            lines[2] = pad(si.name, W);
            // line 3: player tokens
            lines[3] = pad(tokens, W);
        } else {
            auto [l1, l2] = displayName(sq, si.name);
            lines[0] = pad(l1, W);
            lines[1] = pad(l2, W);
            lines[2] = pad("", W);
            lines[3] = pad(tokens, W);
        }
        return lines;
    };

    auto border = [&]() -> std::string {
        std::string s = "|";
        for (int c = 0; c < COLS; ++c) {
            s += std::string(W, '_') + "|";
        }
        return s;
    };

    auto topBorder = [&]() -> std::string {
        std::string s = "_";
        for (int c = 0; c < COLS; ++c) {
            s += std::string(W, '_') + "_";
        }
        return s;
    };

    // top row: squares 20..30
    int topSquares[11] = {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
    std::vector<std::vector<std::string>> topCells;
    for (int i = 0; i < 11; ++i) topCells.push_back(renderCell(topSquares[i]));
    std::cout << topBorder() << "\n";
    for (int line = 0; line < 4; ++line) {
        std::string row = "|";
        for (int c = 0; c < 11; ++c) {
            row += topCells[c][line] + "|";
        }
        std::cout << row << "\n";
    }
    std::cout << border() << "\n";

    // side rows: left = 19..11 (top to bottom), right = 31..39 (top to bottom)
    int leftSquares[9]  = {19, 18, 17, 16, 15, 14, 13, 12, 11};
    int rightSquares[9] = {31, 32, 33, 34, 35, 36, 37, 38, 39};

    // the middle gap width between left and right columns
    int midWidth = (COLS - 2) * (W + 1) - 1;

    // simple watopoly banner lines (placed roughly in the middle rows)
    // Must be exactly 4 lines — each side row has 4 content slots; >4 lines
    // causes the banner to straddle a row border (blank line) creating a gap.
    std::vector<std::string> bannerLines = {
        "#   #  ###  #####  ###  ####   ###  #    #   #",
        "# # # #   #   #   #   # #   # #   # #     # # ",
        "## ## #####   #   #   # ####  #   # #      #  ",
        "#   # #   #   #    ###  #      ###  #####  #   ",
    };
    int bannerStart = 3; // which side row to start the banner at

    for (int r = 0; r < 9; ++r) {
        auto leftCell = renderCell(leftSquares[r]);
        auto rightCell = renderCell(rightSquares[r]);

        for (int line = 0; line < 4; ++line) {
            std::string row = "|" + leftCell[line] + "|";

            // fill middle area
            int bannerLine = (r - bannerStart) * 4 + line;
            if (bannerLine >= 0 && bannerLine < static_cast<int>(bannerLines.size())) {
                std::string bl = bannerLines[bannerLine];
                int leftPad = (midWidth - static_cast<int>(bl.size())) / 2;
                if (leftPad < 0) leftPad = 0;
                int rightPad = midWidth - leftPad - static_cast<int>(bl.size());
                if (rightPad < 0) rightPad = 0;
                row += std::string(leftPad, ' ') + bl + std::string(rightPad, ' ');
            } else {
                row += std::string(midWidth, ' ');
            }

            row += "|" + rightCell[line] + "|";
            std::cout << row << "\n";
        }

        // side row border
        // For the final side row (right above the bottom row), use the same
        // full-width separator style, but without interior '|' separators in
        // the middle area (prevents vertical ticks sticking up there).
        if (r == 8) {
            std::string bottomTransition = "|" + std::string(W, '_') + "|";
            bottomTransition += std::string(midWidth, '_');
            bottomTransition += "|" + std::string(W, '_') + "|";
            std::cout << bottomTransition << "\n";
        } else {
            std::string sideBorder = "|" + std::string(W, '_') + "|";
            sideBorder += std::string(midWidth, ' ');
            sideBorder += "|" + std::string(W, '_') + "|";
            std::cout << sideBorder << "\n";
        }
    }

    // bottom row: squares 10..0 (left to right)
    int bottomSquares[11] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    std::vector<std::vector<std::string>> bottomCells;
    for (int i = 0; i < 11; ++i) bottomCells.push_back(renderCell(bottomSquares[i]));

    for (int line = 0; line < 4; ++line) {
        std::string row = "|";
        for (int c = 0; c < 11; ++c) {
            row += bottomCells[c][line] + "|";
        }
        std::cout << row << "\n";
    }
    std::cout << border() << "\n";
    std::cout << std::endl;
}
