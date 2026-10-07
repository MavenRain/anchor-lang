/* The prelude, embedded in anchorc by tools/embed.c (build/prelude.c), so
 * anchorc reads no prelude file at run time. */
#ifndef ANCHOR_PRELUDE_H
#define ANCHOR_PRELUDE_H
#include <stddef.h>

extern const char anchor_prelude_name[];        /* prelude/Prelude.anc */
extern const unsigned char anchor_prelude_text[]; /* the bytes, then one NUL */
extern const size_t anchor_prelude_size;        /* the bytes without the NUL */
#endif
