#!/bin/sh
# Checker tests of anchorc, run by make test after make (SPEC section 10,
# chunk 3): the prelude, the example programs and the mutants in
# examples/mutants. Files go to build/test. Each run stays far under 4 GB:
# the arena of one run takes at most ANCHOR_ARENA_MAX (src/syntax.h).
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
anchorc=$root/build/anchorc
programs=$root/examples/programs
mutants=$root/examples/mutants
out=$root/build/test
mkdir -p "$out"
failures=0

pass() { printf 'ok   %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1"; failures=$((failures + 1)); }


# refuse NAME CODE DEF SUFFIX ARGS...: exit 1, stderr "anchorc: CODE: DEF: ..."
# that ends in SUFFIX.
refuse() {
  name=$1 code=$2 def=$3 suffix=$4
  shift 4
  "$anchorc" "$@" > /dev/null 2> "$out/check.err"
  status=$?
  err=$(cat "$out/check.err")
  case $err in
    "anchorc: $code: $def: "*"$suffix") shape=1 ;;
    *) shape=0 ;;
  esac
  if [ "$status" -eq 1 ] && [ "$shape" -eq 1 ]; then pass "$name"; else fail "$name: exit $status, stderr: $err"; fi
}


# The prelude checks: with members only, the first error is the missing
# candidates, after the whole prelude.
printf 'def members : Nat := 1\n' > "$out/members-only.anc"
refuse "the prelude checks" TYPE_SCOPE candidates "is not declared; a program defines members, candidates and rule" \
  check "$out/members-only.anc"
printf 'def candidates : Nat := 1\n' > "$out/no-members.anc"
refuse "members is the first definition" REFUSE_MEMBERS - "" check "$out/no-members.anc"

# Each program checks; the verb then exits 1 with PLANNED (chunk 4).
for p in arrow-impossibility arrow-debreu schelling-ising; do
  refuse "$p checks" PLANNED - "" check "$programs/$p.anc"
done

# SPEC section 2: a two p q side whose forkFreeze is flagNo.
refuse "fork-unfrozen is REFUSE_FORK" REFUSE_FORK rule "is flagNo" check "$mutants/fork-unfrozen.anc"

# SPEC section 2 and prelude note P1: each mutant has one defect, and the
# checker gives its code at the definition with the defect.
refuse "data-decl is REFUSE_DATA" REFUSE_DATA Color "mu lives in the prelude" check "$mutants/data-decl.anc"
refuse "rec-def is REFUSE_REC" REFUSE_REC spin "recursion lives in the prelude" check "$mutants/rec-def.anc"
refuse "prelude-name is REFUSE_NAME" REFUSE_NAME flagYes "flagYes is declared already" check "$mutants/prelude-name.anc"
refuse "core-name is REFUSE_NAME" REFUSE_NAME Hash "Hash is declared already" check "$mutants/core-name.anc"
refuse "hash-projection is TYPE_SHAPE" TYPE_SHAPE hashHead "a projection .0 of a term that is not a pair" \
  check "$mutants/hash-projection.anc"
refuse "log-match is TYPE_MATCH" TYPE_MATCH logSize "the subject is not of the family AnchorLog with 0 indices" \
  check "$mutants/log-match.anc"
refuse "rule-type is TYPE_MISMATCH" TYPE_MISMATCH rule "expected Tally -> Outcome, found Nat -> Outcome" \
  check "$mutants/rule-type.anc"

# SPEC section 2: the surface has no axiom form, so the parser refuses an
# axiom before the checker can give REFUSE_AXIOM.
printf 'axiom x : Nat\n' > "$out/axiom.anc"
refuse "an axiom is PARSE_EXPECT" PARSE_EXPECT - "expected 'def' or 'mu', found 'axiom'" check "$out/axiom.anc"

# A policy or its freeze flag may branch on the tally. Every branch must
# freeze, on either side of two. The match variant also exercises a stuck
# match whose arm contains a case, followed by policyForkFreeze.
for form in policy-case policy-match flag-case; do
  for bad in neither zero one; do
    flag0=flagYes flag1=flagYes
    case $bad in zero) flag0=flagNo ;; one) flag1=flagNo ;; esac
    for side in p q; do
      fixture=$out/fork-$form-$bad-$side.anc
      cat > "$fixture" <<EOF
def members : Nat := 1
def safe : Policy := mkPolicy allow nonZero blockTime 0 1 flagYes
def a : Policy := mkPolicy allow nonZero blockTime 0 1 $flag0
def b : Policy := mkPolicy allow nonZero blockTime 0 2 $flag1
def candidates : Candidates := consPolicy a (lastPolicy b)
EOF
      case $form in
        policy-case)
          cat >> "$fixture" <<'EOF'
def choose : Tally -> Policy := fun (t : Tally) =>
  case natEq (tallyCount 0 t) 0 with
  | 0 (u : prod ()) => a
  | 1 (u : prod ()) => b
EOF
          ;;
        policy-match)
          cat >> "$fixture" <<'EOF'
def choose : Tally -> Policy := fun (t : Tally) =>
  match t as w in Tally return Policy with
  | mkTally count =>
    case natEq (count 0) 0 with
    | 0 (u : prod ()) => a
    | 1 (u : prod ()) => b
EOF
          ;;
        flag-case)
          cat >> "$fixture" <<EOF
def freeze : Tally -> Flag := fun (t : Tally) =>
  case natEq (tallyCount 0 t) 0 with
  | 0 (u : prod ()) => $flag0
  | 1 (u : prod ()) => $flag1
def choose : Tally -> Policy := fun (t : Tally) =>
  mkPolicy allow nonZero blockTime 0 1 (freeze t)
EOF
          ;;
      esac
      left='choose t' right=safe
      if [ "$side" = q ]; then left=safe right='choose t'; fi
      printf 'def rule : Tally -> Outcome := fun (t : Tally) => two (%s) (%s)\n' \
        "$left" "$right" >> "$fixture"
      if [ "$bad" = neither ]; then
        refuse "fork $form $bad $side checks" PLANNED - "" check "$fixture"
      else
        refuse "fork $form $bad $side refuses" REFUSE_FORK rule \
          "policyForkFreeze $side of a two p q outcome is flagNo" check "$fixture"
      fi
    done
  done
done

# Erased inputs can construct types, including through eliminators.
# They still cannot determine a run-time result or escape a type check.
cat > "$out/erased-types.anc" <<'EOF'
def members : Nat := 1
def Id : (0 A : Type 0) -> Type 0 := fun (0 A : Type 0) => A
def idNat : Id Nat := 0
def typeArg : (0 A : Type 0) -> (F : Type 0 -> Nat) -> Nat :=
  fun (0 A : Type 0) (F : Type 0 -> Nat) => F A
def CaseType : (0 f : Flag) -> Type 0 := fun (0 f : Flag) =>
  case f with
  | 0 (u : prod ()) => Nat
  | 1 (u : prod ()) => Flag
def caseNat : CaseType flagNo := 0
def MatchType : (0 v : Verdict) -> Type 0 := fun (0 v : Verdict) =>
  match v as w in Verdict return Type 0 with
  | allow => Nat
  | deny => Flag
def matchFlag : MatchType deny := flagYes
def candidates : Candidates := lastPolicy (mkPolicy allow nonZero blockTime 0 1 flagYes)
def rule : Tally -> Outcome := fun (t : Tally) => none
EOF
refuse "erased inputs construct types" PLANNED - "" check "$out/erased-types.anc"

for use in direct application case match projection mismatch; do
  fixture=$out/erased-$use.anc
  printf 'def members : Nat := 1\n' > "$fixture"
  case $use in
    direct)
      printf 'def leak : (0 x : Nat) -> Nat := fun (0 x : Nat) => x\n' >> "$fixture"
      ;;
    application)
      cat >> "$fixture" <<'EOF'
def leak : (0 x : Nat) -> (F : Nat -> Nat) -> Nat :=
  fun (0 x : Nat) (F : Nat -> Nat) => F x
EOF
      ;;
    case)
      cat >> "$fixture" <<'EOF'
def leak : (0 f : Flag) -> Nat := fun (0 f : Flag) =>
  case f with
  | 0 (u : prod ()) => 0
  | 1 (u : prod ()) => 1
EOF
      ;;
    match)
      cat >> "$fixture" <<'EOF'
def leak : (0 v : Verdict) -> Nat := fun (0 v : Verdict) =>
  match v as w in Verdict return Nat with
  | allow => 0
  | deny => 1
EOF
      ;;
    projection)
      cat >> "$fixture" <<'EOF'
def leak : ((0 n : Nat) * Nat) -> Nat :=
  fun (p : (0 n : Nat) * Nat) => p.0
EOF
      ;;
    mismatch)
      printf 'def leak : (0 A : Type 0) -> Type 0 := fun (0 A : Type 0) => 0\n' >> "$fixture"
      ;;
  esac
  expected=TYPE_ERASED
  if [ "$use" = mismatch ]; then expected=TYPE_MISMATCH; fi
  refuse "erased $use is refused" "$expected" leak "" check "$fixture"
done

if [ "$failures" -ne 0 ]; then
  printf '%s checker test(s) failed\n' "$failures"
  exit 1
fi
printf 'check.sh: all passed\n'
