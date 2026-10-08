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
evaluator (chunk 4) and the contract writer (chunk 5) are PLANNED.

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
the tallies of each fate. Because `REFUSE_FORK` forces `ForkFreeze`, a
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
| `anchor(bytes32)` returns `uint256` | caller is a member; `h != 0`; `admit` at the current tally | sets the slot of `keccak256(h, t)` with `t = TIMESTAMP`; logs `Anchored(bytes32,uint256)`; returns `t` |
| `verify(bytes32,uint256)` returns `uint256` | none | returns 1 if the slot of `keccak256(h, t)` is set, else 0 |
| `cast(uint256)` | caller is a member; ballot less than the number of candidates; O6 | changes the ballot and the tally counts |

Storage holds the member addresses, the ballots, the tally counts and the
pair slots. Nothing else. Chunk 5 (section 10) fixes the slot numbers.

The compiler tabulates `Gov` over every tally at compile time. The runtime
reads the outcome code of the current tally from a table at the end of the
runtime code by `CODECOPY`. For each candidate the
table also holds the policy fields that a guard reads.

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
- Gate tools: geth `evm` (1.14.12) runs the bytecode; Foundry `cast` gives
  calldata and selectors as an oracle. The build does not need them.
- `probe/CAPABILITY.md` records what the host can do now: the TinyCC
  build, the EVM assembler, the checker and its codes, and the PLANNED
  work of chunks 4 to 6.

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
  `schema`. Not ruled.
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
