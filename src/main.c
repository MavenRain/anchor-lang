/* anchorc, the anchor-lang compiler (SPEC section 10):
 *   anchorc check PROG                    the fate report (chunk 4)
 *   anchorc table PROG                    the outcome of each tally (chunk 4)
 *   anchorc eval PROG NAME                the normal form of NAME (chunk 4)
 *   anchorc build PROG [--runtime] -o OUT the contract (chunk 5)
 *   anchorc abi PROG                      the entries of the contract (chunk 5)
 * Exit 0 ok, 1 refused, 2 usage or IO; errors go to stderr as
 * "anchorc: CODE: DEF: message". Each verb parses the embedded prelude and
 * PROG and checks them (src/check.h). table prints the outcome table
 * (chunk 4a); the other verbs still exit 1 with PLANNED until their back
 * end lands. */
#include "check.h"
#include "prelude.h"
#include "syntax.h"
#include <string.h>

typedef struct {
  const char *name;
  int argc;  /* argc with the verb, PROG and NAME; build adds -o OUT */
} Verb;

static const Verb VERBS[] = {
  {"check", 3}, {"table", 3}, {"eval", 4}, {"build", 5}, {"abi", 3}
};

static int usage(void) {
  fputs("anchorc: USAGE: -: anchorc check|table|abi PROG, anchorc eval PROG NAME,"
        " anchorc build PROG [--runtime] -o OUT\n", stderr);
  return ANCHOR_EXIT_USAGE;
}

static const Verb *find_verb(const char *name) {
  for (size_t i = 0; i < sizeof VERBS / sizeof VERBS[0]; i++)
    if (strcmp(VERBS[i].name, name) == 0)
      return &VERBS[i];
  return NULL;
}

/* build PROG -o OUT or build PROG --runtime -o OUT */
static int build_fits(int argc, char **argv) {
  int runtime = argc == 6 && strcmp(argv[3], "--runtime") == 0;
  return (argc == 5 || runtime) && strcmp(argv[runtime ? 4 : 3], "-o") == 0;
}

static int arguments_fit(const Verb *verb, int argc, char **argv) {
  if (strcmp(verb->name, "build") == 0)
    return build_fits(argc, argv);
  return argc == verb->argc;
}

static int print_table(AnchorChecked *checked) {
  AnchorTable table;
  int status = anchor_table(checked, &table);
  if (status == ANCHOR_EXIT_OK)
    anchor_print_table(stdout, &table);
  return status;
}

static int run(Arena *arena, const Verb *verb, const char *path, Diag *diag) {
  Program prelude;
  const char *prelude_text = (const char *)anchor_prelude_text;
  int status = anchor_parse(arena, anchor_prelude_name, prelude_text, anchor_prelude_size, &prelude, diag);
  if (status != ANCHOR_EXIT_OK)
    return status;
  const char *text = NULL;
  size_t size = 0;
  status = anchor_read_source(arena, path, &text, &size, diag);
  if (status != ANCHOR_EXIT_OK)
    return status;
  Program program;
  status = anchor_parse(arena, path, text, size, &program, diag);
  if (status != ANCHOR_EXIT_OK)
    return status;
  AnchorChecked *checked = NULL;
  status = anchor_check(arena, &prelude, &program, &checked, diag);
  if (status != ANCHOR_EXIT_OK)
    return status;
  if (strcmp(verb->name, "table") == 0)
    return print_table(checked);
  diag_set(diag, "PLANNED", span_of("-"), "anchorc %s has no back end yet (SPEC section 10)", verb->name);
  return ANCHOR_EXIT_REFUSED;
}

int main(int argc, char **argv) {
  const Verb *verb = argc < 3 ? NULL : find_verb(argv[1]);
  if (verb == NULL || !arguments_fit(verb, argc, argv))
    return usage();
  Arena arena;
  Diag diag;
  arena_init(&arena, ANCHOR_ARENA_MAX);
  diag_init(&diag);
  int status = run(&arena, verb, argv[2], &diag);
  diag_print(&diag, stderr);
  arena_free(&arena);
  return status;
}
