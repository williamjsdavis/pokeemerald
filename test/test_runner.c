/*
 * test/test_runner.c
 *
 * Minimal test runner harness for the ebf-ai project.
 *
 * Backported from rh-hideout/pokeemerald-expansion's test infrastructure
 * but stripped to the minimum: this runner does not implement the DSL
 * (GIVEN/WHEN/SCENE), the test registry, parametrization, or the recorded
 * battle system. It is intentionally a "hello world" floor — a proof
 * that the build pipeline (linker script override → CB2_TestRunner →
 * mGBA debug-print → SWI 0x3 exit) works end-to-end.
 *
 * Once this is verified by mgba-rom-test-mac picking up the print and
 * the clean exit code, the next layer (real battle setup, recording
 * hooks, action injection) lands on top.
 *
 * mGBA debug interface (matches the expansion):
 *   REG_DEBUG_ENABLE  = 0x4FFF780  (write 0xC0DE to enable; read 0x1DEA to confirm)
 *   REG_DEBUG_FLAGS   = 0x4FFF700  (write log-level | 0x100 to flush)
 *   REG_DEBUG_STRING  = 0x4FFF600  (256-byte string buffer)
 *
 * Exit: SWI 0x3 with r0 = exit code. mgba-rom-test will return that
 * code as its own exit status when invoked with `-R r0`.
 */

#include "global.h"
#include "gba/isagbprint.h"  /* provides MGBA_LOG_INFO */
#include "main.h"            /* SetMainCallback2, gMain */
#include "random.h"
#include "battle.h"          /* gBattleTypeFlags, gBattleOutcome, BATTLE_TYPE_* */
#include "battle_main.h"     /* CB2_InitBattle */
#include "battle_setup.h"    /* gTrainerBattleOpponent_A */
#include "battle_factory.h"  /* CallBattleFactoryFunction */
#include "battle_tower.h"    /* gFrontierTempParty[] */
#include "event_data.h"      /* gSpecialVar_0x8004 / _0x8005 */
#include "constants/battle.h"            /* B_OUTCOME_* */
#include "constants/battle_factory.h"    /* BATTLE_FACTORY_FUNC_SET_PARTIES */
#include "constants/battle_frontier.h"   /* FRONTIER_MAX_LEVEL_50 etc. */
#include "constants/battle_frontier_mons.h"  /* NUM_FRONTIER_MONS, FRONTIER_MONS_HIGH_TIER */
#include "constants/trainers.h"          /* opponent trainer ids */
#include "test/ebf_test_args.h"          /* gEbfTestArgs (Layer 3 patchelf input) */

#define REG_DEBUG_ENABLE  (*(volatile u16 *) 0x4FFF780)
#define REG_DEBUG_FLAGS   (*(volatile u16 *) 0x4FFF700)
#define REG_DEBUG_STRING  ((char *) 0x4FFF600)

static bool32 MgbaOpen_(void)
{
    REG_DEBUG_ENABLE = 0xC0DE;
    return REG_DEBUG_ENABLE == 0x1DEA;
}

static void MgbaPuts_(const char *s)
{
    s32 i = 0;
    while (s[i] && i < 255)
    {
        REG_DEBUG_STRING[i] = s[i];
        i++;
    }
    REG_DEBUG_STRING[i] = '\0';
    REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
}

/* Append a decimal integer to REG_DEBUG_STRING starting at offset `i`,
 * returning the new offset. Helper for the printf-lite below. */
static s32 MgbaPutInt_(s32 i, u32 value)
{
    char buf[12];
    s32 j = 0;
    if (value == 0)
    {
        buf[j++] = '0';
    }
    else
    {
        while (value > 0 && j < (s32)sizeof(buf))
        {
            buf[j++] = '0' + (value % 10);
            value /= 10;
        }
    }
    /* buf has digits in reverse order; flip into the debug-string buffer */
    while (j > 0 && i < 255)
    {
        REG_DEBUG_STRING[i++] = buf[--j];
    }
    return i;
}

/* Just enough of printf to emit "<label>=<int>". The expansion has a
 * fuller MgbaVPrintf_ supporting %s/%d/%S; we add it later if needed. */
static void MgbaPrintLabelInt_(const char *label, u32 value)
{
    s32 i = 0;
    while (label[i] && i < 250)
    {
        REG_DEBUG_STRING[i] = label[i];
        i++;
    }
    REG_DEBUG_STRING[i++] = '=';
    i = MgbaPutInt_(i, value);
    REG_DEBUG_STRING[i] = '\0';
    REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
}

/*
 * mgba intercepts SWI 0x3 and stops the emulator. r0 carries the exit
 * code by convention with the expansion's runner, but empirically
 * mgba-rom-test-mac (the macOS universal binary we use) does not seem
 * to propagate r0 to its own process exit code via `-R r0`. So we
 * cannot rely on the exit code alone for pass/fail; the Python
 * harness-side will instead grep for sentinel strings in stdout
 * (e.g. "PASS" / "FAIL" / "DONE"). The SWI is still useful — it
 * terminates mgba cleanly and predictably. (Backport TODO: investigate
 * whether the macOS build has a different convention or whether
 * we need a different mgba flag.)
 */
static void MgbaExit_(u8 exitCode)
{
    register u32 _exitCode asm("r0") = exitCode;
    asm("swi 0x3" :: "r" (_exitCode));
    /* unreachable: mgba terminates the ROM here */
    while (1) { }
}

/*
 * Layer 2 end-of-battle hook. Installed as `gMain.savedCallback` so
 * the engine's battle teardown will jump to us after `gBattleOutcome`
 * is set and cleanup completes. See `battle_main.c:5248` —
 * `FreeResetData_ReturnToOvOrDoEvolutions` calls
 * `SetMainCallback2(gMain.savedCallback)` at end-of-battle.
 *
 * NOTE on watchdog: initially we tried a `gMain.callback1` watchdog
 * polling `gBattleOutcome` each frame. It got clobbered: the engine
 * overwrites `gMain.callback1 = BattleMainCB1` during battle init (line
 * 1140 and a few other spots). The savedCallback path is the standard
 * mechanism the cartridge uses to return to the overworld; it's safe
 * for the engine to assume we don't repurpose `callback1`.
 *
 * Hang detection: if the battle init itself hangs (e.g., a missing
 * graphics path), savedCallback never fires. We rely on mgba-rom-test's
 * timeout-Millis as the ultimate backstop in that case, and on the
 * trail of `MgbaPuts_` sentinels above the hand-off point in
 * `CB2_TestRunner` to know how far we got.
 */
static void CB2_TestRunnerEndOfBattle(void)
{
    MgbaPrintLabelInt_("BATTLE_OUTCOME", gBattleOutcome);
    MgbaPuts_("PASS");
    MgbaPuts_("DONE");
    MgbaExit_(0);
}

/*
 * Phase 16.1 POC — call the cartridge's own GenerateInitialRentalMons
 * (battle_factory.c:509) to produce a 6-mon candidate pool, then emit
 * the result so Python can verify cartridge-side rental generation
 * works from the test runner. This does NOT yet replace the existing
 * gEbfTestArgs.player_mons / opponent_mons path — the battle still
 * runs against whatever Python patched in. The cartridge-generated
 * 6 are emitted alongside for cross-checking.
 *
 * Prerequisites the cartridge function reads:
 *   - gSaveBlock2Ptr->frontier.lvlMode   (set by SetupFirstLightBattle_)
 *   - gSaveBlock2Ptr->frontier.factoryWinStreaks[mode][lvl]  (default 0)
 *   - gSaveBlock2Ptr->frontier.factoryRentsCount[mode][lvl]  (default 0)
 *   - VarGet(VAR_FRONTIER_BATTLE_MODE)   (default 0 = SINGLES)
 *
 * The default save block (`gSaveblock2.block` in main.c) is zero-init,
 * so the streak/rents-count are correctly 0. The challenge number
 * derives from the streak (0/7 = 0), so we generate from the lowest
 * tier band (sInitialRentalMonRanges[0..0+8] for Lv50; first entry
 * 110..199 = the Grimer-to-Furret block).
 *
 * Emits one line: `INITIAL_RENTALS=mon0:mon1:mon2:mon3:mon4:mon5`.
 */
static void EmitCartridgeInitialRentals_(void)
{
    s32 i, j;

    /* Run the dispatch — same path the cartridge's
     * BattleFactoryPreBattleRoom script takes via factory_generaterentalmons. */
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_GENERATE_RENTAL_MONS;
    CallBattleFactoryFunction();

    /* Emit the 6 cartridge-chosen rental monIds in one labelled line.
     * Format: `INITIAL_RENTALS=a:b:c:d:e:f` (six u16 values, colon-sep). */
    {
        const char *label = "INITIAL_RENTALS";
        j = 0;
        while (label[j] && j < 250)
        {
            REG_DEBUG_STRING[j] = label[j];
            j++;
        }
        REG_DEBUG_STRING[j++] = '=';
        for (i = 0; i < 6; i++)
        {
            if (i > 0)
                REG_DEBUG_STRING[j++] = ':';
            j = MgbaPutInt_(j, gSaveBlock2Ptr->frontier.rentalMons[i].monId);
        }
        REG_DEBUG_STRING[j] = '\0';
        REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
    }
}

/*
 * Phase 16.2 POC — drive the cartridge's GenerateOpponentMons and the
 * two hint computations (GetOpponentMostCommonMonType,
 * GetOpponentBattleStyle). Emits:
 *   OPP_TEAM=monId0:monId1:monId2     (the 3 opp facility-rental indices)
 *   OPP_TRAINER=trainerId              (which trainer the cartridge picked)
 *   OPP_TYPE_HINT=type_id              (TYPE_* enum, or NUMBER_OF_MON_TYPES for "no standout")
 *   OPP_STYLE_HINT=style_id            (FACTORY_STYLE_*, or FACTORY_NUM_STYLES for "tied / no standout")
 *
 * Like the rental POC, this runs additively: we generate the opp team
 * + hints, emit the values, then the existing setup overwrites
 * gSaveBlock2Ptr->frontier.rentalMons[3..5] with the Python-patched
 * opponent_mons before the battle starts. So the battle is still
 * fought against the Python-chosen opp; the cartridge-generated +
 * cartridge-hinted values are recorded alongside for cross-checking.
 *
 * Note: GenerateOpponentMons writes to gFrontierTempParty[0..2], not
 * directly to rentalMons. The hint computations read from
 * gFrontierTempParty too. We read from that for the emit.
 */
static void EmitCartridgeOppAndHints_(void)
{
    s32 i, j;

    /* Generate next opp team (cartridge writes to gFrontierTempParty[0..2]). */
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_GENERATE_OPPONENT_MONS;
    CallBattleFactoryFunction();

    /* Emit OPP_TEAM line. */
    {
        const char *label = "OPP_TEAM";
        j = 0;
        while (label[j] && j < 250) { REG_DEBUG_STRING[j] = label[j]; j++; }
        REG_DEBUG_STRING[j++] = '=';
        for (i = 0; i < 3; i++)
        {
            if (i > 0) REG_DEBUG_STRING[j++] = ':';
            j = MgbaPutInt_(j, gFrontierTempParty[i]);
        }
        REG_DEBUG_STRING[j] = '\0';
        REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
    }

    /* Emit the picked trainer ID. */
    MgbaPrintLabelInt_("OPP_TRAINER", gTrainerBattleOpponent_A);

    /* Type hint: returns via gSpecialVar_Result. */
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_GET_OPPONENT_MON_TYPE;
    CallBattleFactoryFunction();
    MgbaPrintLabelInt_("OPP_TYPE_HINT", gSpecialVar_Result);

    /* Style hint. */
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_GET_OPPONENT_STYLE;
    CallBattleFactoryFunction();
    MgbaPrintLabelInt_("OPP_STYLE_HINT", gSpecialVar_Result);
}

/*
 * Hard-coded matchup for first-light. Picks rental indices from the
 * pool such that the player side has higher-tier sets than the
 * opponent. Once Layer 3 lands, these become inputs patched in via
 * `patchelf` before each run.
 *
 * Indices 0..2 → player party, 3..5 → opponent party. The actual
 * species/moveset behind each index lives in `gBattleFrontierMons[]`
 * in `engine/pokeemerald/src/data/battle_frontier/battle_frontier_mons.h`.
 *
 * IV byte is the "stat emphasis" field that the Battle Factory uses to
 * pick which IV-tier table the mon draws from; 0 = baseline. The
 * personality field affects gendered species and some moves but is
 * inert for our pool. `abilityNum` 0 selects the species's primary
 * ability.
 */
/*
 * Higher-tier rentals so neither side OHKOs the other on turn 1 (the
 * low-index Caterpie/Weedle pool used for spike-2 was OHKO'd by its
 * own Tackle, generating a turn-1 forced switch that exercised every
 * graphics-bound code path we needed to stub). With these indices,
 * the battle should run multiple turns before the first faint.
 */
static const struct RentalMon sFirstLightRentals[6] =
{
    /* Player */
    { .monId = 700, .ivs = 0, .personality = 0x12345678, .abilityNum = 0 }, /* ALTARIA */
    { .monId = 730, .ivs = 0, .personality = 0x12345679, .abilityNum = 0 }, /* VAPOREON */
    { .monId = 800, .ivs = 0, .personality = 0x1234567A, .abilityNum = 0 }, /* GENGAR */
    /* Opponent */
    { .monId = 701, .ivs = 0, .personality = 0x1234567B, .abilityNum = 0 },
    { .monId = 731, .ivs = 0, .personality = 0x1234567C, .abilityNum = 0 },
    { .monId = 801, .ivs = 0, .personality = 0x1234567D, .abilityNum = 0 },
};

static void SetupFirstLightBattle_(void)
{
    s32 i;

    /* RNG: seed from the patchelf-addressable argument block. The
     * default values (in `ebf_test_args.c`) match the pre-Layer-3
     * hardcoded constants so an unpatched ROM is byte-identical
     * to the Layer-2 first-light battle. */
    SeedRng(gEbfTestArgs.rng_seed_1);
    SeedRng2(gEbfTestArgs.rng_seed_2);

    /* Battle Factory facility state. Most fields default-zero is OK;
     * we set only what `SetPlayerAndOpponentParties` actually reads. */
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;

    /* Phase 16.1/16.2 POC: cartridge-side initial-rental pool and
     * opp-team-with-hints emission. Wrapped in a save/restore of the
     * RNG state so the battle the engine actually runs (which uses
     * the Python-patched player/opp teams below) sees the same RNG
     * state it would have without the POC calls. This keeps the POC
     * a pure observation point — zero behavioural impact on the
     * battle. See docs/16_in-rom-factory-orchestration.md. */
    {
        u32 savedRng = gRngValue;
        u32 savedRng2 = gRng2Value;
        EmitCartridgeInitialRentals_();
        EmitCartridgeOppAndHints_();
        gRngValue = savedRng;
        gRng2Value = savedRng2;
    }

    /* Build the rental array from `gEbfTestArgs`. Non-monId fields
     * inherit from `sFirstLightRentals` (cosmetic personality /
     * ability fields don't move the needle for Layer 3 tests). */
    for (i = 0; i < 6; i++)
    {
        gSaveBlock2Ptr->frontier.rentalMons[i] = sFirstLightRentals[i];
        if (i < 3)
            gSaveBlock2Ptr->frontier.rentalMons[i].monId = gEbfTestArgs.player_mons[i];
        else
            gSaveBlock2Ptr->frontier.rentalMons[i].monId = gEbfTestArgs.opponent_mons[i - 3];
    }

    /* Build the two parties from the rental array. This is the
     * standard cartridge entry point — populates `gPlayerParty` and
     * `gEnemyParty` with full Pokémon structs (stats, moves, items). */
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_SET_PARTIES;
    gSpecialVar_0x8005 = 0;  /* 0 = build both parties */
    CallBattleFactoryFunction();

    /* Battle-type flags: this is the exact flag combination the
     * cartridge uses for Battle Factory (see battle_tower.c:2093,
     * SPECIAL_BATTLE_FACTORY case). BATTLE_TYPE_FRONTIER is a
     * COMPOSITE mask that includes BATTLE_TYPE_ARENA, BATTLE_TYPE_DOME,
     * etc. — setting it makes the engine run Arena/Dome subsystems
     * (e.g., VARIOUS_ARENA_WAIT_STRING) that wait on text printers
     * that never tick headlessly. The cartridge sets only the
     * specific facility flag (FACTORY) plus TRAINER. */
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_FACTORY;

    gTrainerBattleOpponent_A = gEbfTestArgs.trainer_id;

    /* Sanity print: schema version + first player species index.
     * Asserts (a) the patchelf write landed (Python writer can
     * grep this) and (b) the schema matches what the writer thinks
     * it does. Quiet on a normal pass — three integers' worth. */
    MgbaPrintLabelInt_("ebf_args_schema", gEbfTestArgs.schema_version);
    MgbaPrintLabelInt_("ebf_args_player_mon0", gEbfTestArgs.player_mons[0]);
    MgbaPrintLabelInt_("ebf_args_opponent_mon0", gEbfTestArgs.opponent_mons[0]);
}

/*
 * CB2_TestRunner is the entry point selected by ld_script_test.ld
 * (it overrides gInitialMainCB2). It is called once per main-loop
 * iteration as long as `gMain.callback2` points at it. Our pattern:
 *
 *   - First (and only) invocation: print sanity sentinels, set up a
 *     Battle Factory matchup, install `CB1_BattleWatchdog` as the
 *     per-frame poll, switch `gMain.callback2` to `CB2_InitBattle`,
 *     and return. The engine then drives the battle frame by frame.
 *   - The watchdog (`CB1`) exits via `MgbaExit_` once `gBattleOutcome`
 *     is set, so this function is never re-entered.
 *
 * Exit codes (set by `MgbaExit_` via SWI 0x3):
 *   0 = battle completed; outcome printed.
 *   1 = mGBA debug interface unavailable (likely running on real HW).
 *   2 = battle didn't complete within `BATTLE_MAX_FRAMES` — watchdog
 *       timeout. Suggests an engine hang or an unexpected code path.
 */
void CB2_TestRunner(void)
{
    if (!MgbaOpen_())
    {
        /* No way to print; just exit non-zero. */
        MgbaExit_(1);
    }

    MgbaPuts_("ebf-ai test harness: hello from CB2_TestRunner");

    /* Layer 1.6 sanity checks: prove that game-engine code and headers
     * are linked into the test ROM. */
    MgbaPrintLabelInt_("NUM_FRONTIER_MONS", NUM_FRONTIER_MONS);             /* expect 882 */
    MgbaPrintLabelInt_("FRONTIER_MONS_HIGH_TIER", FRONTIER_MONS_HIGH_TIER); /* expect 849 */
    MgbaPrintLabelInt_("Random()", Random());

    MgbaPuts_("step 0/N: harness boot OK");

    /* Layer 2 (Option A): set up a Battle Factory matchup and hand off
     * to the engine. The watchdog handles exit when the battle ends. */
    MgbaPuts_("layer2: setting up Battle Factory matchup");
    SetupFirstLightBattle_();

    MgbaPuts_("layer2: handing off to CB2_InitBattle");
    /* When the battle's teardown completes (FreeResetData_Return…) it
     * jumps to gMain.savedCallback. Hooking that gives us a clean way
     * to read gBattleOutcome and exit. */
    gMain.savedCallback = CB2_TestRunnerEndOfBattle;
    SetMainCallback2(CB2_InitBattle);
    /* Returns to the main loop; next iteration calls CB2_InitBattle. */
}
