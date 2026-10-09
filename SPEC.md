# anchor-lang specification (draft)

Status: draft before milestone M1. `anchor-lang` is a working name. The
compiler is `anchorc`. A source file has the suffix `.anc`.

## 1. Purpose

anchor-lang is a language for one governed anchor log: an append-only public
witness of content hashes at times, admitted only under a policy that a DAO
constitutes of itself. A program gives the members, the candidate policies
and the constitution (the choice rule). The compiler checks the program,
tabulates the constitution, and writes EVM bytecode for one contract.

Its type formers are F1 to F15 of `formers/FORMERS.md`. Its core data types
and core operations are only the types and operations of `design/DESIGN.md`
(the design). The host is TinyCC: `anchorc` is C99 and tcc builds it. The
target is EVM bytecode (creation code and runtime code) for Ethereum,
telcoin-network and other EVM chains. No other toolchain is in the build.

The design fixes what the compiler output owes (design section 7):

1. Public state is a set of hash-time pairs and nothing that can be a
   document or a balance.
2. Append is union under `admit`, and the identity otherwise.
3. Amendment is `Gov` on the policy and the identity on the pair set.
4. A fork is a disjunction of future policies over the shared prefix.

Gas, storage layout and the choice of hash function are representation.

## 2. Programs

A program is a sequence of definitions:

```
def NAME : TYPE := TERM
```

The surface is the grammar comment in `src/syntax.h` (origin: section
8), with the suffix `.anc`. The first definition is
`def members : Nat := N` with 1 <= N. The program then gives the candidate
policies and `def rule : Tally -> Outcome`.

The compiler refuses these forms in a program, each with a stable code:

| Form | Code |
|---|---|
| a data declaration (`mu`) | `REFUSE_DATA` |
| an axiom | `REFUSE_AXIOM` |
| a recursive definition (`rec`) | `REFUSE_REC` |
| a definition that uses a core name or a prelude name | `REFUSE_NAME` |
| a `two p q` outcome where `p` or `q` has `forkFreeze = flagNo` | `REFUSE_FORK` |
| `amendments` with no `amendTo`, or `amendTo` with no `amendments` | `REFUSE_AMEND` |

The checker explores `case` and `match` alternatives in rule outcomes and
their policy freeze flags. Every possible fork side must reduce to
`flagYes`. It conservatively refuses a flag or outcome whose alternatives
remain unresolved, using `REFUSE_FORK`.

Chunk 10 (O3, section 9) adds two optional program defs:
`amendments : Constitutions` and `amendTo : Policy -> Nat -> Flag`. A
program defines both or neither. A program with only one of them is
`REFUSE_AMEND` at that def ("amendments has no amendTo", "amendTo has no
amendments"). A def with a different type is `TYPE_MISMATCH`, as for
`candidates` and `rule`. The checker runs the fork check above on each
constitution `F` of `amendments`, applied to a free profile `x`, at the def
`amendments`. The message is the same as the message for `rule`. These
checks run in each verb, so `check`, `table`, `eval`, `abi` and `build`
give the same code. My choice, not ruled.

Chunk 4a adds the full fork check by tabulation (section 7). At each
tally, a `two p q` outcome with a side whose `policyForkFreeze` is
`flagNo` is `REFUSE_FORK`, and the message names the tally. The two checks
relate as follows. The chunk 3 check runs first, on each verb, and
explores each arm of `rule` under an open tally. Thus it refuses each fork
that the full check can find, and also forks at count vectors that are not
tallies. The full check is a second check of the same property on closed
outcomes. With the chunk 3 check as written, no program gets to the
`REFUSE_FORK` of the full check, so no test does. My choice, not ruled.

`REFUSE_AXIOM` cannot be reached from the parser. The surface of section 8
has no axiom form: each declaration starts with `def` or `mu`. Thus the
parser refuses `axiom x : Nat` with `PARSE_EXPECT` before the checker runs
(`test/check.sh`).

Chunk 3 checks the forks and the required names as follows. Chunk 4
tabulates the rule over the tallies and makes the full fork check.

- A fork side whose freeze flag does not normalize to a closed flag, after
  the checker explores each `case` and `match` arm of the flag, is
  `REFUSE_FORK`. My choice, not ruled.
- A stuck outcome of `rule` with no `case` or `match` to explore is
  `REFUSE_FORK`, because the checker cannot see its fork. My choice, not
  ruled.
- The checker finds `candidates` and `rule` by presence and type
  (`Candidates`, `Tally -> Outcome`), in any order after `members`. A
  missing one is `TYPE_SCOPE`. A wrong type is `TYPE_MISMATCH`
  (`examples/mutants/rule-type.anc`). My choice, not ruled.

Thus a program cannot add a data type, an unproved fact or general
recursion. Recursion comes only from `fold` (F6). The compiler writes the
target; a program cannot.

The design has no type for bytes, text, an author, an address value or a
balance. The core name set does not contain one. Thus no program can make a
projection to plaintext, a morphism to an author, or a balance monoid
(design section 6). The check is the absence of the name, not a rule.

## 3. Type formers

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses a
new host column, `tcc-evm`. `formers/tcc-evm.md` gives a status and the
evidence for each former. A status is for the checker of chunk 3. The
evaluator of chunk 4 uses the normalizer of the checker. The contract
writer of chunk 5 uses no former at run time. The compiler tabulates the
outcomes (chunk 4a), and the runtime reads them from the outcome table
(section 7).

| ID | Status on tcc-evm | Effect on this language | Open item |
|---|---|---|---|
| F1 Product | DONE | `prod`, `tuple`, `.0`, `.1`; `AnchorPair` | none |
| F2 Coproduct | DONE | `sum`, `inj`, `case`; `Flag` | none |
| F3 Option | DONE | `Option A := sum (prod (), A)`; `candidateAt` | none |
| F4 List | PARTIAL | one `mu` family for each element type: `Candidates`, `Profile` | none |
| F5 Monad | PARTIAL | over `Option` only: `optionPure`, `optionBind`, `optionMap` | none |
| F6 Algebra fold | PARTIAL | `candidatesFold` and `profileFold` in the prelude; no fold on `Nat` (prelude note P10) | none |
| F7 Algebra unfold | HOST-LIMIT | no unfold: recursion is structural only, and a program has no `def rec` | none |
| F8 Filterable filter | PARTIAL | over `Profile` only: `profileFilter` | none |
| F9 Pi, not dependent | DONE | `rule : Tally -> Outcome` | none |
| F10 Pi, dependent | DONE | erased type inputs; `amendKeepsGov` | none |
| F11 Sigma | DONE | `AnchorDAO F := (L : Aggregation F) * AnchorLog`, read by `.0` and `.1` | none |
| F12 Eq refl, symm, trans | PARTIAL | one family, `EqOutcome`: refl (`sameOutcome`), `symmOutcome`, `transOutcome`; no generic `Eq` | none |
| F13 Eq transport, cong | PARTIAL | `transportOutcome` and `congOutcome` for `EqOutcome` only; no generic `Eq`, no heterogeneous cong | none |
| F14 Universes | PARTIAL | `Type 0 : Type 1`; Pi, Sigma, product and sum types can inhabit `Type 1`; an explicit `Type 1` annotation is refused | none |
| F15 Indexed family | DONE | `EqOutcome`, `Aggregation F` | none |

## 4. Structures

- **Monad**: `optionPure`, `optionMap`, `optionBind` on `Option`.
- **Algebra**: `fold` on the prelude lists `Candidates` and `Profile`, not
  on `Nat` (prelude note P10 in section 6). The tabulation in the
  compiler visits every tally; a program does not need `unfold` (F7).
- **Filterable**: `profileFilter` on `Profile`.

## 5. Core types

| Type | Meaning (design section) | Definition |
|---|---|---|
| `Hash` | external collision-resistant digest (3) | opaque word; no literal in a program; no projection |
| `Time` | external total order; block time only (3, O1) | opaque word; read only from the chain |
| `Prod Hash Time` | one anchored pair (3) | F1 |
| `AnchorLog` | `Set (Hash x Time)` (3) | abstract core type; operations `logMember` and the insert inside `anchor` only; no eliminator |
| `Verdict` | `Allow` or `Deny` (3) | prelude sum |
| `HashDom` | accepted digest domain (3) | prelude sum; one value `nonZero` (O4) |
| `Clock` | admissible clock (3) | prelude sum; one value `blockTime` (O1) |
| `Policy` | `F.obj X` (3) | prelude record: `admit : Verdict`, `hashDom : HashDom`, `clock : Clock`, `window : Nat`, `schema : Nat`, `forkFreeze : Flag` |
| `Ballot` | one member's choice of a candidate policy (3, 5) | `Nat`, less than the number of candidates |
| `Tally` | the orbit of a profile under `act` (3) | prelude record: one count per candidate; the counts sum to `members` |
| `Outcome` | the fate at one tally (6) | prelude sum: `none`, `one p`, `two p q` |
| `Aggregation F` | `Lan (orbitProjection act) F` (3) | prelude record indexed by the rule `F` |
| `AnchorDAO` | `Sigma (L : Aggregation act F). AnchorLog` (3) | F11 |

`act` is the symmetric group on the member positions. The rule reads only
the tally, so the aggregation is anonymous in the sense of the design: it
depends on the orbit, not on who voted. Ballots are public on chain; the
design does not claim ballot privacy. The design allows a trivial group
(O8).

A `Hash` argument and the `Time` from the block are the only values that come
from outside. A program cannot make either one.

## 6. Core operations

| Operation | Type | Meaning (design section) |
|---|---|---|
| `Gov` | `Aggregation F -> Tally -> Outcome` | `orbitProjection act >>> L.functor` (4) |
| `admit` | `Aggregation F -> Tally -> Flag` | `flagYes` only for `one p` with `p.admit = Allow` (3); `none` and `two p q` give `flagNo` (6, O2) |
| `anchor` | `Hash -> AnchorDAO -> AnchorDAO` | union with `{(h, t)}`, `t` the block time, if `admit`, `Clock t` and `h` in `HashDom`; the identity otherwise (5) |
| `verify` | `Hash -> Time -> AnchorLog -> Flag` | `(h, t)` is in the log (5); `hash file = h` is checked off chain |
| `cast` | `Nat -> Ballot -> Tally -> Tally` | a member changes a ballot; the log does not change (5) |
| `amend` | `Aggregation F -> Aggregation F` | `GovPhi Phi`; M1 has only `canonicalAmendment`, the identity (4, O3) |
| `IsSelfConstituting` | `Type` | `exists L, forall X, (Gov L).obj X = F.obj X` (4); the tally set is finite, so the compiler checks it at each tally |

There is no operation whose meaning is a proper subset of the log. There is
no delete, no update and no retention operation. A retention map is a view
(design section 3) and is not in M1.

The fate of a tally is its outcome: `none` is Arrow-Impossibility, `one p`
is Arrow-Debreu, `two p q` is Schelling-Ising. The program fate report lists
the tallies of each fate (`anchorc check`, section 7). Because `REFUSE_FORK` forces `ForkFreeze`, a
Schelling-Ising tally admits nothing. No branch-local pair can exist, so the
shared prefix is the log, and `verify` reads the log (O2).

Prelude notes (chunk 2, `prelude/Prelude.anc`):

- P1. Core names. RULED 2026-10-07 (USER): the surface has no axiom
  form, so `Hash`, `Time`, `AnchorLog`, `logMember`
  (`Hash -> Time -> AnchorLog -> Flag`), `logInsert`
  (`Hash -> Time -> AnchorLog -> AnchorLog`) and `hashNonZero`
  (`Hash -> Flag`) are core names, as `Nat`, `natAdd`, `natSub`, `natEq`
  and `natLt` are. The prelude uses them and does not declare them. Chunk
  3 gives them their types.
- P2. `Flag` is `sum (prod (), prod ())`: leg 0 is `flagNo`, leg 1 is
  `flagYes`, as for `natEq`. Design `Allow` and `Deny` are `allow` and
  `deny`. My choice, not ruled.
- P3. A program gives `def candidates : Candidates := ...` after
  `members`: a list that is not empty (`consPolicy`, `lastPolicy`),
  candidate 0 first. A ballot is the number of a candidate in it. The
  prelude does not define `members`, `candidates` or `rule`. My choice,
  not ruled.
- P4. `mkPolicy` takes the fields of section 5 in their order. My
  choice, not ruled.
- P5. `Tally` holds one count function `Nat -> Nat`. It holds no proof
  that the counts sum to `members`: `cast` cannot make that proof for an
  open tally, because `Nat` has no eliminator. The compiler makes only
  tallies whose counts at candidates 0 to K-1 sum to `members` and are 0
  at the other numbers (chunk 4). My choice, not ruled.
- P6. `Profile` is the list of ballots, member position 0 first.
  `orbitProjection` counts it. `Constitution := Profile -> Outcome` is the
  type of `F`. `Outcome` holds policies, not candidate numbers. My
  choice, not ruled.
- P7. The types in the table above do not show the erased `F`. The
  prelude gives it as the first argument of `Gov`, `admit`, `anchor` and
  `amend`. `anchor` also takes the current tally and the block time:
  `(0 F : Constitution) -> Tally -> Time -> Hash -> AnchorDAO F -> AnchorDAO F`.
  My choice, not ruled.
- P8. The first argument of `cast` is the old ballot of the member, not
  the member position, because a tally does not record who voted. The
  runtime reads the old ballot from storage. My choice, not ruled.
- P9. The proof in `Aggregation F` is `L (orbitProjection x) = F x`, so
  `IsSelfConstituting` uses it with no symmetry step. `aggregationOf rule`
  is the aggregation of a program. No name makes an empty log; the
  contract starts with one (chunk 5). My choice, not ruled.
- P10. There is no `fold` on `Nat`: `Nat` is a core type with no
  eliminator, and the origin checker accepts a `def rec` only as a `match`
  on a `mu` parameter. My choice, not ruled.

## 7. Target and instance encoding

`anchorc build` writes the creation code and the runtime code as lowercase
hex. The constructor takes `members` addresses, one for each member position
(O5). Each entry reverts on a non-zero call value. A failure is `REVERT`
with empty data, so the state stays the same, which is the identity branch
of `anchor`.

| Entry | Guards | Effect |
|---|---|---|
| `anchor(bytes32)` returns `uint256` | caller is a member; `h != 0`; `admit` gives `flagYes` at the current tally (the outcome is `one p` and `p.admit` is `allow`) | when the pair slot of `(h, t)` with `t = TIMESTAMP` is 0, sets it to 1 and logs `Anchored(bytes32,uint256)`; returns `t` |
| `verify(bytes32,uint256)` returns `uint256` | none | returns 1 if the slot of `keccak256(h, t)` is set, else 0 |
| `cast(uint256)` | caller is a member; ballot less than the number of candidates; O6 when the outcomes before and after the move are both `one` (below) | changes the ballot and the tally counts |

Storage holds the member addresses, the ballots, the tally counts and the
pair slots. Nothing else. The slot numbers (chunk 5a) are my choice, not
ruled:

- The tally count of candidate `c` is slot `c`, for `c` from 0 to K - 1.
- The ballot of member position `i` is slot `K + i`. The zero word is
  candidate 0, so the constructor writes no ballot (O10).
- The member slot of an address `a` is `keccak256(a)`, with `a` as one
  32-byte word. It holds `i + 1` for the member at position `i`, else 0.
- The pair slot of `(h, t)` is `keccak256(h . t)`, two 32-byte words. It
  holds 1 when the log holds the pair.

A member slot hashes 32 bytes and a pair slot hashes 64 bytes, so `verify`
cannot read a member slot as a pair.

The constructor (O5, O10) reads `members` address words after the creation
code. It reverts when the code does not end with exactly `members` words,
when a word is not an address, when an address is zero, and when an
address occurs two times (my choice, not ruled). It writes the member slot
of each address and sets the count of candidate 0 to `members`. Thus the
tally starts at `(members, 0, ..., 0)` (O10).

The dispatch order is `anchor`, `verify`, `cast`. An unknown selector
reverts. `cast(c)` moves the ballot of the caller from `old` to `c`: the
count of `old` goes down by 1 and the count of `c` goes up by 1. Then the
O6 guard (below) compares the outcome before the move with the outcome
after the move.

`anchor(h)` reverts when the caller is not a member, when `h` is 0, and
when `admit` does not give `flagYes` at the current tally (O9). Then it
reads `t = TIMESTAMP`. When the pair slot of `(h, t)` is 0, it sets the
slot to 1 and logs `Anchored`. When the slot is 1 (the same `h` in the
same block), it does not write and it does not log (my choice, not
ruled: a second insert of a pair does not change the log). In both cases
it returns `t`. The log is `LOG2` (my choice, not ruled): topic 0 is
`keccak256("Anchored(bytes32,uint256)")`, topic 1 is `h`, and the data is
`t` as one 32-byte word. The writer pushes topic 0 with the shortest
`PUSH` that holds it (my choice, not ruled).

`anchorc abi PROG` prints one line for the constructor, then one line for
each entry in the order of the dispatch (my choice, not ruled). A selector
is 8 lowercase hex digits: the first 4 bytes of `keccak256` of the
signature. `-` is no output. The last line is the `Anchored` log: the
signature, `topic` and topic 0 as 64 lowercase hex digits, `indexed` and
the type of topic 1, then `data` and the type of the data (my choice, not
ruled). For 3 members:

    constructor inputs address[3]
    entry anchor(bytes32) selector eecdf927 inputs bytes32 outputs uint256
    entry verify(bytes32,uint256) selector 382262fc inputs bytes32,uint256 outputs uint256
    entry cast(uint256) selector 738198b4 inputs uint256 outputs -
    event Anchored(bytes32,uint256) topic fde54488b5523b3abf19b99976dd0e2c531fbcd233d0eb682c96c3a18cf6b3c1 indexed bytes32 data uint256

`anchorc build PROG [--runtime] -o OUT` writes the creation code to OUT as
lowercase hex, or the runtime code with `--runtime`. The creation code ends
with the runtime code. When the back end refuses, OUT is removed. `abi` and
`build` run after `anchor_check` and read `members` and the candidates.
Then they tabulate one time, as `table` does. A table refusal
(`TABLE_LIMIT`, `TABLE_STUCK`, `REFUSE_FORK`) stops them with its code.

Bounds of the back end (my choice, not ruled, from the EIPs): the runtime
code with the table bytes is at most 24576 bytes (EIP-170), else
`EVM_SIZE` with the message
`the runtime has B bytes, the limit is 24576`. The writer checks this
bound before it writes the table, so a large table does not start a long
loop. The creation code with the member words is at most 49152 bytes
(EIP-3860), else `EVM_SIZE` with the message
`the creation code and N member words have B bytes, the limit is 49152 (EIP-3860)`.
The writer checks the runtime bound first. With 1 member and 1
candidate, the runtime has 560 bytes and the creation code has 659 bytes
(`test/evm.sh`). The table grows with the members, so the edges are not
fixed numbers. `test/build.sh` finds the EIP-3860 edge N of
`examples/programs/arrow-debreu.anc` by bisection over 256 to 4095
members, then checks that N members pass and N + 1 members are
`EVM_SIZE`. With 2 candidates the EIP-3860 bound fails first, because
each member word has 32 bytes. Thus `test/evm.sh` tells the two bounds
apart by the message: at the EIP-170 edge N with 2 candidates, N members
give the EIP-3860 message and N + 1 members give `the runtime has`.
0 members or 0 candidates is `EVM_LIMIT`. `abi` applies no code bound. A
back-end error is `anchorc: CODE: -: message`.

The compiler tabulates `Gov` over every tally at compile time (see
`anchorc table` below). The runtime reads the outcome of the current
tally and the policy fields that a guard reads from table bytes at the
end of the runtime code, by `CODECOPY`. Each item of this list is my
choice, not ruled:

- The table bytes come after the last instruction. The rank subroutine
  ends with `JUMP`, so control never goes into the data. The table bytes
  count toward the EIP-170 bound.
- The table has three parts, each at a label: the binomial part, the rows
  and the policy records. Numbers are big-endian.
- Binomial part: for d = 1 to K - 1 and S = 0 to M, 2 bytes hold
  C(S + d - 1, d), at offset 2((d - 1)(M + 1) + S). The part is empty
  when K = 1. The writer computes a term with
  `c = 1; for j = 1..d while c <= 0xffff: c = c * (S - 1 + j) / j`
  (exact; S = 0 gives 0) and clamps it at 0xffff.
- Rows: one row for each tally, in the order of `anchorc table`, 5
  bytes: the fate (1 byte: 0 `none`, 1 `one`, 2 `two`), then `p` and `q`
  (2 bytes each, 0 when not used).
- Policy records: one record for each policy number, 9 bytes: `admit` (1
  byte: 1 `allow`, 0 `deny`), then `schema` (8 bytes). The guards read
  only these two fields: `hashDom` and `clock` have one value (O4, O1),
  `forkFreeze` is forced (O2), and no entry reads `window` (O7).
- The widths are sufficient because `TABLE_LIMIT` is 4096. There are at
  most K + 2 x rows <= 12288 policies, less than 65536. Each term that
  the runtime reads is at most the rank, which is less than 4096. `Nat`
  has 64 bits. Thus the writer needs no new bound.
- The row of the current tally is its rank in reverse lexicographic
  order. The runtime computes the rank from the K count slots:
  rank = sum for d = 1 to K - 1 of C(S_d + d - 1, d), where S_d is the
  sum of the last d counts. For K = 2 the rank is c1, and the rank of
  `(M, 0, ..., 0)` is 0. Thus storage gets no new slot.
- A rank subroutine after the last entry computes the rank. `anchor` and
  `cast` call it. Memory 0x40 to 0x5f holds the word of each `CODECOPY`;
  memory 0x00 to 0x3f holds the `keccak256` inputs of the slots. The
  writer uses 18 of its 64 labels.
- The writer refuses no table with `EVM_USAGE` (`no outcome table`), and
  a row that names a policy with no record with `EVM_INTERNAL` (`a row of
  the outcome table names no policy`). The test driver `evmtool` makes
  C(N + K - 1, K - 1) `none` rows and K `deny` policies, at most 65536
  rows.

The O6 guard of `cast` (my choice, not ruled): `cast` computes the rank
before the move and after the move. When both rows are `one`, it reverts
when the `schema` of the new policy is less than the `schema` of the old
policy. A `none` or `two` row has no schema, so the guard does not apply
to it. Thus a path of casts through a `none` tally can lower the schema.

`test/run.sh` runs the runtime code on geth `evm run --prestate`. The
prestate puts the code and the storage words (32 bytes each) in the
alloc. The block timestamp is 4660. At a `one` tally with an `allow`
policy, `anchor` returns `t`, makes 1 `LOG2` (topic 1 is `h`, the data is
`t`) and sets one more slot to 1, and then `verify(h, t)` returns 1. A
second `anchor` of the same `h` returns `t` and makes no log. `anchor`
reverts for a caller that is not a member, for `h = 0`, at a `one` tally
with a `deny` policy, at a `none` tally and at a `two` tally. The O6
cases use a variant of `arrow-debreu.anc` in which the `closedLog` policy
has schema 2, because no example program has two schemas (the test makes
the variant with awk). The cast from the tally `(1, 2)` to `(2, 1)`
reverts, and the cast from `(2, 1)` to `(1, 2)` runs. A cast between two
policies with the same schema runs, and a cast between `none` rows runs.
When no `evm` is on the PATH, `test/run.sh` prints a message and exits 0
(my choice, not ruled). `make test` runs it after `test/build.sh`.

Chunk 6 (M3) adds three test files on geth `evm` and one sourced helper.
`make test` runs them after `test/run.sh`, in the order `test/deploy.sh`,
`test/diff.sh`, `test/laws.sh`. When no `evm` is on the PATH, each one
prints a message and exits 0, as `test/run.sh` does.

- `test/evmchain.sh` has the helpers and no cases. The three test files
  source it. A deploy runs the creation code and the member words with
  `evm run --create`. A step runs one call on the receiver, on the
  storage after the last step.
- `test/deploy.sh` (13 cases) deploys each example program. The deployed
  code (the dump `code` and the last stdout line) must equal
  `anchorc build --runtime`. The storage must be M at slot 0 and the M
  member slots. The constructor guards of this section must revert on
  `evm`: too few words, too many words, a zero address, a duplicate, a
  word that is not an address and a non-zero value. One `anchor` call
  must run on the deployed state.
- `test/diff.sh` (45 cases) runs four fixed traces of calls. Each trace
  starts from a deploy and runs one `evm run` for each call. A model in
  awk predicts each call from the `anchorc table` text (the members, the
  candidates, the `admit` and the `schema` of each policy, the outcome of
  each tally) and the slot rules of this section. For each call, the
  model predicts the result (a revert or the output word), the log (none,
  or topic 0, `h` and `t`) and the full storage after the call. Each case
  compares the prediction with the `evm` result, the `LOG2` lines and the
  dump storage. Limit: the model reads the table of `anchorc`. Thus
  `test/diff.sh` checks the contract against the table, and
  `test/table.sh` checks the table.
- `test/laws.sh` (23 cases) tests the laws of the log on `evm`: the log
  only grows (monotone), a second `anchor` of the same pair changes
  nothing (idempotent), two distinct anchors commute, no call deletes a
  pair, `amend` is the identity on the log (O3) and there is no admit at
  a `two` tally.

The tests get keccak256 (topic 0 of `Anchored(bytes32,uint256)`, the test
digests `h`, the pair and member slot keys and the `amend` selectors) from
a helper contract on `evm`, `0x3660006000373660002060005260206000f3`, not
from `src/keccak.c`. The helper copies the call data to memory, hashes it
and returns the hash. For an empty output (for example of `cast`), `evm`
1.14.12 prints an empty line as stdout line 1, not `0x`; the tests show
this output as `0x`. A harness error stops the test with a `FAIL` line and
exit 1: `evm` fails, `anchorc build` or `anchorc table` fails, or the
keccak helper does not return 64 lowercase hex digits.

Rulings of chunk 6:

- a. Reference of `test/diff.sh`: the awk model above, from the
  `anchorc table` text and the slot rules of this section. My choice, not
  ruled.
- b. Traces: fixed lists (no seed), four traces, one `evm run` for each
  call. My choice, not ruled.
- c. `amend`: M1 has no `amend` entry (O3). `test/laws.sh` shows that
  `anchorc abi` prints no `amend` line for each example program, that the
  selectors of `amend(uint256)` and `amend()` revert, and that the storage
  does not change. My choice, not ruled.
- d. Deploy test: `evm run --create`, then the compare of the deployed
  code with `anchorc build --runtime` and of the storage with the count
  slot and the member slots. Adopted. The constructor guard cases and the
  `anchor` call on the deployed state are my choice, not ruled.
- e. Files: three test files and one sourced helper, and no change to
  `test/run.sh` (its 19 cases stay). My choice, not ruled.

`anchorc table PROG` (chunk 4a) prints the table in this stable text form,
one item on each line:

- `members M`, then `candidates K`.
- `policy N FORM` for each policy: the candidates in order (0 to K-1),
  then each other policy of an outcome, in the order of first use. FORM is
  the closed normal form in the canonical form of the printer. Two
  policies with the same normal form have the same number.
- `tally c0 ... cK-1 : none`, `: one P` or `: two P Q` for each tally,
  where P and Q are policy numbers. The tallies are the count vectors
  whose sum is M, in reverse lexicographic order: `(M, 0, ..., 0)` first
  (the tally of the O10 proposal) and `(0, ..., 0, M)` last.

The count function of a tally gives `ci` at candidate i and 0 at the other
numbers (prelude note P5). Each outcome must reduce to `none`, `one p` or
`two p q` with closed policies, else `TABLE_STUCK`. A table holds at most
`TABLE_LIMIT` = 4096 tallies, C(M + K - 1, K - 1); a larger one is
`TABLE_LIMIT`. With 2 candidates, 4095 members give the largest table.
My choice, not ruled. The arena of one run (`ANCHOR_ARENA_MAX`, 256 MiB)
also bounds the memory of the table.

`anchorc check PROG` (chunk 4b) prints the fate report of section 6 from
the same table, in this stable text form, one item on each line:

- `members M`, then `candidates K`, as in the table.
- `fate none N`, then the N tallies whose outcome is `none`, one
  `tally c0 ... cK-1` line for each, in the order of the table. Then
  `fate one N` and `fate two N` in the same form.

The report gives no policy numbers. `anchorc table` gives them. `check`
tabulates one time, so its refusals include the codes of the table
(`TABLE_LIMIT`, `TABLE_STUCK`, `REFUSE_FORK`). My choice, not ruled.

Chunk 10 (O3, section 9) adds the constitutions. Constitution 0 is `rule`.
Constitution k, for k >= 1, is item k of `amendments`, so C is 1 plus the
length of `amendments`. A program has at most 8 constitutions (b5). A
larger C is `AMEND_LIMIT` at the def `amendments`. A list that does not
reduce is `TABLE_STUCK` at the def `amendments`. My choice, not ruled.

The checker tabulates constitution k >= 1 at the sorted profile of each
tally: c0 ballots of 0 first, then c1 ballots of 1, and so on. At that
profile, `constitutionOf r` gives r at the tally. A constitution that does
not factor through `orbitProjection` runs as its value at the sorted
profile. This is a limit. The evaluation of a profile of M ballots nests
one level for each ballot, so a large M is `TYPE_FUEL` at the def
`amendments` (`CHECK_DEPTH`). `examples/programs/arrow-debreu-amend.anc`
tabulates with 200 members and not with 300. With C = 1, `TABLE_LIMIT` does
not change. With C > 1, a table of more than 4096 rows, C R, is
`TABLE_LIMIT` at the def `candidates`, with the message "M members, K
candidates and C constitutions give more than 4096 rows". Each `amendTo p
k` must reduce to a closed flag, else `TABLE_STUCK` at the def `amendTo`.
In memory, the table holds one `amendTo` mask for each policy and C blocks
of R rows (`src/check.h`). My choice, not ruled.

With C = 1, `table` and `check` print the same bytes as before. With C > 1,
`anchorc table` prints `members M`, `candidates K`, `constitutions C` and
the policy lines. Then it prints `amendTo i MASK` for each policy i. MASK
has C digits, k = 0 first, and digit k is 1 when `amendTo p k` is
`flagYes`. Then, for each constitution k, it prints `constitution k` and
the tally rows of that constitution. `anchorc check` prints `members M`,
`candidates K` and `constitutions C`. Then, for each constitution k, it
prints `constitution k` and the fate blocks of that constitution.

Chunk 11 gives such a program a contract (O3 b4 to b8). Slot K + M holds
the current constitution c. The zero word is constitution 0, so the
constructor writes no new slot. The code table has C R rows: the row of a
tally under c is c R + rank, at the offset of the rows plus 5 (c R +
rank). Each policy record has 10 bytes: admit, schema and the `amendTo`
mask (bit k is 1 when `amendTo p k` is `flagYes`). `anchor` and `cast`
read the row under c. The entry `amend(uint256 k)` makes k the current
constitution. Its guards: the caller is a member, k is less than C, the
row of the current tally under c has the fate one, and bit k of the mask
of its policy is 1. When the row under k has the fate one too, its schema
is not less than the schema of the row under c (O6, b6). Then slot K + M
becomes k and the log `Amended(uint256)` gives k as its data (b7).
`anchorc abi` prints the amend entry last and, after the `Anchored` line,
`event Amended(uint256) topic T data uint256`. When C = 1, the entry, the
slot, the mask and the log do not exist, so the bytes do not change (b8,
D0). `EVM_LIMIT` (EIP-170) bounds the bytes. For
`arrow-debreu-amend.anc`, the runtime has 952 bytes and the creation code
has 1051 bytes.

My choice, not ruled: when k is the current constitution, `amend` stops
after the member check, before the other guards, with no slot write and
no log (the b3 reading). My choice, not ruled: k >= C reverts. My choice,
not ruled: the data of `Amended` is k and k is not indexed, so the abi
line has no `indexed` part. My choice, not ruled: `amend` is the last
entry of the dispatch. My choice, not ruled: the mask is byte 9 of the
10-byte record. My choice, not ruled: the writer refuses C outside 1 to 8
with `EVM_LIMIT`, a second guard after `AMEND_LIMIT`. My choice, not
ruled: the contract does not copy the masks, because the arena of the
checked program owns them. My choice, not ruled: the amend cases are in
`test/run.sh` on prestates (a prestate with slot K + M = 1 is the storage
after `amend(1)`), and `test/diff.sh` and `test/laws.sh` keep their cases.

Chunk 12 tests `amend` on the chain (row 12). `test/laws.sh` deploys
`arrow-debreu-amend.anc` and tests two laws. `amend` is the identity on the
pairs: `amend(1)` changes only slot K + M, and `amend(0)` after it gives the
storage before it. The canonical `amend(c)`, with c the current
constitution, changes nothing and makes no log (b3). The cases of the 3
programs with C = 1, whose `amend` selectors revert, stay (b8). The awk
model of `test/diff.sh` reads `constitutions`, the `amendTo` masks and the
row block of each constitution from `anchorc table`, and it reads the row of
a tally under the constitution in slot K + M. Trace E calls `anchor`,
`cast`, `verify` and `amend` (18 calls). `test/deploy.sh` deploys the new
program and runs `amend(1)` on the deployed state. My choice, not ruled:
trace E runs on `arrow-debreu-amend.anc` with the schema 2 in `closedLog`,
so that the O6 guard of `amend` refuses one call. My choice, not ruled: in
the chain tests, the line of a log with one topic is `log TOPIC0 DATA`.
The chain reader preserves all topics, so an extra topic fails the
differential comparison. `test/deploy.sh` has a LOG3 regression case.

`anchorc eval PROG NAME` (chunk 4b) prints the normal form of the def NAME
of the prelude or PROG, including a prelude `def rec`, in the canonical
form of the printer, then exits 0. A recursive def prints its normalized
body; recursive calls on a neutral argument keep their name.
The compiler names each bound variable of the normal form (`anchorq0x0`).
`eval` does not tabulate, so a program whose table is `TABLE_LIMIT` still
evaluates. A NAME that is not declared, or that is not a def (a `mu`, a
constructor or a core name), is `EVAL_NAME`. My choice, not ruled. The fuel
(`CHECK_FUEL`) and the arena bound `eval` as they bound the checker.

Types, `Eq` proofs and universes erase. The `Anchored` log is
representation for indexers. The design model is not an event log.

## 8. Host and target

- Host: TinyCC (`/Users/oobi/.local/bin/tcc`, 0.9.28rc). The compiler is
  C99; `cc -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only` is a
  second syntax check.
- Origins: the type formers and this template come from lang-template at
  ad3cb92. The compiler starts from the escrow-lang TinyCC compiler at
  cfe211b (lexer, parser, printer, arena, diagnostics, keccak, EVM
  assembler). RULED 2026-10-07 (USER): a standalone repo first. M5 ported
  the host to lang-template as `hosts/tcc-evm-anchor` (lang-template 5bec924).
- Gate tools: geth `evm` (1.14.12) runs the bytecode (`test/run.sh`,
  `test/deploy.sh`, `test/diff.sh` and `test/laws.sh`); Foundry `cast`
  gives calldata and selectors as an oracle. The build does not need them.
  `make test` needs `evm` on the PATH to run these four tests; without it,
  each one exits 0 with a message.
- `probe/CAPABILITY.md` records what the host can do now: the TinyCC
  build, the EVM assembler, the checker and its codes, and the gates.
  Nothing is PLANNED after chunk 6. The contract writer of chunk 5 is in
  `src/evm.c`.

## 9. Open items

- O1. Time source. RULED 2026-10-07 (USER): block time only. `anchor` takes
  `h`; `t` is `block.timestamp`; `Clock` has one value, `blockTime`, so
  `Clock t` is always true.
- O2. Fork fate. RULED 2026-10-07 (USER): ForkFreeze is forced. A
  Schelling-Ising contract admits nothing new, and `verify` reads the whole
  log, which is then the shared prefix. RULED 2026-10-08 (USER): the fork
  is a `two p q` outcome at a tally.
- O3. `amend`. The design forces `GovPhi canonicalAmendment L = Gov L`. M1
  has only the canonical amendment, so there is no `amend` entry.
  `test/laws.sh` tests the identity on the log: the `amend` selectors
  revert and the storage does not change. RULED 2026-10-08 (USER): a list
  of constitutions at compile time and a guarded `amend(uint256)` entry
  (M6, chunks 10 to 12). The defaults below are RULED 2026-10-08 (USER).
  - b1. Surface. `rule` is constitution 0, the genesis. The optional def
    `amendments : Constitutions` gives constitutions 1 to C - 1. It is a
    list of `Profile -> Outcome` that is not empty. Each constitution has
    the same members and the same candidates. With no `amendments`, C = 1.
  - b2. Who may amend. Only a member may call `amend(k)`. The rule is read
    off the policy, as for `admit` (`design/DESIGN.md:50-53`). The
    optional def `amendTo : Policy -> Nat -> Flag` gives the rule.
    `amend(k)` runs only when the current outcome is `one p` and
    `amendTo p k` is `flagYes`. `Policy` gets no new field, so `mkPolicy`
    keeps its arity. A program with `amendments` and no `amendTo` is
    `REFUSE_AMEND`. A program with `amendTo` and no `amendments` is
    `REFUSE_AMEND`.
  - b3. Canonical amendment. `amend(k)`, with k the current constitution,
    changes no slot and makes no log, after the member check. This is
    `GovPhi canonicalAmendment L = Gov L` on `evm`.
  - b4. Storage. One new slot holds the number of the current
    constitution. It is slot K + M, after the ballot slots. Its zero word
    is constitution 0, so the constructor writes no new slot.
  - b5. Table. The table has one block of rows for each constitution, in
    the order of `anchorc table`. The row address is
    `rows + 5 (c R + rank)`, with R the rows of one constitution. The
    constitutions share the policy records. Each policy record gets a
    1-byte mask of `amendTo p k` for k = 0 to C - 1. Thus C is at most 8
    (`AMEND_LIMIT`). `TABLE_LIMIT` applies to C R. `EVM_LIMIT` (EIP-170)
    bounds the bytes, as now.
  - b6. O6 applies. `amend` reverts when the outcomes at the current tally
    before and after the switch are both `one` and the `schema` goes down.
    This is the guard of `cast` (section 7).
  - b7. Log. `Amended(uint256)` has topic 0 the event hash and data `k`.
    `anchorc abi` prints its line.
  - b8. The `amend` entry, the slot, the mask and the log exist only when
    C > 1 (D0). For C = 1, the `test/laws.sh` cases with no `amend` entry
    do not change.
  - b9. Prelude. `amendDAO` takes a second erased constitution and the new
    aggregation, and keeps `s.1` (the log). `amendKeepsGov` stays as the
    canonical case. The checker has no special case for `amend`.
  - D0. A program that uses no M6 feature keeps its M5 bytes. The 3
    example programs have C = 1.
- O4. `HashDom`. M1 has one value, `nonZero` (`h != 0`). The chain cannot
  see which function made a digest. RULED 2026-10-08 (USER): `nonZero`
  only. The choice of hash function is representation
  (`design/DESIGN.md:120`). M6 does not change `HashDom`.
- O5. Member addresses. RULED 2026-10-08 (USER): constructor arguments, one
  per member position, so the program holds no address value.
- O6. Schema version. The design says it "only increases". RULED 2026-10-08
  (USER): `cast` reverts when it moves a `one p` outcome to a policy with a
  lower `schema`. The guard of section 7 applies when the outcomes
  before and after the move are both `one`.
- O7. Challenge window and dispute annotations. No operation in the design
  dictionary reads `window`. M1 carries it in the policy and no entry reads
  it. Dispute annotations (metadata, never removal) are not in M1. RULED
  2026-10-08 (USER): a `dispute(bytes32,uint256,bytes32)` entry that
  writes no storage (M6, chunk 13). The defaults below are RULED 2026-10-08
  (USER).
  - A member annotates a recorded pair `(h, t)` with a note digest. The
    caller must be a member, and the pair slot of `(h, t)` must be set.
  - `dispute` runs only at a `one p` tally. A `two` tally reverts, as for
    `anchor`.
  - `dispute` runs only while `TIMESTAMP < t + window`. The strict `<`
    makes `window` 0 mean "no dispute".
  - `window` comes from the policy of the current tally, not of the tally
    at anchor time. Thus there is no new storage.
  - The entry emits `Disputed(bytes32,uint256,bytes32)`.
  - `verify` does not change. A dispute is metadata, never removal
    (`design/DESIGN.md:94`).
  - The prelude `dispute` is the identity on `AnchorDAO F`, so the law
    holds by definition.
  - The policy record goes from 9 to 17 bytes. The 8 new bytes are
    `window`.
  - The dispatch order is `anchor`, `verify`, `cast`, `amend`, `dispute`.
  - D0. The `dispute` entry and the 17-byte record exist only when some
    policy has `window` > 0. The 3 example programs have `window` 0, so
    they keep their M5 bytes.
- O8. Symmetry group. RULED 2026-10-08 (USER): the full symmetric group on
  the member positions. The trivial group makes anonymity vacuous (design
  section 3).
- O9. Admit. RULED 2026-10-07 (USER): members only. The caller must be a
  member and `admit` must give `flagYes`.
- O10. Initial profile. RULED 2026-10-08 (USER): every ballot starts at
  candidate 0, the genesis policy.

## 10. Milestones

One chunk per fresh session. Each chunk ends with its gate GREEN, its files
staged, and a status line here. The USER commits.

| Milestone | Chunk | Content |
|---|---|---|
| M0 | 0 | This SPEC, `design/DESIGN.md` (the design, verbatim), `formers/FORMERS.md` |
| M1 | 1 | Copy the origin compiler of section 8; rename to `anchorc` and `.anc`; Makefile (tcc build, check-clang); gate: `tcc -Wall -Werror`, parse tests, keccak vectors |
| M1 | 2 | `prelude/Prelude.anc` (sections 5 and 6), examples (one fate each, one `REFUSE_FORK` mutant), parse round trips |
| M1 | 3 | Checker (bidirectional, conversion by normalization) and the refusals of section 2; `formers/tcc-evm.md`; `probe/CAPABILITY.md`. If the origin compiler of section 8 gets a checker first, port it |
| M2 | 4 | Evaluator, tabulation, fate report: `anchorc check`, `table`, `eval` |
| M2 | 5 | Contract writer: entries, storage, outcome table, `Anchored` log; `anchorc build`, `abi` |
| M3 | 6 | Differential tests against geth `evm` on call traces; law tests (monotone, idempotent, distinct anchors commute, no deletion, `amend` is the identity on the log, no admit at a `two` tally); deploy test; docs |
| M4 | 7 | F13: `transportOutcome` and `congOutcome` for `EqOutcome`, with `symmOutcome` and `transOutcome` (F12); check, eval and build tests |
| M5 | 8 | Port the host to lang-template as `hosts/tcc-evm-anchor`: rename to `langc` and `.lang`, move the prelude to `domain/domain.lang`, register the kit at the lang-template root; gate: the kit `make check` and the root `make test` |
| M6 | 9 | Rulings of O3, O4 and O7 in SPEC section 9; the M6 rows of section 10; probe/CAPABILITY.md:113 ("Nothing after M5") and its title; the port script with `--check`; gate: `make`, `make check-clang`, `make test` (273 compiler cases plus port-script regressions), dash and origin-name scans, `--check` against lang-template 2a88a3a exits 0 |
| M6 | 10 | O3 prelude and checker: `amendDAO` with two constitutions, optional `amendments` and `amendTo`, tabulation and fork check for each constitution, `REFUSE_AMEND`, `AMEND_LIMIT`, `table` and `check` for each constitution, one new program (C = 2), mutants; gate: `make test`, new check/table/eval cases, the bytes of the 3 programs unchanged |
| M6 | 11 | O3 contract writer: the constitution slot, the row blocks, the `amend(uint256)` entry with its guards (member, `one p`, `amendTo`, k < C, O6) and the `Amended` log, the `abi` line; gate: `make test` with new build.sh, evm.sh and run.sh cases |
| M6 | 12 | O3 chain tests: laws.sh (`amend` is the identity on the pairs; the canonical `amend` changes nothing), diff.sh (the constitution slot in the awk model, 1 trace with `amend`), deploy.sh (the new program); gate: `make test` |
| M6 | 13 | O7: prelude `dispute`; the `dispute` entry, the 17-byte record and the `Disputed` log when some policy has `window` > 0; one new program; run.sh, laws.sh and diff.sh cases; gate: `make test` |
| M6 | 14 | Port: the script writes lang-template `hosts/tcc-evm-anchor`; gate: the kit `make check`, the root `make test`, the root `make check` blocks after `hosts/mech`, `--check` exits 0 |

Status 2026-10-07: chunk 0 staged.

Status 2026-10-07: chunk 1 staged. The front end, keccak and EVM
assembler of the origin compiler (section 8), renamed to `anchorc` and `.anc`. `src/evm.h` exports
the assembler and the target interface `anchor_evm_write`; its runtime has no
entry yet (every call reverts), and chunk 5 fills it. The prelude is a
placeholder (`def members : Nat := 1`) until chunk 2. Verbs `check`, `table`,
`eval`, `build`, `abi` parse the prelude and PROG, then exit 1 with
`PLANNED`. Gate GREEN: `make` (tcc `-Wall -Werror`), `make check-clang`,
`make test` (parse.sh 28 cases, evm.sh 5 cases: keccak vectors and the bytes
of the contract with no entry).

Status 2026-10-07: chunk 2 staged. `prelude/Prelude.anc` has the types of
section 5, the operations of section 6 and the structures of section 4
(prelude notes P1 to P10; P1 RULED). `examples/programs/` has one program
for each fate: `arrow-impossibility.anc` (`none`), `arrow-debreu.anc`
(`one p`), `schelling-ising.anc` (`two p q`, both sides frozen).
`examples/mutants/fork-unfrozen.anc` parses; chunk 3 refuses it with
`REFUSE_FORK`. Gate GREEN: `make`, `make check-clang`, `make test`
(parse.sh 32 cases: 6 round trips, embedded prelude, 13 refusals, 12
command line exits; evm.sh 5 cases).

Status 2026-10-07: chunk 3 staged. `src/check.{h,c}` is the checker of
the origin compiler of section 8 (bidirectional, conversion by
normalization), with the core names of prelude note P1 as opaque globals,
the codes `REFUSE_MEMBERS`, `REFUSE_DATA`, `REFUSE_REC`, `REFUSE_NAME` and
`REFUSE_FORK`, and the required `candidates` and `rule`. Each verb checks the
prelude and PROG, then exits 1 with `PLANNED`. `examples/mutants/` has one
mutant for each refusal of section 2 that the parser lets through, two for
the opaque core names (`TYPE_SHAPE`, `TYPE_MATCH`) and one for the type of
`rule` (`TYPE_MISMATCH`). `REFUSE_AXIOM` cannot be reached (section 2).
`formers/tcc-evm.md` gives the status of F1 to F15, and
`probe/CAPABILITY.md` gives the host facts. Gate GREEN: `make`,
`make check-clang`, `make test` (parse.sh 39 cases: 13 round trips,
embedded prelude, 13 refusals, 12 command line exits; evm.sh 5 cases;
check.sh 39 cases, with the conditional fork and type erasure regressions).

Status 2026-10-08: chunk 4a staged. `src/check.c` tabulates `rule` over
each tally (section 7) and makes the full fork check (section 2).
`anchorc table` prints the table and exits 0. `check` and `eval` (chunk
4b), `build` and `abi` (chunk 5) still exit 1 with `PLANNED`. Gate GREEN:
`make`, `make check-clang`, `make test` (parse.sh 38 cases: 13 round
trips, embedded prelude, 13 refusals, 11 command line exits; evm.sh 5
cases; check.sh 39 cases; table.sh 13 cases: the table of each example
program, the code of each mutant under `table`, the `TABLE_LIMIT` bound).

Status 2026-10-08: chunk 4b staged. `anchorc check` prints the fate report
of section 6 from the table of chunk 4a (section 7). `anchorc eval` prints
the normal form of a def, else `EVAL_NAME` (section 7). `build` and `abi`
(chunk 5) still exit 1 with `PLANNED`. Gate GREEN: `make`,
`make check-clang`, `make test` (parse.sh 38 cases: 13 round trips,
embedded prelude, 13 refusals, 11 command line exits; evm.sh 5 cases;
check.sh 40 cases: the fate report of each example program and
`TABLE_LIMIT` under `check`; table.sh 13 cases; eval.sh 24 cases: 9 normal
forms, 2 recursive-def round trips, 4 `EVAL_NAME` names, `eval` of a `TABLE_LIMIT` program, the code of
each mutant under `eval`).

Status 2026-10-08: chunk 5a staged. Chunk 5 has two parts. 5a: the slots,
the constructor, the dispatch, `verify`, `cast`, `anchorc abi` and
`anchorc build` (section 7). 5b: the `anchor` entry with the `admit` guard
on the outcome table by `CODECOPY`, the policy fields that a guard reads,
the O6 guard of `cast`, the `Anchored` log and the table bytes. Gate
GREEN: `make`, `make check-clang`, `make test` (parse.sh 38 cases: 13 round
trips, embedded prelude, 13 refusals, 11 command line exits with `abi` and
`build` at exit 0; evm.sh 7 cases: 2 keccak vectors, the dispatch, the
runtime and the creation code of 1 member and 1 candidate, 2 `EVM_LIMIT`
cases; check.sh 40 cases; table.sh 13 cases; eval.sh 24 cases; build.sh 35
cases: the `abi` text of each example program, 3 selectors, 6 build checks
for each example program, the code of each mutant under `abi` and `build`,
the EIP-3860 bound at 1527 and 1528 members, and a bytecode write failure
that exits 2 and removes the incomplete output).

Status 2026-10-08: chunk 5b staged. `anchorc abi` and `anchorc build`
tabulate. The runtime ends with the outcome table and computes the row of
the current tally (section 7). `anchor` has its guards, the pair slot and
the `Anchored` log. `cast` has the O6 guard. `anchorc abi` prints the
`Anchored` line. `test/run.sh` runs the contract on geth `evm`. Gate
GREEN: `make`, `make check-clang`, `make test` (parse.sh 38 cases; evm.sh
9 cases, with the bytes of the 5b contract and the EIP-170 edge; check.sh
40 cases; table.sh 13 cases; eval.sh 24 cases; build.sh 33 cases, with
the EIP-3860 edge by bisection; run.sh 19 cases), 176 cases in all.

Status 2026-10-08: chunk 6 staged. Chunk 6 is M3: three test files run
the contract on geth `evm` (section 7). `test/deploy.sh` deploys each
example program and checks the constructor guards. `test/diff.sh` runs
four call traces against a model of the outcome table. `test/laws.sh`
tests the laws of the log, with `amend` as the identity (O3). The tests
source `test/evmchain.sh`, which has no cases. There is no change to
`src/`. Gate GREEN: `make`, `make check-clang`, `make test` (parse.sh 38
cases; evm.sh 9 cases; check.sh 40 cases; table.sh 13 cases; eval.sh 24
cases; build.sh 33 cases; run.sh 19 cases; deploy.sh 13 cases; diff.sh 45
cases; laws.sh 23 cases), 257 cases in all.

M4 (chunk 7) design defaults:

- a. One family only: `EqOutcome`. The prelude adds `transportOutcome` and
  `congOutcome`. There is no new Eq family (no `EqTally`, `EqPolicy` or
  `EqNat`). My choice, not ruled.
- b. No heterogeneous cong, because it needs a second family. `congOutcome`
  takes `f : Outcome -> Outcome`. My choice, not ruled.
- c. The prelude also adds `symmOutcome` and `transOutcome` (F12).
  `transOutcome` uses `transportOutcome`, as in the origin compiler of
  section 8. My choice, not ruled.
- d. Surface: prelude defs with the family suffix and explicit erased
  arguments, in the form of the origin compiler. There is no generic `Eq`,
  because a family with a type index is outside the F14 policy
  (`formers/FORMERS.md:216-218`). My choice, not ruled.
- e. Computation is definitional. `transportOutcome P o o (sameOutcome o)
  u` reduces to `u` through the dependent match of the checker. `src/` has
  no special case for Eq, and chunk 7 does not change `src/`. My choice, not
  ruled.
- f. Erasure: the proof argument is relevant; `P`, `x` and `y` are erased.
  Proofs erase from the target (section 7). A rule that calls
  `transportOutcome` builds, and its bytecode is the bytecode of
  `arrow-impossibility.anc` (`test/build.sh`). A match on an erased proof is
  `TYPE_ERASED`. My choice, not ruled.
- g. Place: the new defs come after the last def of the prelude, so the
  prelude line citations of `formers/tcc-evm.md` do not move. My choice,
  not ruled.

Status 2026-10-08: chunk 7 staged. Chunk 7 is M4 (F13). The prelude adds
`transportOutcome`, `congOutcome`, `symmOutcome` and `transOutcome` for
`EqOutcome` after its last def. Both laws of F13 hold by definition
(`test/eval.sh`). Two new mutants, `cong-type.anc` and
`transport-motive.anc`, are `TYPE_MISMATCH`. `test/table.sh`,
`test/eval.sh` and `test/build.sh` read each mutant, so each of these
files gets two more cases. There is no change to `src/`. Gate GREEN:
`make`, `make check-clang`, `make test` (parse.sh 40
cases; evm.sh 9 cases; check.sh 45 cases; table.sh 15 cases; eval.sh 28
cases; build.sh 36 cases; run.sh 19 cases; deploy.sh 13 cases; diff.sh 45
cases; laws.sh 23 cases), 273 cases in all.

Status 2026-10-08: M5 done. Session 1 copied the compiler to lang-template
as `hosts/tcc-evm-anchor` (lang-template 5bec924, the kit): the names
`anchorc`, `anchor_`, `ANCHOR_` and `.anc` become `langc`, `lang_`, `LANG_`
and `.lang`, and `prelude/Prelude.anc` becomes `domain/domain.lang`. Session
2 registers the kit at the lang-template root (lang-template 2a88a3a:
`bin/new-lang.sh`, `Makefile`, `README.md`, `tests/test_tools.py`,
`formers/FORMERS.md`). Validation: the kit `make check` passed (273 cases,
the counts of M4), and the root `make test` passed (27 tests). The root
`make check` exited with status 2 at `hosts/mech` (`spawnSync mech ENOENT`,
no `mech` tool on this machine), before running the remaining host blocks.
Those blocks were then run separately and passed: `hosts/assay`,
`hosts/tcc-json`, `hosts/tcc-evm-contract`, `hosts/tcc-wasm`, `hosts/tcc-evm`,
`hosts/tcc-evm-dao` and `hosts/tcc-evm-anchor` (`gate: 0 failures`). The
`hosts/tcc-evm-contract` check included concurrent uncommitted changes in
that kit. The shared files of `hosts/tcc-wasm` and `hosts/tcc-evm` were
also compared separately and are the same. There is no change to `src/`
in this repository.

Status 2026-10-08: chunk 9 staged. Chunk 9 opens M6. Section 9 has the
rulings of O3 (with b1 to b9 and D0), O4 and O7 (with its defaults and
D0). The table above has the M6 rows, chunks 9 to 14. `probe/CAPABILITY.md`
gives the M6 plan. Its title stays (M5), because chunk 9 does not change
the capability. `tools/port.py` maps 44 source files to the kit
`hosts/tcc-evm-anchor` and applies the M5 renames. `--write KIT` writes
them, and `--check KIT` compares them. The script never writes the
kit-owned files: `README.md`, `FORMERS.md`, `Makefile`, `.gitignore`,
`docs/` and `test/gate.sh`. Before writing, it refuses missing or empty
source groups, duplicate kit paths, symlinks, multiply linked target
files, obstructed target paths and a kit that is the source root. These
refusals and I/O errors exit 2. There is no change to `src/`. Gate GREEN:
`make`, `make check-clang`, `make test` (273 compiler cases, the counts
of M4, plus 13 port-script regressions in `tools/test_port.py`).
`--check` on the lang-template kit (2a88a3a) exits 0 with 44 files.
`--write` into a copy of the kit without its mapped files, then `diff -r`
with the kit, is empty. A copy with one changed byte, a copy with one
mapped file removed and a copy with one extra file each make `--check`
exit 1.

Status 2026-10-08: chunk 10 staged. The checker reads the optional defs
`amendments` and `amendTo` (O3, section 9). It adds `REFUSE_AMEND`
(section 2), `AMEND_LIMIT` and the C R bound of `TABLE_LIMIT` (section 7),
the fork check of each constitution, the `amendTo` masks, and the `table`
and `check` forms for more than one constitution. `abi` and `build` give
`PLANNED` for such a program, because the contract writer is chunk 11.
`src/evm.c` does not change. The prelude adds `mu Constitutions` with
`lastConstitution` and `consConstitution`, and `amendDAO` gives the new
aggregation with the log of the old state (b9). The new files are
`examples/programs/arrow-debreu-amend.anc` and the mutants
`amend-no-to.anc`, `amend-to-only.anc`, `amend-limit.anc` and
`amend-fork-unfrozen.anc`. For the 3 programs of M5, `table`, `check`,
`abi`, `build` and `build --runtime` give the same bytes as the binary of
the parent commit. Gate GREEN: `make`, `make check-clang`, `make test` (304
compiler cases: parse 45, evm 9, check 52, table 21, eval 34, build 43, run
19, deploy 13, diff 45, laws 23; plus 13 port-script regressions).
`--check` on the lang-template kit exits 1 and lists only the chunk 10
paths. Open: a constitution k >= 1 tabulates only while the evaluation of
its profile stays under `CHECK_DEPTH` (section 7).

Status 2026-10-09: chunk 11 staged. `src/evm.c` writes the contract of a
program with more than one constitution (section 7): the slot K + M, the
C R rows, the policy records with the `amendTo` mask, the `amend` entry
with the O6 guard and the `Amended` log. `src/main.c` gives the masks to
the writer and no longer gives `PLANNED`. For the 3 programs of M5,
`table`, `check`, `abi`, `build` and `build --runtime` give the same bytes
as the binary of the parent commit. Gate GREEN: `make`, `make
check-clang`, `make test` (328 compiler cases: parse 45, evm 9, check 52,
table 21, eval 34, build 46, run 40, deploy 13, diff 45, laws 23; plus 13
port-script regressions). `--check` on the lang-template kit exits 1 and
lists only the chunk 10 and 11 paths.

Status 2026-10-09: chunk 12 staged. The chain tests of O3 (section 7):
`test/laws.sh` tests that `amend` is the identity on the pairs and that the
canonical `amend` changes nothing, `test/diff.sh` has the constitution slot
in its awk model and trace E with `amend`, and `test/deploy.sh` deploys
`arrow-debreu-amend.anc`. `test/evmchain.sh` adds `amend_in` and reads the
data and every topic of a log. There is no change to `src/`, so the bytes of
the 3 programs of M5 do not change. Gate GREEN: `make`, `make check-clang`,
`make test` (359 compiler cases: parse 45, evm 9, check 52, table 21, eval
34, build 46, run 40, deploy 17, diff 65, laws 30; plus 13 port-script
regressions). `--check` on the lang-template kit exits 1 and lists only the
chunk 10 to 12 paths.
