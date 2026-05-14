/*
 * test/test_player_input.c
 *
 * Phase 1.3 layer 2 — definition of the test-mode player input block.
 * Phase 1.3 layer 5 update: the struct now lives in `.rodata` (via
 * `const`) so the Python harness can patch it with patchelf before
 * each run. The cursor (which must be mutable across turns) is
 * factored out into a private file-scope variable.
 *
 * Default initializer encodes "use move 0 every turn for up to
 * TEST_PLAYER_INPUT_MAX_TURNS turns" — the same behaviour the
 * pre-Layer-5 EWRAM-default-fallback produced. An unpatched test
 * ROM is therefore unchanged from the Layer 4 first-light state.
 */

#include "global.h"
#include "battle.h"  /* B_ACTION_USE_MOVE etc. */
#include "test/test_player_input.h"

/*
 * Static "kind=USE_MOVE, move_index=0" entry, repeated. Python
 * overwrites this whole block via patchelf when an agent wants to
 * script real per-turn actions.
 *
 * NOTE: we cannot use `B_ACTION_USE_MOVE` as a designated initialiser
 * here because the compound literal would need to match the macro at
 * preprocess time; the static_assert below catches drift instead.
 */
#define TPI_USE_MOVE_0 \
    { .kind = 0 /* B_ACTION_USE_MOVE */, .move_index = 0, \
      .switch_target = 0, ._pad = 0 }

const struct TestPlayerInput gTestPlayerInput =
{
    .schema_version = TEST_PLAYER_INPUT_SCHEMA_VERSION,
    .turn_count = TEST_PLAYER_INPUT_MAX_TURNS,
    ._reserved_cursor = 0,
    .turns =
    {
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
        TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0, TPI_USE_MOVE_0,
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

/*
 * Mutable cursor, separate from the patchable struct. BSS-zero at
 * boot means we always start from turn 0; no patcher needs to write
 * it. Move-in / switch-in scripts can advance it independently from
 * the action / item scripts via the same `TestPlayerInput_Advance`
 * entry point that already exists.
 */
static u32 sCursor;

const struct TestPlayerInputTurn *TestPlayerInput_Peek(void)
{
    if (gTestPlayerInput.schema_version != TEST_PLAYER_INPUT_SCHEMA_VERSION)
        return &sFallbackTurn;

    if (sCursor >= gTestPlayerInput.turn_count
        || sCursor >= TEST_PLAYER_INPUT_MAX_TURNS)
        return &sFallbackTurn;

    return &gTestPlayerInput.turns[sCursor];
}

void TestPlayerInput_Advance(void)
{
    if (gTestPlayerInput.schema_version != TEST_PLAYER_INPUT_SCHEMA_VERSION)
        return;
    if (sCursor >= TEST_PLAYER_INPUT_MAX_TURNS)
        return;
    sCursor++;
}
