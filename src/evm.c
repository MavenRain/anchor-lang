/* The EVM back end of anchorc (evm.h). The assembler writes bytecode
 * directly; anchor_evm_write is the target interface.
 *
 * Every failure of a contract is REVERT with empty output: a short
 * calldata, an unknown selector, a call value and a failed guard. */
#include "evm.h"
#include "keccak.h"
#include <stdarg.h>
#include <string.h>

static int fail(FILE *err, const char *code, const char *format, ...) {
  va_list args;
  va_start(args, format);
  fprintf(err, "anchorc: %s: ", code);
  vfprintf(err, format, args);
  fputc('\n', err);
  va_end(args);
  return 1;
}

void evm_init(EvmAsm *a) { memset(a, 0, sizeof *a); }

void evm_byte(EvmAsm *a, unsigned value) {
  a->full = a->full || a->size >= EVM_CAPACITY;
  if (a->full)
    return;
  a->code[a->size] = (unsigned char)value;
  a->size++;
}

void evm_op(EvmAsm *a, EvmOp code) { evm_byte(a, (unsigned)code); }

void evm_push_word(EvmAsm *a, const unsigned char word[32]) {
  size_t lead = 0;
  while (lead < 32 && word[lead] == 0)
    lead++;
  evm_byte(a, (unsigned)OP_PUSH0 + (unsigned)(32 - lead));
  for (size_t i = lead; i < 32; i++)
    evm_byte(a, word[i]);
}

void evm_push(EvmAsm *a, unsigned long value) {
  unsigned char word[32] = {0};
  for (size_t i = 0; i < sizeof value && i < 32; i++)
    word[31 - i] = (unsigned char)(value >> (8 * i));
  evm_push_word(a, word);
}

void evm_push_label(EvmAsm *a, EvmLabel label) {
  a->broken = a->broken || label >= EVM_LABELS;
  a->full = a->full || a->sites >= EVM_FIXUPS;
  if (a->full || a->broken)
    return;
  evm_op(a, OP_PUSH2);
  a->site[a->sites] = a->size;
  a->target[a->sites] = label;
  a->sites++;
  evm_byte(a, 0);
  evm_byte(a, 0);
}

void evm_bind(EvmAsm *a, EvmLabel label) {
  a->broken = a->broken || label >= EVM_LABELS;
  if (a->broken)
    return;
  a->at[label] = a->size;
  a->bound[label] = 1;
}

void evm_jumpdest(EvmAsm *a, EvmLabel label) {
  evm_bind(a, label);
  evm_op(a, OP_JUMPDEST);
}

void evm_jump(EvmAsm *a, EvmLabel label) {
  evm_push_label(a, label);
  evm_op(a, OP_JUMP);
}

void evm_jump_if(EvmAsm *a, EvmLabel label) {
  evm_push_label(a, label);
  evm_op(a, OP_JUMPI);
}

void evm_revert_if(EvmAsm *a) { evm_jump_if(a, EVM_LABEL_REVERT); }

/* Each site holds two bytes below a->size, because evm_finish runs only
 * when no evm_byte was dropped. */
static int resolve(EvmAsm *a) {
  int ok = !a->full && !a->broken;
  for (size_t i = 0; ok && i < a->sites; i++) {
    EvmLabel label = a->target[i];
    size_t site = a->site[i];
    ok = label < EVM_LABELS && a->bound[label] && a->at[label] <= 0xffff && site + 1 < a->size;
    if (ok) {
      a->code[site] = (unsigned char)(a->at[label] >> 8);
      a->code[site + 1] = (unsigned char)a->at[label];
    }
  }
  return ok;
}

void evm_argument(EvmAsm *a, unsigned j) {
  evm_push(a, 4ul + 32ul * j);
  evm_op(a, OP_CALLDATALOAD);
}

void evm_dispatch_head(EvmAsm *a) {
  evm_push(a, 4);
  evm_op(a, OP_CALLDATASIZE);
  evm_op(a, OP_LT);
  evm_revert_if(a);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_CALLDATALOAD);
  evm_push(a, 0xe0);
  evm_op(a, OP_SHR);
}

void evm_dispatch(EvmAsm *a, const char *signature, EvmLabel label) {
  unsigned char digest[32] = {0};
  anchor_keccak256((const unsigned char *)signature, strlen(signature), digest);
  evm_op(a, OP_DUP1);
  evm_op(a, OP_PUSH4);
  for (size_t i = 0; i < 4; i++)
    evm_byte(a, digest[i]);
  evm_op(a, OP_EQ);
  evm_jump_if(a, label);
}

void evm_entry(EvmAsm *a, EvmLabel label, unsigned words) {
  evm_jumpdest(a, label);
  evm_op(a, OP_POP);
  evm_op(a, OP_CALLVALUE);
  evm_revert_if(a);
  evm_push(a, 4ul + 32ul * words);
  evm_op(a, OP_CALLDATASIZE);
  evm_op(a, OP_LT);
  evm_revert_if(a);
}

void evm_return_top(EvmAsm *a) {
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_MSTORE);
  evm_push(a, 0x20);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_RETURN);
}

void evm_revert_block(EvmAsm *a) {
  evm_jumpdest(a, EVM_LABEL_REVERT);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_REVERT);
}

void evm_creation(EvmAsm *a, const EvmAsm *body) {
  evm_op(a, OP_CALLVALUE);
  evm_jump_if(a, EVM_LABEL_REVERT);
  evm_push(a, body->size);
  evm_op(a, OP_DUP1);
  evm_push_label(a, EVM_LABEL_RUNTIME);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_CODECOPY);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_RETURN);
  evm_revert_block(a);
  evm_bind(a, EVM_LABEL_RUNTIME);
  for (size_t i = 0; i < body->size && i < EVM_CAPACITY; i++)
    evm_byte(a, body->code[i]);
}

int evm_finish(EvmAsm *a, FILE *err) {
  if (a->full)
    return fail(err, "EVM_SIZE", "the bytecode exceeds %d bytes or %d label sites", EVM_CAPACITY,
                EVM_FIXUPS);
  if (!resolve(a))
    return fail(err, "EVM_INTERNAL", "a jump label is out of range or unbound");
  return 0;
}

int evm_write_hex(const EvmAsm *a, FILE *out, FILE *err) {
  for (size_t i = 0; i < a->size && i < EVM_CAPACITY; i++)
    fprintf(out, "%02x", a->code[i]);
  fputc('\n', out);
  if (fflush(out) != 0 || ferror(out))
    return fail(err, "EVM_IO", "cannot write the bytecode");
  return 0;
}

/* ---- the target ---- */

/* The runtime of the anchor contract. Chunk 5 writes the entries of SPEC
 * section 7 here. Until then the runtime has no entry, so every call
 * reverts. */
static void runtime(EvmAsm *a) {
  evm_dispatch_head(a);
  evm_revert_block(a);
}

int anchor_evm_write(const AnchorContract *contract, AnchorPart part, FILE *out, FILE *err) {
  if (contract == NULL)
    return fail(err, "EVM_USAGE", "no contract");
  if (contract->members < 1)
    return fail(err, "EVM_LIMIT", "a contract needs at least 1 member, got 0");
  EvmAsm body;
  EvmAsm creation;
  evm_init(&body);
  evm_init(&creation);
  runtime(&body);
  int bad = evm_finish(&body, err);
  if (bad)
    return bad;
  if (body.size > EVM_RUNTIME_MAX)
    return fail(err, "EVM_SIZE", "the runtime has %zu bytes, the limit is %d", body.size,
                EVM_RUNTIME_MAX);
  switch (part) {
    case ANCHOR_PART_RUNTIME:
      return evm_write_hex(&body, out, err);
    case ANCHOR_PART_CREATION:
      evm_creation(&creation, &body);
      bad = evm_finish(&creation, err);
      return bad ? bad : evm_write_hex(&creation, out, err);
  }
  return fail(err, "EVM_USAGE", "unknown part %d", (int)part);
}
