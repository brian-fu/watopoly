module dice;

import types;
import <random>;

int RandomEventGenerator::rollDie() {
    std::uniform_int_distribution<int> dist{1, 6};
    return dist(engine_);
}

SLCEvent RandomEventGenerator::generateSLCEvent() {
    // probabilities from spec table 2 (lcm = 24):
    // back3=3/24, back2=4/24, back1=4/24, fwd1=3/24, fwd2=4/24, fwd3=4/24, tims=1/24, osap=1/24
    std::uniform_int_distribution<int> dist{0, 23};
    int r = dist(engine_);
    if (r < 3)  return SLCEvent::Back3;
    if (r < 7)  return SLCEvent::Back2;
    if (r < 11) return SLCEvent::Back1;
    if (r < 14) return SLCEvent::Forward1;
    if (r < 18) return SLCEvent::Forward2;
    if (r < 22) return SLCEvent::Forward3;
    if (r < 23) return SLCEvent::GoToTims;
    return SLCEvent::AdvanceToOSAP;
}

int RandomEventGenerator::generateNeedlesHallDelta() {
    // probabilities from spec table 3 (lcm = 18):
    // -200=1/18, -100=2/18, -50=3/18, +25=6/18, +50=3/18, +100=2/18, +200=1/18
    std::uniform_int_distribution<int> dist{0, 17};
    int r = dist(engine_);
    if (r < 1)  return -200;
    if (r < 3)  return -100;
    if (r < 6)  return -50;
    if (r < 12) return 25;
    if (r < 15) return 50;
    if (r < 17) return 100;
    return 200;
}

bool RandomEventGenerator::rollUpTheRimCheck(int activeCupCount) {
    if (activeCupCount >= 4) return false;
    std::uniform_int_distribution<int> dist{0, 99};
    return dist(engine_) == 0;
}

Dice::Dice(RandomEventGenerator &rng, bool testingMode)
    : rng_{rng}, testingMode_{testingMode} {}

DiceResult Dice::roll() {
    DiceResult r;
    r.die1 = rng_.rollDie();
    r.die2 = rng_.rollDie();
    r.sum = r.die1 + r.die2;
    r.isDoubles = (r.die1 == r.die2);
    if (r.isDoubles) ++consecutiveDoubles_;
    else consecutiveDoubles_ = 0;
    return r;
}

DiceResult Dice::roll(int d1, int d2) {
    DiceResult r;
    r.die1 = d1;
    r.die2 = d2;
    r.sum = d1 + d2;
    r.isDoubles = (d1 == d2);
    if (r.isDoubles) ++consecutiveDoubles_;
    else consecutiveDoubles_ = 0;
    return r;
}

void Dice::resetConsecutiveDoubles() { consecutiveDoubles_ = 0; }

int Dice::consecutiveDoubles() const { return consecutiveDoubles_; }
