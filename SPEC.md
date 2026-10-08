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

The checker explores `case` and `match` alternatives in rule outcomes and
their policy freeze flags. Every possible fork side must reduce to
`flagYes`. It conservatively refuses a flag or outcome whose alternatives
remain unresolved, using `REFUSE_FORK`.

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
| F12 Eq refl, symm, trans | PARTIAL | `EqOutcome` with refl (`sameOutcome`) only | none |
| F13 Eq transport, cong | PLANNED | no prelude definition yet | none |
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
  assembler). RULED 2026-10-07 (USER): a standalone repo now; a later port
  can return the host to lang-template as `hosts/tcc-evm`.
- Gate tools: geth `evm` (1.14.12) runs the bytecode (`test/run.sh`);
  Foundry `cast` gives calldata and selectors as an oracle. The build does
  not need them. `make test` needs `evm` on the PATH to run
  `test/run.sh`; without it, `test/run.sh` exits 0 with a message.
- `probe/CAPABILITY.md` records what the host can do now: the TinyCC
  build, the EVM assembler, the checker and its codes, and the PLANNED
  work of chunk 6. The contract writer of chunk 5 is in `src/evm.c`.

## 9. Open items

- O1. Time source. RULED 2026-10-07 (USER): block time only. `anchor` takes
  `h`; `t` is `block.timestamp`; `Clock` has one value, `blockTime`, so
  `Clock t` is always true.
- O2. Fork fate. RULED 2026-10-07 (USER): ForkFreeze is forced. A
  Schelling-Ising contract admits nothing new, and `verify` reads the whole
  log, which is then the shared prefix. The encoding of the fork as a
  `two p q` outcome at a tally is my choice and is not ruled.
- O3. `amend`. The design forces `GovPhi canonicalAmendment L = Gov L`. M1
  has only the canonical amendment, so there is no `amend` entry. A
  non-canonical `Phi` (a second constitution that members switch to) needs
  a design for who may amend and under which rule. Not ruled.
- O4. `HashDom`. M1 has one value, `nonZero` (`h != 0`). The chain cannot
  see which function made a digest. Not ruled.
- O5. Member addresses. Constructor arguments, one per member position, so
  the program holds no address value. Not ruled.
- O6. Schema version. The design says it "only increases". Proposal: `cast`
  reverts when it moves a `one p` outcome to a policy with a lower
  `schema`. Not ruled. The guard of section 7 applies when the outcomes
  before and after the move are both `one`.
- O7. Challenge window and dispute annotations. No operation in the design
  dictionary reads `window`. M1 carries it in the policy and no entry reads
  it. Dispute annotations (metadata, never removal) are not in M1. Not
  ruled.
- O8. Symmetry group. M1 uses the full symmetric group on the member
  positions. The trivial group makes anonymity vacuous (design section 3).
  Not ruled.
- O9. Admit. RULED 2026-10-07 (USER): members only. The caller must be a
  member and `admit` must give `flagYes`.
- O10. Initial profile. Proposal: every ballot starts at candidate 0, the
  genesis policy. Not ruled.

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
