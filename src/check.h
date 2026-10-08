/* The anchorc checker (SPEC section 10, chunk 3; SPEC sections 2 to 6).
 *
 * anchor_check checks the program's `def members : Nat := N` first, then
 * the embedded prelude, then the rest of the program. The program must
 * define `candidates : Candidates` and `rule : Tally -> Outcome`. Errors go
 * to the Diag of the run as "anchorc: CODE: DEF: message":
 *
 *   REFUSE_MEMBERS, REFUSE_DATA, REFUSE_REC, REFUSE_NAME, REFUSE_FORK
 *     the refusal list of SPEC section 2;
 *   TYPE_SCOPE, TYPE_DUPLICATE, TYPE_MISMATCH (with both normal forms),
 *   TYPE_SHAPE, TYPE_INFER, TYPE_UNIVERSE, TYPE_ERASED, TYPE_MATCH, TYPE_MU,
 *   TYPE_REC, TYPE_NAT, TYPE_FUEL, TYPE_INTERNAL, MEMORY
 *     the checker. */
#ifndef ANCHOR_CHECK_H
#define ANCHOR_CHECK_H
#include "syntax.h"

typedef struct AnchorChecked AnchorChecked;

/* Returns ANCHOR_EXIT_OK and the checked program in *CHECKED, or
 * ANCHOR_EXIT_REFUSED with the first error in DIAG. DIAG must stay alive
 * while *CHECKED is used. */
int anchor_check(Arena *arena, const Program *prelude, const Program *program,
                 AnchorChecked **checked, Diag *diag);
unsigned anchor_members(const AnchorChecked *checked);
#endif
