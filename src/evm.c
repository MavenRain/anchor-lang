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
  fprintf(err, "anchorc: %s: -: ", code);
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
  evm_bind(a, EVM_LABEL_END);
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

/* Storage (SPEC section 7): the count of candidate c is slot c; the ballot
 * of member position i is slot K + i; the member slot of an address a is
 * keccak256(a), i + 1 for the member at position i and 0 for each other
 * address; the pair slot of (h, t) is keccak256(h . t), 1 when the log
 * holds the pair. The two keccak inputs have different sizes (32 and 64
 * bytes). Memory 0 to 0x1f is the keccak input of a member slot. */
enum { MEM_ARGUMENTS = 0x20 };

enum { LABEL_ANCHOR = EVM_LABEL_TARGET, LABEL_VERIFY, LABEL_CAST, LABEL_MEMBER };

typedef struct {
  const char *signature;
  const char *inputs;
  const char *outputs;  /* "-": no output */
  EvmLabel label;
} Entry;

/* The entries of SPEC section 7, in the order of the dispatch. */
static const Entry ENTRIES[] = {
  {"anchor(bytes32)", "bytes32", "uint256", LABEL_ANCHOR},
  {"verify(bytes32,uint256)", "bytes32,uint256", "uint256", LABEL_VERIFY},
  {"cast(uint256)", "uint256", "-", LABEL_CAST}
};

/* word -> keccak256(word), the member slot of an address. */
static void member_slot(EvmAsm *a) {
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_MSTORE);
  evm_push(a, 0x20);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_SHA3);
}

/* anchor(bytes32) reverts until chunk 5b adds its guards, the pair slot
 * and the Anchored log. */
static void anchor(EvmAsm *a) {
  evm_entry(a, LABEL_ANCHOR, 1);
  evm_jump(a, EVM_LABEL_REVERT);
}

/* verify(bytes32 h, uint256 t): 1 when the pair slot of (h, t) is set,
 * else 0. No guard. */
static void verify(EvmAsm *a) {
  evm_entry(a, LABEL_VERIFY, 2);
  evm_argument(a, 0);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_MSTORE);
  evm_argument(a, 1);
  evm_push(a, 0x20);
  evm_op(a, OP_MSTORE);
  evm_push(a, 0x40);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_SHA3);
  evm_op(a, OP_SLOAD);
  evm_op(a, OP_ISZERO);
  evm_op(a, OP_ISZERO);
  evm_return_top(a);
}

/* cast(uint256 c): the ballot of the caller moves from old to c, the count
 * of old goes down by 1 and the count of c goes up by 1. The guards: c is
 * less than K, and the caller is a member. The O6 guard reads the outcome
 * table, so chunk 5b adds it. */
static void cast(EvmAsm *a, size_t candidates) {
  evm_entry(a, LABEL_CAST, 1);
  evm_argument(a, 0);                /* c */
  evm_push(a, candidates - 1);
  evm_op(a, OP_DUP2);
  evm_op(a, OP_GT);
  evm_revert_if(a);                  /* c > K - 1 */
  evm_op(a, OP_CALLER);
  member_slot(a);
  evm_op(a, OP_SLOAD);               /* c, i + 1 */
  evm_op(a, OP_DUP1);
  evm_op(a, OP_ISZERO);
  evm_revert_if(a);                  /* the caller is not a member */
  evm_push(a, candidates - 1);
  evm_op(a, OP_ADD);                 /* c, the ballot slot K + i */
  evm_op(a, OP_DUP1);
  evm_op(a, OP_SLOAD);               /* c, ballot slot, old */
  evm_op(a, OP_DUP1);
  evm_op(a, OP_SLOAD);
  evm_push(a, 1);
  evm_op(a, OP_SWAP1);
  evm_op(a, OP_SUB);                 /* c, ballot slot, old, count of old - 1 */
  evm_op(a, OP_SWAP1);
  evm_op(a, OP_SSTORE);              /* c, ballot slot */
  evm_op(a, OP_DUP2);
  evm_op(a, OP_SWAP1);
  evm_op(a, OP_SSTORE);              /* c */
  evm_op(a, OP_DUP1);
  evm_op(a, OP_SLOAD);
  evm_push(a, 1);
  evm_op(a, OP_ADD);
  evm_op(a, OP_SWAP1);
  evm_op(a, OP_SSTORE);              /* the count of c + 1 */
  evm_op(a, OP_STOP);
}

static void runtime(EvmAsm *a, const AnchorContract *contract) {
  evm_dispatch_head(a);
  for (size_t e = 0; e < sizeof ENTRIES / sizeof ENTRIES[0]; e++)
    evm_dispatch(a, ENTRIES[e].signature, ENTRIES[e].label);
  evm_revert_block(a);  /* an unknown selector falls through to here */
  anchor(a);
  verify(a);
  cast(a, contract->candidates);
}

/* The constructor (O5, O10). The creation code ends with one address word
 * for each member position, and nothing after them. Each word must be a
 * nonzero address that no earlier word repeats. The member slot of each
 * address gets its position + 1, and the count of candidate 0 gets
 * members. Each ballot stays the zero word, which is candidate 0. */
static void constructor(EvmAsm *a, unsigned members) {
  unsigned long bytes = 32ul * members;
  evm_op(a, OP_CALLVALUE);
  evm_revert_if(a);
  evm_push(a, bytes);
  evm_push_label(a, EVM_LABEL_END);
  evm_op(a, OP_ADD);
  evm_op(a, OP_CODESIZE);
  evm_op(a, OP_EQ);
  evm_op(a, OP_ISZERO);
  evm_revert_if(a);                  /* not members argument words */
  evm_push(a, bytes);
  evm_push_label(a, EVM_LABEL_END);
  evm_push(a, MEM_ARGUMENTS);
  evm_op(a, OP_CODECOPY);
  evm_op(a, OP_PUSH0);               /* i */
  evm_jumpdest(a, LABEL_MEMBER);
  evm_op(a, OP_DUP1);
  evm_push(a, 0x20);
  evm_op(a, OP_MUL);
  evm_push(a, MEM_ARGUMENTS);
  evm_op(a, OP_ADD);
  evm_op(a, OP_MLOAD);               /* i, word */
  evm_op(a, OP_DUP1);
  evm_push(a, 0xa0);
  evm_op(a, OP_SHR);
  evm_revert_if(a);                  /* not an address */
  evm_op(a, OP_DUP1);
  evm_op(a, OP_ISZERO);
  evm_revert_if(a);                  /* the zero address */
  member_slot(a);                    /* i, member slot */
  evm_op(a, OP_DUP1);
  evm_op(a, OP_SLOAD);
  evm_revert_if(a);                  /* an address that an earlier word gave */
  evm_op(a, OP_DUP2);
  evm_push(a, 1);
  evm_op(a, OP_ADD);
  evm_op(a, OP_SWAP1);
  evm_op(a, OP_SSTORE);              /* i */
  evm_push(a, 1);
  evm_op(a, OP_ADD);
  evm_op(a, OP_DUP1);
  evm_push(a, members);
  evm_op(a, OP_GT);
  evm_jump_if(a, LABEL_MEMBER);      /* while i + 1 < members */
  evm_op(a, OP_POP);
  evm_push(a, members);
  evm_op(a, OP_PUSH0);
  evm_op(a, OP_SSTORE);              /* the tally (members, 0, ..., 0) */
}

int anchor_evm_write(const AnchorContract *contract, AnchorPart part, FILE *out, FILE *err) {
  if (contract == NULL)
    return fail(err, "EVM_USAGE", "no contract");
  if (contract->members < 1)
    return fail(err, "EVM_LIMIT", "a contract needs at least 1 member, got 0");
  if (contract->candidates < 1)
    return fail(err, "EVM_LIMIT", "a contract needs at least 1 candidate, got 0");
  EvmAsm body;
  EvmAsm creation;
  evm_init(&body);
  evm_init(&creation);
  runtime(&body, contract);
  int bad = evm_finish(&body, err);
  if (bad)
    return bad;
  if (body.size > EVM_RUNTIME_MAX)
    return fail(err, "EVM_SIZE", "the runtime has %zu bytes, the limit is %d", body.size,
                EVM_RUNTIME_MAX);
  constructor(&creation, contract->members);
  evm_creation(&creation, &body);
  bad = evm_finish(&creation, err);
  if (bad)
    return bad;
  unsigned long initcode = creation.size + 32ul * contract->members;
  if (initcode > EVM_INITCODE_MAX)
    return fail(err, "EVM_SIZE", "the creation code and %u member words have %lu bytes, the limit is %d (EIP-3860)",
                contract->members, initcode, EVM_INITCODE_MAX);
  return evm_write_hex(part == ANCHOR_PART_RUNTIME ? &body : &creation, out, err);
}

int anchor_abi_write(const AnchorContract *contract, FILE *out, FILE *err) {
  if (contract == NULL)
    return fail(err, "EVM_USAGE", "no contract");
  fprintf(out, "constructor inputs address[%u]\n", contract->members);
  for (size_t e = 0; e < sizeof ENTRIES / sizeof ENTRIES[0]; e++) {
    unsigned char digest[32] = {0};
    anchor_keccak256((const unsigned char *)ENTRIES[e].signature, strlen(ENTRIES[e].signature), digest);
    fprintf(out, "entry %s selector %02x%02x%02x%02x inputs %s outputs %s\n", ENTRIES[e].signature,
            digest[0], digest[1], digest[2], digest[3], ENTRIES[e].inputs, ENTRIES[e].outputs);
  }
  if (fflush(out) != 0 || ferror(out))
    return fail(err, "EVM_IO", "cannot write the entries");
  return 0;
}
