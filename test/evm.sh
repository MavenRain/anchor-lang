#!/bin/sh
# Back end tests of anchorc, run by make test after make: the keccak
# vectors and the bytes of the contract with no entry (src/evm.c), which
# fix the PUSH widths and the PUSH2 label offsets of both passes.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
tool=$root/build/evmtool
out=$root/build/test
mkdir -p "$out"
failures=0

pass() { printf 'ok   %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1"; failures=$((failures + 1)); }

# same NAME GOT WANT
same() {
  if [ "$2" = "$3" ]; then pass "$1"; else fail "$1: got $2, want $3"; fi
}

same 'keccak256 of the empty string' "$("$tool" keccak '')" \
  c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470
same 'selector of transfer(address,uint256)' "$("$tool" keccak 'transfer(address,uint256)' | cut -c1-8)" \
  a9059cbb

# Runtime of 1 member and 1 candidate: PUSH1 4, CALLDATASIZE, LT, PUSH2 46,
# JUMPI, PUSH0, CALLDATALOAD, PUSH1 0xe0, SHR; then DUP1, PUSH4 selector, EQ,
# PUSH2 entry, JUMPI for anchor (byte 50), verify (byte 69) and cast (byte
# 108); then the revert block at byte 46.
head=6004361061002e575f3560e01c8063eecdf92714610032578063382262fc14610045578063738198b41461006c575b5f5ffd
runtime=$("$tool" runtime 1 1)
case $runtime in
  "$head"*) pass 'runtime dispatch of 1 member and 1 candidate' ;;
  *) fail "runtime dispatch: got $runtime" ;;
esac
same 'runtime of 1 member and 1 candidate' "$runtime" \
  6004361061002e575f3560e01c8063eecdf92714610032578063382262fc14610045578063738198b41461006c575b5f5ffd5b503461002e576024361061002e5761002e565b503461002e576044361061002e576004355f5260243560205260405f205415155f5260205ff35b503461002e576024361061002e576004355f811161002e57335f5260205f2054801561002e575f01805480546001900390558190558054600101905500
# Creation: CALLVALUE, PUSH2 94, JUMPI; the check of the argument words
# against PUSH2 END, the size of the creation code; CODECOPY of the words to
# 0x20; the member loop at byte 27; the count of candidate 0; the copy of the
# runtime; the revert block at byte 94; the runtime at byte 98.
same 'creation of 1 member and 1 candidate' "$("$tool" creation 1 1)" \
  3461005e57602061010c0138141561005e57602061010c6020395f5b80602002602001518060a01c61005e57801561005e575f5260205f20805461005e578160010190556001018060011161001b575060015f5560aa806100625f395ff35b5f5ffd6004361061002e575f3560e01c8063eecdf92714610032578063382262fc14610045578063738198b41461006c575b5f5ffd5b503461002e576024361061002e5761002e565b503461002e576044361061002e576004355f5260243560205260405f205415155f5260205ff35b503461002e576024361061002e576004355f811161002e57335f5260205f2054801561002e575f01805480546001900390558190558054600101905500

# limit_is NAME MEMBERS CANDIDATES: exit 1 with EVM_LIMIT.
limit_is() {
  "$tool" runtime "$2" "$3" > /dev/null 2> "$out/evm.err"
  status=$?
  case $(cat "$out/evm.err") in
    'anchorc: EVM_LIMIT: -: '*) prefix_ok=1 ;;
    *) prefix_ok=0 ;;
  esac
  if [ "$status" -eq 1 ] && [ "$prefix_ok" -eq 1 ]; then pass "$1 is EVM_LIMIT"; else fail "$1: exit $status, stderr: $(cat "$out/evm.err")"; fi
}
limit_is 'zero members' 0 1
limit_is 'zero candidates' 1 0

if [ "$failures" -eq 0 ]; then echo "evm.sh: all passed"; exit 0; fi
echo "evm.sh: $failures failed"
exit 1
