/*
 * include/test/ebf_interactive.h
 *
 * Phase 1.5 — frame-level callback support. Public surface:
 *   - struct EbfInteractiveAction — one TurnAction to inject.
 *   - gEbfInteractiveAction — the buffer Python writes to.
 *   - EbfInteractiveYield() — the breakpoint target.
 *
 * Schema for the action buffer is version-stamped so Python can
 * verify it's talking to a matching ROM build. Bump
 * EBF_INTERACTIVE_ACTION_SCHEMA_VERSION on any layout change.
 */

#ifndef GUARD_EBF_INTERACTIVE_H
#define GUARD_EBF_INTERACTIVE_H

#define EBF_INTERACTIVE_ACTION_SCHEMA_VERSION 1

/*
 * One action to inject. `action_kind` and `move_index` mirror the
 * existing `TurnAction` Python dataclass. `schema_version` lets
 * Python verify the C struct layout matches its packer. Pad to
 * 8 bytes for natural alignment.
 */
struct EbfInteractiveAction
{
    u8  schema_version;
    u8  action_kind;       /* B_ACTION_* — typically USE_MOVE (0) or SWITCH */
    u8  move_index;        /* 0..3 when action_kind == USE_MOVE */
    u8  switch_target;     /* party slot when action_kind == SWITCH */
    u8  _pad[4];           /* round to 8-byte boundary */
};

extern struct EbfInteractiveAction gEbfInteractiveAction;

void EbfInteractiveYield(void);

#endif /* GUARD_EBF_INTERACTIVE_H */
