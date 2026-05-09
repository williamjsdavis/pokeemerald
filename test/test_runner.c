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
 * CB2_TestRunner is the entry point selected by ld_script_test.ld
 * (it overrides gInitialMainCB2). It runs once per VBlank-cycle but we
 * don't actually need to wait for vblank — we set up, print, exit.
 *
 * Exit codes:
 *   0 = test pass
 *   1 = mGBA debug interface unavailable (likely running on real hardware)
 */
void CB2_TestRunner(void)
{
    if (!MgbaOpen_())
    {
        /* No way to print; just exit non-zero. */
        MgbaExit_(1);
    }

    MgbaPuts_("ebf-ai test harness: hello from CB2_TestRunner");
    MgbaPuts_("step 0/N: harness boot OK");
    MgbaPuts_("PASS");  /* sentinel string — the Python harness greps for this */
    MgbaPuts_("DONE");  /* end-of-test sentinel */
    MgbaExit_(0);
}
