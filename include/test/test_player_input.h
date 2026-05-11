#ifndef GUARD_TEST_PLAYER_INPUT_H
#define GUARD_TEST_PLAYER_INPUT_H

/*
 * test/test_player_input.h
 *
 * Phase 1.3 layer 2 — the input block the test-mode player controller
 * reads to decide actions. In headless test builds, the default
 * `battle_controller_player.c` blocks on keypad input that mGBA can't
 * provide; under `#if TESTING` we override its two decision functions
 * (`HandleInputChooseAction`, `HandleInputChooseMove`) to instead read
 * the choice from this struct.
 *
 * The struct lives in EWRAM at a stable, symbol-addressable location so
 * that the Python-side harness can (Layer 3) patch new values into the
 * ELF before each run via the expansion's `patchelf`-equivalent mechanism.
 * See `docs/11_phase-1.3-roadmap.md` and `docs/10_patchelf-and-mgba-protocol.md`.
 *
 * For Layer 2 (this commit) the struct is populated with hard-coded values
 * so the test ROM can drive one canonical Battle Factory matchup to
 * completion. Layer 3 will introduce dynamic patching.
 *
 * Schema versioning: any change to this struct's layout must bump
 * `TEST_PLAYER_INPUT_SCHEMA_VERSION`. The Python-side `patchelf` writer
 * will read that constant via its own symbol and refuse to write if the
 * version doesn't match what it was compiled for, catching skew early.
 */

#include "global.h"

#define TEST_PLAYER_INPUT_SCHEMA_VERSION 1

/*
 * One entry per agent decision. `kind` is one of `B_ACTION_USE_MOVE`,
 * `B_ACTION_SWITCH`, etc. (see `include/battle.h`). `move_index` is
 * 0..3 when `kind == B_ACTION_USE_MOVE`. `switch_target` is 0..5 (a
 * party slot) when `kind == B_ACTION_SWITCH`. Other action kinds are
 * not modelled by this struct yet (no items / no run for Battle Factory).
 */
struct TestPlayerInputTurn
{
    u8 kind;
    u8 move_index;
    u8 switch_target;
    u8 _pad;
};

/*
 * A buffer of pre-scripted turn actions. The test player controller
 * consumes one entry per decision via `cursor`. If `cursor` reaches
 * `turn_count` the controller falls back to "use move 0" so a runaway
 * battle still terminates rather than hangs.
 *
 * 32 entries is enough for any realistic Battle Factory battle (typical
 * is 3-8 turns; 32 is the documented Gen-3 Frontier turn cap).
 */
#define TEST_PLAYER_INPUT_MAX_TURNS 32

struct TestPlayerInput
{
    u32 schema_version;                                   /* must equal TEST_PLAYER_INPUT_SCHEMA_VERSION */
    u32 turn_count;                                       /* number of valid entries in `turns` */
    u32 cursor;                                           /* index of next entry to consume */
    struct TestPlayerInputTurn turns[TEST_PLAYER_INPUT_MAX_TURNS];
};

extern struct TestPlayerInput gTestPlayerInput;

/*
 * Read-and-advance the next scripted action. Returns a pointer to the
 * entry the controller should use. If the buffer is exhausted, returns
 * a static fallback (`B_ACTION_USE_MOVE`, move_index 0) so the battle
 * still progresses.
 *
 * Caller must only invoke under `#if TESTING`.
 */
const struct TestPlayerInputTurn *TestPlayerInput_Next(void);

#endif /* GUARD_TEST_PLAYER_INPUT_H */
