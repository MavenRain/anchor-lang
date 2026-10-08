#!/bin/sh
# Back-end tests of anchorc, run by make test after make (SPEC section 10,
# chunk 5a): abi of each example program gives its golden text, each
# selector is the first 4 bytes of keccak256 of its signature, build writes
# lowercase hex, the runtime holds each selector, the creation code ends
# with the runtime code, --runtime gives the runtime code only, each mutant
# keeps its code under abi and build, and EIP-3860 bounds the creation code
# and the member words. A bytecode write failure exits 2 and removes the
# incomplete output. Files go to build/test.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
anchorc=$root/build/anchorc
tool=$root/build/evmtool
programs=$root/examples/programs
mutants=$root/examples/mutants
out=$root/build/test
mkdir -p "$out"
failures=0

pass() { printf 'ok   %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1"; failures=$((failures + 1)); }

entries='entry anchor(bytes32) selector eecdf927 inputs bytes32 outputs uint256
entry verify(bytes32,uint256) selector 382262fc inputs bytes32,uint256 outputs uint256
entry cast(uint256) selector 738198b4 inputs uint256 outputs -'

# abi_is PROG MEMBERS: exit 0 and stdout is the golden text.
abi_is() {
  "$anchorc" abi "$programs/$1" > "$out/abi.out" 2> "$out/abi.err"
  status=$?
  printf 'constructor inputs address[%s]\n%s\n' "$2" "$entries" > "$out/want.txt"
  if [ "$status" -eq 0 ] && cmp -s "$out/abi.out" "$out/want.txt"; then
    pass "abi of $1"
  else
    fail "abi of $1: exit $status, stderr: $(cat "$out/abi.err")"
    diff "$out/want.txt" "$out/abi.out"
  fi
}
abi_is arrow-debreu.anc 3
abi_is arrow-impossibility.anc 2
abi_is schelling-ising.anc 2

# Each selector is the first 4 bytes of keccak256 of its signature.
for signature in 'anchor(bytes32)' 'verify(bytes32,uint256)' 'cast(uint256)'; do
  got=$(awk -v s="$signature" '$2 == s { print $4 }' "$out/abi.out")
  want=$("$tool" keccak "$signature" | cut -c1-8)
  if [ -n "$got" ] && [ "$got" = "$want" ]; then pass "selector of $signature is $got"; else fail "selector of $signature: got $got, want $want"; fi
done

# hex_ok TEXT: 1 when TEXT is a nonempty even run of lowercase hex digits.
hex_ok() {
  case $1 in
    '' | *[!0-9a-f]*) echo 0 ;;
    *) echo $((1 - ${#1} % 2)) ;;
  esac
}

for f in "$programs"/*.anc; do
  name=$(basename "$f")
  "$anchorc" table "$f" > "$out/table.out"
  members=$(awk '/^members/ { print $2 }' "$out/table.out")
  candidates=$(awk '/^candidates/ { print $2 }' "$out/table.out")
  rm -f "$out/creation.hex" "$out/runtime.hex"
  "$anchorc" build "$f" -o "$out/creation.hex" 2> "$out/build.err"
  first=$?
  "$anchorc" build "$f" --runtime -o "$out/runtime.hex" 2>> "$out/build.err"
  second=$?
  creation=$(cat "$out/creation.hex" 2> /dev/null)
  runtime=$(cat "$out/runtime.hex" 2> /dev/null)
  if [ "$first" -eq 0 ] && [ "$second" -eq 0 ]; then pass "build of $name exits 0"; else fail "build of $name: exit $first and $second, stderr: $(cat "$out/build.err")"; fi
  if [ "$(hex_ok "$creation")" -eq 1 ] && [ "$(hex_ok "$runtime")" -eq 1 ]; then pass "build of $name writes lowercase hex"; else fail "build of $name: not lowercase hex"; fi
  held=1
  for selector in eecdf927 382262fc 738198b4; do
    case $runtime in *"63$selector"*) ;; *) held=0 ;; esac
  done
  if [ "$held" -eq 1 ]; then pass "runtime of $name holds each selector"; else fail "runtime of $name: a selector is missing"; fi
  case $creation in
    ?*"$runtime") pass "creation of $name ends with the runtime" ;;
    *) fail "creation of $name does not end with the runtime" ;;
  esac
  if [ "$runtime" = "$("$tool" runtime "$members" "$candidates")" ]; then pass "--runtime of $name gives the runtime code only"; else fail "--runtime of $name is not the runtime of $members members and $candidates candidates"; fi
  if [ "$creation" = "$("$tool" creation "$members" "$candidates")" ]; then pass "creation of $name is the contract of $members members and $candidates candidates"; else fail "creation of $name differs from evmtool"; fi
done

# Each mutant gives the same code under abi and build as under check, and
# build writes no file.
for f in "$mutants"/*.anc; do
  rm -f "$out/mutant.hex"
  "$anchorc" check "$f" > /dev/null 2> "$out/check.err"
  "$anchorc" abi "$f" > /dev/null 2> "$out/abi.err"
  abi_status=$?
  "$anchorc" build "$f" -o "$out/mutant.hex" 2> "$out/build.err"
  build_status=$?
  want=$(cut -d: -f2 "$out/check.err")
  abi=$(cut -d: -f2 "$out/abi.err")
  build=$(cut -d: -f2 "$out/build.err")
  same=0
  if [ -n "$want" ] && [ "$want" = "$abi" ]; then same=1; fi
  if [ "$same" -eq 1 ] && [ "$want" = "$build" ]; then same=2; fi
  if [ "$same" -eq 2 ] && [ ! -e "$out/mutant.hex" ]; then same=3; fi
  refused=0
  if [ "$abi_status" -eq 1 ] && [ "$build_status" -eq 1 ]; then refused=1; fi
  if [ "$refused" -eq 1 ] && [ "$same" -eq 3 ]; then
    pass "mutant $(basename "$f") keeps$want"
  else
    fail "mutant $(basename "$f"): exit $abi_status and $build_status, check$want, abi$abi, build$build"
  fi
done

# EIP-3860: the creation code and 32 bytes for each member word are at most
# 49152 bytes. From 256 to 2047 members each PUSH has a fixed width, so the
# creation code has one size S, and the largest contract has
# (49152 - S) / 32 members. build refuses one member more with EVM_SIZE and
# writes no file.
members_is() {
  awk -v n="$1" '/^def members/ { print "def members : Nat := " n; next } { print }' \
    "$programs/arrow-debreu.anc" > "$out/members-$1.anc"
}
"$anchorc" table "$programs/arrow-debreu.anc" > "$out/table.out"
candidates=$(awk '/^candidates/ { print $2 }' "$out/table.out")
size=$(( $("$tool" creation 1000 "$candidates" | wc -c) / 2 ))
max=$(( (49152 - size) / 32 ))
members_is "$max"
members_is $((max + 1))
rm -f "$out/max.hex" "$out/over.hex"
"$anchorc" build "$out/members-$max.anc" -o "$out/max.hex" 2> "$out/build.err"
status=$?
bytes=$(( $(tr -d '\n' < "$out/max.hex" | wc -c) / 2 + 32 * max ))
if [ "$status" -eq 0 ] && [ "$bytes" -gt $((49152 - 32)) ]; then pass "$max members build, $bytes bytes with the member words"; else fail "$max members: exit $status, $bytes bytes, stderr: $(cat "$out/build.err")"; fi
"$anchorc" build "$out/members-$((max + 1)).anc" -o "$out/over.hex" 2> "$out/build.err"
status=$?
case $(cat "$out/build.err") in
  'anchorc: EVM_SIZE: -: '*) prefix_ok=1 ;;
  *) prefix_ok=0 ;;
esac
if [ -e "$out/over.hex" ]; then prefix_ok=0; fi
if [ "$status" -eq 1 ] && [ "$prefix_ok" -eq 1 ]; then pass "$((max + 1)) members is EVM_SIZE"; else fail "$((max + 1)) members: exit $status, stderr: $(cat "$out/build.err")"; fi

# Let fopen succeed, then make the bytecode write fail. Ignore SIGXFSZ so
# stdio reports the error. Capture stderr through a pipe because regular
# files in this subshell have a zero size limit.
rm -f "$out/write-error.hex"
message=$(
  trap '' XFSZ
  ulimit -f 0
  "$anchorc" build "$programs/arrow-debreu.anc" -o "$out/write-error.hex" 2>&1
)
status=$?
case $message in
  'anchorc: EVM_IO: -: '*) prefix_ok=1 ;;
  *) prefix_ok=0 ;;
esac
if [ -e "$out/write-error.hex" ]; then prefix_ok=0; fi
if [ "$status" -eq 2 ] && [ "$prefix_ok" -eq 1 ]; then pass 'bytecode write failure exits 2 and removes output'; else fail "bytecode write failure: exit $status, stderr: $message"; fi

if [ "$failures" -eq 0 ]; then echo "build.sh: all passed"; exit 0; fi
echo "build.sh: $failures failed"
exit 1
