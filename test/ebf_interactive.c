/*
 * test/ebf_interactive.c
 *
 * Phase 1.5 — frame-level callback support. See
 * docs/19_frame-level-callback-design.md for the full design.
 *
 * The player controller calls EbfInteractiveYield() once per move
 * decision when gEbfTestArgs.player_uses_interactive_callback != 0.
 * The function body is empty (well — a single `nop` to prevent
 * link-time DCE). Python sets a GDB breakpoint at this symbol and
 * uses it as the synchronisation point: pause when hit, read state,
 * write the next action into gEbfInteractiveAction, then continue.
 *
 * The action buffer lives in .data alongside gEbfTestArgs. Python
 * writes to it via mgba's GDB write-memory command; the engine
 * reads it after EbfInteractiveYield returns.
 *
 * Why a dedicated yield function rather than a breakpoint at
 * HandleInputChooseMove? HandleInputChooseMove runs for every
 * battler in every turn including opp AI invocations and switch-ins,
 * so breaking there is noisy. The dedicated yield is called from
 * exactly one site (the player-side move decision) so the GDB
 * driver doesn't need state filtering.
 */

#include "global.h"
#include "test/ebf_interactive.h"

/* Storage for the next-action buffer that Python writes via GDB.
 *
 * Lives in EWRAM (writable at runtime). The cartridge cannot patch
 * .data sections in the test/ object directory — see
 * `ld_script_test.ld`, which deliberately picks up only .text /
 * .bss / .rodata / ewram_data for `test/*.o`. We need writable RAM
 * because Python writes the next action via GDB write-memory mid-
 * battle; .rodata would be read-only at runtime.
 *
 * Schema_version is filled in by C at first yield (lazy init); we
 * cannot put a `.data`-style initializer here. Python should also
 * write schema_version on every action write — the version check
 * inside the player controller is a sanity guard, not load-bearing
 * (Python is in control of the buffer contents). */
EWRAM_DATA struct EbfInteractiveAction gEbfInteractiveAction = {0};

/* No-op function. Exists purely as a known symbol for the GDB
 * breakpoint. The `noinline` + asm-volatile-nop ensures the
 * compiler doesn't elide it or fold it into the caller. */
__attribute__((noinline))
void EbfInteractiveYield(void)
{
    asm volatile ("nop");
}
