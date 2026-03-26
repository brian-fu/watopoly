export module dice;

import types;
import <random>;

// centralized rng for dice, slc, needles hall, and cup checks
export class RandomEventGenerator {
    std::default_random_engine engine_;
public:
    explicit RandomEventGenerator(unsigned seed = 0) { engine_.seed(seed); }
    void seed(unsigned value) { engine_.seed(value); }
    int rollDie();
    SLCEvent generateSLCEvent();
    int generateNeedlesHallDelta();
    bool rollUpTheRimCheck(int activeCupCount);
};

// wraps rng for player movement rolls and tracks consecutive doubles
export class Dice {
    RandomEventGenerator &rng_;
    int consecutiveDoubles_ = 0;
    bool testingMode_;
public:
    Dice(RandomEventGenerator &rng, bool testingMode);
    DiceResult roll();
    DiceResult roll(int d1, int d2);
    void resetConsecutiveDoubles();
    int consecutiveDoubles() const;
};
