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
};
