/* Test driver of the EVM back end, built by make as build/evmtool:
 *   evmtool creation|runtime N   the contract of N members (anchor_evm_write)
 *   evmtool keccak TEXT          keccak256 of the bytes of TEXT
 * Exit 0 ok, 1 refused by the back end, 2 usage. */
#include "../src/evm.h"
#include "../src/keccak.h"
#include <stdlib.h>
#include <string.h>

enum { TOOL_DIGITS = 9 };

static int usage(void) {
  fputs("usage: evmtool creation|runtime N | evmtool keccak TEXT\n", stderr);
  return 2;
}

/* A decimal number of at most TOOL_DIGITS digits, or -1. */
static long number(const char *text) {
  size_t size = strlen(text);
  int ok = size >= 1 && size <= TOOL_DIGITS && strspn(text, "0123456789") == size;
  return ok ? strtol(text, NULL, 10) : -1;
}

static int keccak(const char *text) {
  unsigned char digest[32];
  anchor_keccak256((const unsigned char *)text, strlen(text), digest);
  for (size_t i = 0; i < 32; i++)
    printf("%02x", digest[i]);
  putchar('\n');
  return 0;
}

static int part_of(const char *text, AnchorPart *part) {
  *part = strcmp(text, "creation") == 0 ? ANCHOR_PART_CREATION : ANCHOR_PART_RUNTIME;
  return strcmp(text, "creation") == 0 || strcmp(text, "runtime") == 0;
}

int main(int argc, char **argv) {
  if (argc == 3 && strcmp(argv[1], "keccak") == 0)
    return keccak(argv[2]);
  AnchorPart part;
  if (argc != 3 || !part_of(argv[1], &part))
    return usage();
  long members = number(argv[2]);
  if (members < 0)
    return usage();
  AnchorContract contract = { (unsigned)members };
  return anchor_evm_write(&contract, part, stdout, stderr) == 0 ? 0 : 1;
}
