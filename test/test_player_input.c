/*
 * test/test_player_input.c
 *
 * Phase 1.3 layer 2 — definition of the test-mode player input block.
 * The struct is allocated in EWRAM so its symbol resolves to a fixed
 * file offset in the ELF, which the Python-side `patchelf` writer will
 * target at Layer 3 to script per-run player decisions.
 *
 * For Layer 2 (this commit) the buffer is initialised to a single
 * trivial action — "use move 0" — and the test runner exercises just
 * the wiring. Layer 3 starts patching the buffer.
 */

#include "global.h"
#include "battle.h"  /* B_ACTION_USE_MOVE etc. */
#include "test/test_player_input.h"

/*
 * Initialiser: schema version stamped, cursor at 0, one turn pre-loaded
 * with `B_ACTION_USE_MOVE` move 0. The rest of the turns array is
 * zeroed by EWRAM init.
 *
 * NOTE: we cannot use `B_ACTION_USE_MOVE` as a designated initialiser
 * here because the surrounding `EWRAM_DATA` macro expects a literal
 * initialiser list; instead we use the numeric value (0) and rely on
 * the static-assert below to catch any drift.
 */
EWRAM_DATA struct TestPlayerInput gTestPlayerInput =
{
    .schema_version = TEST_PLAYER_INPUT_SCHEMA_VERSION,
    .turn_count = 1,
    .cursor = 0,
    .turns =
    {
        { .kind = 0 /* B_ACTION_USE_MOVE */, .move_index = 0, .switch_target = 0, ._pad = 0 },
    },
};

_Static_assert(B_ACTION_USE_MOVE == 0,
    "TestPlayerInput initialiser assumes B_ACTION_USE_MOVE == 0");

static const struct TestPlayerInputTurn sFallbackTurn =
{
    .kind = 0 /* B_ACTION_USE_MOVE */,
    .move_index = 0,
    .switch_target = 0,
    ._pad = 0,
};

const struct TestPlayerInputTurn *TestPlayerInput_Next(void)
{
    /* Defensive: if the schema version stamped in the struct doesn't
     * match what we were compiled with, something patchelf-wrote into
     * the ELF is stale. Fall back to the safe default rather than
     * silently using a structurally invalid entry. */
    if (gTestPlayerInput.schema_version != TEST_PLAYER_INPUT_SCHEMA_VERSION)
        return &sFallbackTurn;

    if (gTestPlayerInput.cursor >= gTestPlayerInput.turn_count
        || gTestPlayerInput.cursor >= TEST_PLAYER_INPUT_MAX_TURNS)
        return &sFallbackTurn;

    return &gTestPlayerInput.turns[gTestPlayerInput.cursor++];
}
