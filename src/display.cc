export module display;

import types;
import core;
import board;
import <vector>;
import <string>;
import <memory>;
import <iostream>;

// snapshot structs for observer push model (decouples display from game)
export struct SquareInfo {
    std::string name;
    bool isProperty = false;
    bool isAcademic = false;
    std::string ownerName;
    int improvements = 0;
    bool mortgaged = false;
};

export struct PlayerInfo {
    std::string name;
    char token = '?';
    int cash = 0;
    int position = 0;
    int timsCups = 0;
    bool inTimsLine = false;
    bool bankrupt = false;
};

export struct GameSnapshot {
    SquareInfo squares[40];
    std::vector<PlayerInfo> players;
};

// DisplayObserver - abstract observer for display updates
export class DisplayObserver {
public:
    virtual ~DisplayObserver() = default;
    virtual void update(const GameSnapshot &snap) = 0;
};

// DisplayHub - manages observers
export class DisplayHub {
    std::vector<DisplayObserver *> observers_;
public:
    void attach(DisplayObserver *obs);
    void detach(DisplayObserver *obs);
    void publish(const GameSnapshot &snap);
};

// BoardDisplay - text-based board renderer
export class BoardDisplay : public DisplayObserver {
public:
    void update(const GameSnapshot &snap) override;

private:
    // helpers for rendering the board grid
    std::string pad(const std::string &s, int width) const;
    std::string renderImprovements(int n) const;
    std::string getPlayersOnSquare(const GameSnapshot &snap, int sq) const;
};

// EventLogger - action log enhancement
export class EventLogger {
    std::vector<std::string> events_;
    bool enabled_ = false;
public:
    void setEnabled(bool on) { enabled_ = on; }
    bool isEnabled() const { return enabled_; }
    void log(const std::string &msg);
    void printHistory(int n = -1) const;
    const std::vector<std::string> &events() const { return events_; }
    void clear() { events_.clear(); }
};
