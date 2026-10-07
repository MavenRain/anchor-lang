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

# Runtime: PUSH1 4, CALLDATASIZE, LT, PUSH2 13, JUMPI, PUSH0, CALLDATALOAD,
# PUSH1 0xe0, SHR, then the revert block at byte 13.
runtime=6004361061000d575f3560e01c5b5f5ffd
same 'runtime with no entry' "$("$tool" runtime 1)" "$runtime"
# Creation: the revert block at byte 15, PUSH1 17 (the runtime size), the
# runtime at byte 19.
same 'creation with no entry' "$("$tool" creation 1)" "3461000f576011806100135f395ff35b5f5ffd$runtime"

"$tool" runtime 0 > /dev/null 2> "$out/evm.err"
status=$?
case $(cat "$out/evm.err") in
  'anchorc: EVM_LIMIT: '*) prefix_ok=1 ;;
  *) prefix_ok=0 ;;
esac
if [ "$status" -eq 1 ] && [ "$prefix_ok" -eq 1 ]; then pass 'zero members is EVM_LIMIT'; else fail "zero members: exit $status, stderr: $(cat "$out/evm.err")"; fi

if [ "$failures" -eq 0 ]; then echo "evm.sh: all passed"; exit 0; fi
echo "evm.sh: $failures failed"
exit 1
