/* Test driver of the anchorc front end, built by make as build/parsetool,
 * or run with:
 *   tcc src/arena.c src/diag.c src/lexer.c src/parser.c src/printer.c build/prelude.c -run test/parsetool.c FILE
 *   tcc ... -run test/parsetool.c --prelude
 * FILE prints the canonical form of FILE (src/syntax.h); --prelude prints
 * the bytes of the embedded prelude. Exit 0 ok, 1 refused by the parser,
 * 2 usage or IO. */
#include "../src/prelude.h"
#include "../src/syntax.h"
#include <string.h>

static int usage(void) {
  fputs("usage: parsetool FILE | --prelude\n", stderr);
  return ANCHOR_EXIT_USAGE;
}

static int print_prelude(void) {
  size_t written = fwrite(anchor_prelude_text, 1, anchor_prelude_size, stdout);
  return written == anchor_prelude_size ? ANCHOR_EXIT_OK : ANCHOR_EXIT_USAGE;
}

static int print_file(Arena *arena, const char *path, Diag *diag) {
  const char *text = NULL;
  size_t size = 0;
  int status = anchor_read_source(arena, path, &text, &size, diag);
  if (status != ANCHOR_EXIT_OK)
    return status;
  Program program;
  status = anchor_parse(arena, path, text, size, &program, diag);
  if (status != ANCHOR_EXIT_OK)
    return status;
  anchor_print_program(stdout, &program);
  return ANCHOR_EXIT_OK;
}

int main(int argc, char **argv) {
  if (argc != 2)
    return usage();
  if (strcmp(argv[1], "--prelude") == 0)
    return print_prelude();
  Arena arena;
  Diag diag;
  arena_init(&arena, ANCHOR_ARENA_MAX);
  diag_init(&diag);
  int status = print_file(&arena, argv[1], &diag);
  diag_print(&diag, stderr);
  arena_free(&arena);
  return status;
}
