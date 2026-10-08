/* anchorc, the anchor-lang compiler (SPEC section 10):
 *   anchorc check PROG                    the fate report (chunk 4b)
 *   anchorc table PROG                    the outcome of each tally (chunk 4a)
 *   anchorc eval PROG NAME                the normal form of NAME (chunk 4b)
 *   anchorc build PROG [--runtime] -o OUT the contract as hex (chunk 5a)
 *   anchorc abi PROG                      the entries of the contract (chunk 5a)
 * Exit 0 ok, 1 refused, 2 usage or IO; errors go to stderr as
 * "anchorc: CODE: DEF: message". Each verb parses the embedded prelude and
 * PROG and checks them (src/check.h), then prints its result and exits 0.
 * build and abi do not tabulate until chunk 5b. */
#include "check.h"
#include "evm.h"
#include "prelude.h"
#include "syntax.h"
#include <string.h>

/* Tabulates the checked program once and prints the table with PRINT. */
static int tabulate(AnchorChecked *checked, void (*print)(FILE *, const AnchorTable *)) {
  AnchorTable table;
  int status = anchor_table(checked, &table);
  if (status == ANCHOR_EXIT_OK)
    print(stdout, &table);
  return status;
}

static int check_verb(AnchorChecked *checked, char **argv) {
  (void)argv;
  return tabulate(checked, anchor_print_report);
}

static int table_verb(AnchorChecked *checked, char **argv) {
  (void)argv;
  return tabulate(checked, anchor_print_table);
}

static int eval_verb(AnchorChecked *checked, char **argv) {
  return anchor_eval(checked, argv[3], stdout);
}

/* The contract of the checked program (src/evm.h). */
static int contract_of(AnchorChecked *checked, AnchorContract *contract) {
  contract->members = anchor_members(checked);
  contract->candidates = 0;
  return anchor_candidates(checked, &contract->candidates);
}

static int abi_verb(AnchorChecked *checked, char **argv) {
  (void)argv;
  AnchorContract contract;
  int status = contract_of(checked, &contract);
  if (status != ANCHOR_EXIT_OK)
    return status;
  return anchor_abi_write(&contract, stdout, stderr) == 0 ? ANCHOR_EXIT_OK : ANCHOR_EXIT_USAGE;
}

/* build PROG [--runtime] -o OUT. OUT gets the hex; a refusal of the back
 * end removes OUT. */
static int build_verb(AnchorChecked *checked, char **argv) {
  int runtime = strcmp(argv[3], "--runtime") == 0;
  const char *path = argv[runtime ? 5 : 4];
  AnchorContract contract;
  int status = contract_of(checked, &contract);
  if (status != ANCHOR_EXIT_OK)
    return status;
  FILE *out = fopen(path, "w");
  if (out == NULL) {
    fprintf(stderr, "anchorc: IO: -: cannot write %s\n", path);
    return ANCHOR_EXIT_USAGE;
  }
  AnchorPart part = runtime ? ANCHOR_PART_RUNTIME : ANCHOR_PART_CREATION;
  int bad = anchor_evm_write(&contract, part, out, stderr);
  int write_failed = ferror(out);
  int closed = fclose(out) == 0;
  if (bad) {
    remove(path);
    return write_failed ? ANCHOR_EXIT_USAGE : ANCHOR_EXIT_REFUSED;
  }
  if (!closed) {
    fprintf(stderr, "anchorc: IO: -: cannot write %s\n", path);
    return ANCHOR_EXIT_USAGE;
  }
  return ANCHOR_EXIT_OK;
}

typedef struct {
  const char *name;
  int argc;  /* argc with the verb, PROG and NAME; build adds -o OUT */
  int (*back)(AnchorChecked *checked, char **argv);
} Verb;

static const Verb VERBS[] = {
  {"check", 3, check_verb}, {"table", 3, table_verb}, {"eval", 4, eval_verb},
  {"build", 5, build_verb}, {"abi", 3, abi_verb}
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

static int run(Arena *arena, const Verb *verb, char **argv, Diag *diag) {
  const char *path = argv[2];
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
  return verb->back(checked, argv);
}

int main(int argc, char **argv) {
  const Verb *verb = argc < 3 ? NULL : find_verb(argv[1]);
  if (verb == NULL || !arguments_fit(verb, argc, argv))
    return usage();
  Arena arena;
  Diag diag;
  arena_init(&arena, ANCHOR_ARENA_MAX);
  diag_init(&diag);
  int status = run(&arena, verb, argv, &diag);
  diag_print(&diag, stderr);
  arena_free(&arena);
  return status;
}
