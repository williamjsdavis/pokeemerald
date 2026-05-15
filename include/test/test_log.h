#ifndef GUARD_TEST_LOG_H
#define GUARD_TEST_LOG_H

/*
 * test/test_log.h
 *
 * Header-only printf-lite over the mGBA debug interface. Lives in
 * `include/test/` because both `test/test_runner.c` and (during the
 * Layer-2c hybrid instrumentation pass) selected engine files under
 * `src/` need to emit sentinel strings from the same TEST=1 build.
 *
 * Memory-mapped interface, same convention as the expansion:
 *   REG_DEBUG_ENABLE  = 0x4FFF780  (write 0xC0DE; read 0x1DEA to confirm)
 *   REG_DEBUG_FLAGS   = 0x4FFF700  (write log-level | 0x100 to flush)
 *   REG_DEBUG_STRING  = 0x4FFF600  (256-byte buffer)
 *
 * All functions are `static inline` so the file is safe to include
 * from multiple translation units without link-time conflicts.
 *
 * Caveats:
 *   - The flush writes the entire string buffer; concurrent calls from
 *     interrupt handlers would race. Don't call from a handler.
 *   - Output is observed only when running under mGBA — on real hardware
 *     these become harmless writes to BIOS-protected addresses.
 *   - The helpers are intended for TEST=1 builds. They compile fine in
 *     non-test builds too (they're inline; the compiler elides them if
 *     unused), but production code shouldn't depend on the side effect.
 */

#include "global.h"
#include "gba/isagbprint.h"  /* MGBA_LOG_INFO */

#define MGBA_LOG_REG_DEBUG_ENABLE   (*(volatile u16 *) 0x4FFF780)
#define MGBA_LOG_REG_DEBUG_FLAGS    (*(volatile u16 *) 0x4FFF700)
#define MGBA_LOG_REG_DEBUG_STRING   ((char *) 0x4FFF600)

static inline bool32 Mgba_LogOpen(void)
{
    MGBA_LOG_REG_DEBUG_ENABLE = 0xC0DE;
    return MGBA_LOG_REG_DEBUG_ENABLE == 0x1DEA;
}

static inline void Mgba_LogPuts(const char *s)
{
    s32 i = 0;
    while (s[i] && i < 255)
    {
        MGBA_LOG_REG_DEBUG_STRING[i] = s[i];
        i++;
    }
    MGBA_LOG_REG_DEBUG_STRING[i] = '\0';
    MGBA_LOG_REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
}

static inline s32 Mgba_LogPutIntAt_(s32 i, u32 value)
{
    char buf[12];
    s32 j = 0;
    if (value == 0)
    {
        buf[j++] = '0';
    }
    else
    {
        while (value > 0 && j < (s32) sizeof(buf))
        {
            buf[j++] = '0' + (value % 10);
            value /= 10;
        }
    }
    while (j > 0 && i < 255)
    {
        MGBA_LOG_REG_DEBUG_STRING[i++] = buf[--j];
    }
    return i;
}

static inline void Mgba_LogLabelInt(const char *label, u32 value)
{
    s32 i = 0;
    while (label[i] && i < 250)
    {
        MGBA_LOG_REG_DEBUG_STRING[i] = label[i];
        i++;
    }
    MGBA_LOG_REG_DEBUG_STRING[i++] = '=';
    i = Mgba_LogPutIntAt_(i, value);
    MGBA_LOG_REG_DEBUG_STRING[i] = '\0';
    MGBA_LOG_REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
}

/*
 * Stops the emulator via SWI 0x3 with `exitCode` in r0. Inline-able
 * so any TU can exit cleanly without depending on test_runner.c's
 * internal `MgbaExit_`. Empirically mgba-rom-test-mac doesn't
 * propagate r0 to the subprocess exit code (see docs/09 caveats),
 * but the SWI is what stops emulation cleanly — Python parses
 * stdout sentinels instead.
 */
static inline void Mgba_LogExit(u8 exitCode) __attribute__((noreturn));
static inline void Mgba_LogExit(u8 exitCode)
{
    register u32 _exitCode asm("r0") = exitCode;
    asm("swi 0x3" :: "r" (_exitCode));
    while (1) { }  /* unreachable: mgba terminates here */
}

/*
 * Prints "LABEL=a:b". Two-field variant of Mgba_LogLabel3Int; same
 * splitting rule on the Python side. Used by recording hooks that
 * carry exactly two fields (STATUS, FAINT-with-cause, etc.).
 */
static inline void Mgba_LogLabel2Int(const char *label, u32 a, u32 b)
{
    s32 i = 0;
    while (label[i] && i < 245)
    {
        MGBA_LOG_REG_DEBUG_STRING[i] = label[i];
        i++;
    }
    MGBA_LOG_REG_DEBUG_STRING[i++] = '=';
    i = Mgba_LogPutIntAt_(i, a);
    MGBA_LOG_REG_DEBUG_STRING[i++] = ':';
    i = Mgba_LogPutIntAt_(i, b);
    MGBA_LOG_REG_DEBUG_STRING[i] = '\0';
    MGBA_LOG_REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
}

/*
 * Prints "LABEL=a:b:c". Used by the Phase 1.3 layer 4 recording hooks
 * for events that carry more than one field (TURN_MOVE, HP). The
 * Python parser splits on `=` then `:`. Keep all values within u32 —
 * the helper has no signed-int support.
 */
static inline void Mgba_LogLabel3Int(const char *label, u32 a, u32 b, u32 c)
{
    s32 i = 0;
    while (label[i] && i < 240)
    {
        MGBA_LOG_REG_DEBUG_STRING[i] = label[i];
        i++;
    }
    MGBA_LOG_REG_DEBUG_STRING[i++] = '=';
    i = Mgba_LogPutIntAt_(i, a);
    MGBA_LOG_REG_DEBUG_STRING[i++] = ':';
    i = Mgba_LogPutIntAt_(i, b);
    MGBA_LOG_REG_DEBUG_STRING[i++] = ':';
    i = Mgba_LogPutIntAt_(i, c);
    MGBA_LOG_REG_DEBUG_STRING[i] = '\0';
    MGBA_LOG_REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
}

#endif /* GUARD_TEST_LOG_H */
