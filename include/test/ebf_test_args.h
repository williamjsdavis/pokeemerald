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

#define EBF_TEST_ARGS_SCHEMA_VERSION 3

/*
 * Field order is load-bearing — the Python writer reproduces this
 * struct in `BattleArgs.pack()` (little-endian). Don't reorder
 * fields without also bumping the schema version.
 *
 * `player_mons` / `opponent_mons` are u16 indices into
 * `gBattleFrontierMons[]` (0..NUM_FRONTIER_MONS-1 = 0..881).
 *
 * Sentinel triple `(0xFFFF, 0xFFFF, 0xFFFF)` in `opponent_mons`
 * activates Phase 16.2.5 — the test runner calls the cartridge's
 * `GenerateOpponentMons` and uses its pick instead of the patched
 * values. (Phase 16.2.6 will add the analogous sentinel for
 * `player_mons`.)
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
    /*
     * Schema v3 (Phase 16.3): state-transport across per-battle
     * ROM invocations. The cartridge's GenerateOpponentMons /
     * GenerateInitialRentalMons / GetNumPastRentalsRank read these
     * via gSaveBlock2Ptr->frontier.* and VAR_FRONTIER_BATTLE_MODE.
     * Before the v3 transport, every ROM run started with
     * factoryWinStreaks=0 and curChallengeBattleNum=0 so the
     * cartridge always thought we were on the first battle of the
     * first challenge, pinning opp generation to tier band 0
     * (sInitialRentalMonRanges[0] = indices [110, 199] at Lv50).
     * Issue #3 was the bug report; this is the fix.
     *
     * `factory_streak` — the player's accumulated win streak going
     *   into this battle. Sets gSaveBlock2Ptr->frontier.factoryWinStreaks
     *   [battleMode][lvlMode]. Drives challengeNum = streak / 7 in
     *   GenerateOpponentMons and GenerateInitialRentalMons.
     *
     * `factory_rents_count` — accumulated swap count so far. Sets
     *   factoryRentsCount[mode][lvl]. Drives the rentalRank tier
     *   bump in GenerateInitialRentalMons (more rentals → better pool).
     *
     * `cur_challenge_battle_num` — which battle within the current
     *   7-battle challenge (0..6). Sets frontier.curChallengeBattleNum.
     *   Used by GenerateOpponentMons to know which slot in the
     *   `trainerIds` array to write and to scan for "no repeat
     *   trainer in this challenge". At cur_challenge_battle_num=0,
     *   the trainer-repeat check is a no-op (loop body empty).
     *
     * `trainer_ids_so_far[7]` — the trainer IDs already encountered
     *   in this challenge. Slots [0..cur_challenge_battle_num-1] are
     *   filled with previous battles' opp trainers; remaining slots
     *   are 0xFFFF (the cartridge's "unused" sentinel from
     *   InitFactoryChallenge at line 215-216 of battle_factory.c).
     *   Used as the no-repeat exclusion set during opp gen.
     *
     * `battle_mode` — FRONTIER_MODE_SINGLES (0) or _DOUBLES (1).
     *   Maps to VAR_FRONTIER_BATTLE_MODE. v1 only exercises singles,
     *   but the cartridge code paths read this so we expose it for
     *   future expansion.
     *
     * `lvl_mode` — FRONTIER_LVL_50 (0) or FRONTIER_LVL_OPEN (1).
     *   Maps to gSaveBlock2Ptr->frontier.lvlMode. Default 0.
     */
    u16 factory_streak;
    u16 factory_rents_count;
    u16 cur_challenge_battle_num;
    u16 trainer_ids_so_far[7];
    u8  battle_mode;
    u8  lvl_mode;
    u8  _pad_v3[2];  /* round struct size up to 52 (4-byte multiple) so
                      * Python pack length matches C sizeof exactly. */
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
