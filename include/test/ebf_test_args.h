#ifndef GUARD_EBF_TEST_ARGS_H
#define GUARD_EBF_TEST_ARGS_H

/*
 * test/ebf_test_args.h
 *
 * Phase 1.3 layer 3 — the per-run argument block. SetupFirstLightBattle_
 * reads team / RNG / opponent-trainer values from this struct instead of
 * hardcoding them, so the Python harness can drive a different battle on
 * each invocation by patching the bytes via patchelf-equivalent.
 *
 * Like `gTestPlayerInput`, this struct lives at a stable, symbol-
 * addressable location and carries a schema version. The Python writer
 * locates the symbol in the ELF, writes new bytes at its file offset,
 * and `objcopy -O binary` produces a patched .gba. mgba-rom-test then
 * runs the .gba normally — the patched values are baked into the .data
 * section that the BIOS copies into EWRAM at boot, so the ROM sees the
 * patched values from its first cycle.
 *
 * Default values (set in `ebf_test_args.c`) reproduce the Layer-2
 * first-light Battle Factory matchup so an unpatched test ROM still
 * runs an identical battle to the pre-Layer-3 state. This is the
 * "patchelf is optional" invariant — building and running the test ROM
 * directly never requires the Python harness.
 *
 * Schema versioning: any change to this struct's layout must bump
 * `EBF_TEST_ARGS_SCHEMA_VERSION`. The Python writer reads the version
 * via its own symbol and refuses to patch on mismatch, catching skew.
 */

#include "global.h"

#define EBF_TEST_ARGS_SCHEMA_VERSION 2

/*
 * Field order is load-bearing — the Python writer reproduces this
 * struct as `struct.Struct("<II3H3HH2x")` (little-endian). Don't
 * reorder fields without also bumping the schema version.
 *
 * `player_mons` / `opponent_mons` are u16 indices into
 * `gBattleFrontierMons[]` (0..NUM_FRONTIER_MONS-1 = 0..881). The
 * Layer-2 first-light battle used indices 700/730/800 (player) and
 * 701/731/801 (opponent), all high-tier non-OHKO rentals.
 *
 * `trainer_id` selects the opponent's trainer-class graphics / name;
 * doesn't affect AI behaviour. Default `TRAINER_NONE` (0).
 */
struct EbfTestArgs
{
    u32 schema_version;
    u32 rng_seed_1;
    u32 rng_seed_2;
    u16 player_mons[3];
    u16 opponent_mons[3];
    u16 trainer_id;
    /*
     * Schema v2: in-ROM turn-counter watchdog. When non-zero, the
     * test harness checks gBattleResults.battleTurnCounter each
     * frame in BattleMainCB1 and exits via Mgba_LogExit when it
     * exceeds this threshold (emitting BATTLE_OUTCOME=99 +
     * WATCHDOG_HIT=<count> sentinels). Zero = no in-ROM limit;
     * Python's subprocess timeout is the only backstop. Either,
     * both, or neither limit can be set — whichever fires first.
     */
    u16 max_in_rom_turns;
};

/*
 * Declared `const` so it lands in `.rodata` (ROM) — this matches the
 * test linker script's `test/*.o(.rodata*)` pattern, and avoids the
 * complexity of EWRAM initial-value patching (where the load address
 * in ROM and the virtual address in EWRAM differ). Engine code reads
 * the values; nothing writes them at runtime. patchelf writes raw
 * bytes regardless of the C-level const-ness.
 */
extern const struct EbfTestArgs gEbfTestArgs;

#endif /* GUARD_EBF_TEST_ARGS_H */
