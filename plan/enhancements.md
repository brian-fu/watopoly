# Watopoly Enhancement Ideas

This document proposes optional enhancements that are realistic for CS246 project scope, easy to demo, and compatible with the existing `plan.md` architecture (`IGameContext`, `Square` polymorphism, `Command` hierarchy, observer-based display updates).

---

## Design Goals for Enhancements

- Keep core gameplay stable and always runnable without enhancements.
- Enable each enhancement through command-line flags (or runtime commands) so features can be toggled during demos.
- Prefer additive features that reuse existing abstractions (new `Command`, new `Square`, new display observer, small game-rule policies).
- Avoid enhancements that require invasive rewrites late in the schedule.

---

## Recommended Enhancements

## 1) Action Log + Replay Mode

**What it adds**
- A chronological event log of the game: rolls, movement, purchases, rent payments, auctions, bankruptcy, and special square effects.
- Optional replay mode that replays a finished game from the log at human-readable speed.

**Why it is strong**
- Very demo-friendly: clearly shows correctness of state transitions.
- Helps debugging and post-game analysis.

**Implementation sketch**
- Add `GameEvent` structs and `EventLogger` service.
- Emit events from key game operations (or through centralized `IGameContext` methods).
- Add commands:
  - `history [n]` to print latest events
  - `replay <file>` to replay a saved event stream

**Toggle**
- CLI: `-enable-log` and `-enable-replay`

---

## 2) Rich Testing/Debug Commands (Controlled Dev Mode)

**What it adds**
- Additional deterministic testing commands beyond required `roll <d1> <d2>`, for example:
  - `teleport <player> <square>`
  - `givecash <player> <amount>`
  - `setowner <property> <player|BANK>`
  - `setimprove <property> <0..5>`

**Why it is strong**
- Huge productivity boost for validating edge cases quickly.
- Excellent for TA demos to jump directly to complex scenarios (debt, mortgage transfer, auction, Tims Line turns).

**Implementation sketch**
- New `Command` subclasses gated by a `DebugMode` flag in game context.
- Reuse existing validation logic instead of bypassing invariants where possible.

**Toggle**
- CLI: `-debug`
- Commands only registered when debug mode is enabled.

---

## 3) Alternate Board Displays (Observer Extension)

**What it adds**
- Multiple output styles for the same game state:
  - Compact board (single-line summaries)
  - Detailed board (current style with improvements and tokens)
  - Player-centric dashboard (`cash`, `properties`, `mortgages`, `cups`, current debt state)

**Why it is strong**
- Directly demonstrates separation of concerns and Observer extensibility.
- Makes the game easier to use and grade.

**Implementation sketch**
- Keep `DisplayObserver` interface.
- Add additional observer classes, e.g., `CompactDisplay`, `StatusDisplay`.
- Add command: `display <compact|detailed|status|all>`.

**Toggle**
- CLI: `-enable-multidisplay` (or always available as a quality-of-life feature).

---

## 4) Statistics & Fairness Report

**What it adds**
- Runtime and end-game metrics:
  - Landing frequency per square
  - Net gains/losses by square type
  - Number of doubles, auctions, mortgages, bankruptcies
  - Cup acquisition and usage rates

**Why it is strong**
- Easy to justify as analytics on game dynamics.
- Simple incremental implementation with high demo value.

**Implementation sketch**
- `StatsCollector` increments counters on state transitions.
- Add command: `stats [summary|square|player]`.
- Optional export to markdown or csv for analysis.

**Toggle**
- CLI: `-enable-stats`

---

## 5) Rule Packs (Configurable Variants)

**What it adds**
- Named rule presets loaded at startup, for example:
  - `classic`: strict spec defaults
  - `fast`: higher OSAP, lower tuition, quicker games
  - `hardcore`: stronger penalties, less starting cash

**Why it is strong**
- Demonstrates resilience-to-change and clean policy encapsulation.
- Lets you show that rule changes do not require rewiring core classes.

**Implementation sketch**
- Introduce a `RuleConfig` struct loaded from file or preset table.
- Route fees/thresholds through config values:
  - OSAP amount
  - Coop fee
  - Tims line exit cost
  - Starting cash
- Keep base spec as default config.

**Toggle**
- CLI: `-rules <classic|fast|hardcore|file.json>`

---

## 6) Card-Deck Model for SLC / Needles Hall

**What it adds**
- A card-style implementation (draw without replacement, reshuffle when empty) to mirror Monopoly-like behavior.

**Why it is strong**
- Cleanly answers the project question about patterns for SLC/Needles Hall.
- Great design-discussion bonus: Command/Strategy-like effect objects + deck abstraction.

**Implementation sketch**
- Add `Card` interface with `execute(Player&, IGameContext&)`.
- `CardDeck` manages draw order and reshuffling.
- `SLCSquare` and `NeedlesHallSquare` delegate effects to decks.

**Toggle**
- CLI: `-enable-carddeck-events`
- Default remains baseline probability implementation unless this flag is set.

---

## Recommended Priority (Best Value / Effort)

1. **Rich Testing/Debug Commands** (high impact, low effort)
2. **Statistics & Fairness Report** (high impact, low effort)
3. **Action Log + Replay Mode** (high demo value, medium effort)
4. **Alternate Displays** (medium effort, strong architecture story)
5. **Rule Packs** (medium effort, strong resilience story)
6. **Card-Deck Event Model** (higher effort, strongest design-pattern story)

---

## Suggested Deliverable Wording

If you include this in your DD2 design doc, you can position enhancements as:

- "Implemented" (done and demoed),
- "Partially implemented" (core mechanism complete, limited UI),
- "Planned extension" (architecture prepared, deferred due to schedule).

This framing helps show intentional design decisions even if not all enhancements are completed.

