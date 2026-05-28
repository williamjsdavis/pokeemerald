/*
 * test/ebf_test_args.c
 *
 * Storage for the patchelf-addressable test argument block. See
 * `include/test/ebf_test_args.h` for the schema and rationale.
 *
 * The initial values match the Layer-2 first-light Battle Factory
 * matchup so an unpatched build runs the same battle Layer 2 shipped
 * with (3v3 Altaria/Vaporeon/Gengar vs three high-tier opponents).
 *
 * `.data` placement is intentional: EWRAM_DATA / BSS storage would
 * be patchable by a different mechanism (write to the load-address
 * for the EWRAM initialiser in ROM), but plain `.data` in the test
 * harness's address space is simpler — patchelf writes directly to
 * the section bytes in the ELF, objcopy emits them into the .gba,
 * and the BIOS init copies them straight into EWRAM at boot.
 *
 * Build: dropped into engine/pokeemerald/test/ — the Makefile picks
 * up *.c under that directory automatically.
 */

#include "test/ebf_test_args.h"

const struct EbfTestArgs gEbfTestArgs =
{
    .schema_version = EBF_TEST_ARGS_SCHEMA_VERSION,
    .rng_seed_1 = 0x1234,
    .rng_seed_2 = 0x5678,
    .player_mons   = { 700, 730, 800 },
    .opponent_mons = { 701, 731, 801 },
    .trainer_id = 0,
    .max_in_rom_turns = 0,  /* 0 = no in-ROM turn-counter watchdog */
    /* Schema v3 — state transport. Defaults reproduce a fresh
     * "battle 1 of challenge 0" matchup so unpatched runs are
     * cartridge-equivalent to the Layer-2 first-light battle. */
    .factory_streak = 0,
    .factory_rents_count = 0,
    .cur_challenge_battle_num = 0,
    .trainer_ids_so_far = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
    .battle_mode = 0,  /* FRONTIER_MODE_SINGLES */
    .lvl_mode = 0,     /* FRONTIER_LVL_50 */
    .player_uses_cartridge_ai = 0,  /* 0 = scripted input via gTestPlayerInput */
    ._pad_v3 = 0,
    /* Schema v4 — Phase 16.5 split-streak override. 0xFFFF =
     * "no override" (use save-block streak for both sides). When
     * non-sentinel, the value is used as the streak for the player's
     * AI flag selection only; opp still uses the save block. */
    .player_ai_streak_override = 0xFFFF,
    ._pad_v4 = { 0, 0 },
    /* Schema v5 — Phase 1.5 frame-level callback. 0 = scripted/AI
     * mode (existing behaviour, default). Non-zero = interactive
     * mode (player controller calls EbfInteractiveYield once per
     * move decision). */
    .player_uses_interactive_callback = 0,
    ._pad_v5 = { 0, 0, 0 },
};
