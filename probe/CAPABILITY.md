# anchor-lang host capability: TinyCC to EVM (M6 chunk 12, 2026-10-09)

This file records what the `tcc-evm` host can do at the end of chunk 12
(SPEC section 10). The host is the C99 compiler `anchorc`, built by
TinyCC. The target is EVM bytecode for one contract. Each fact cites a file
or a test. PLANNED work has no evidence yet.

## Compiler

- TinyCC 0.9.28rc (mob 0fb54300, AArch64 Darwin), at
  `/Users/oobi/.local/bin/tcc`. `make` builds `build/anchorc`,
  `build/parsetool` and `build/evmtool` with `tcc -std=c99 -Wall -Werror`
  (`Makefile`).
- `make check-clang` checks every C file with
  `cc -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only` (Apple
  clang 21.0.0).
- `tools/embed.c` runs under `tcc -run` and writes the prelude as C
  (`build/prelude.c`). `test/parse.sh` checks that the embedded prelude
  is equal to `prelude/Prelude.anc`.
- Verbs (`src/main.c`): `anchorc check|table|abi PROG`,
  `anchorc eval PROG NAME`, `anchorc build PROG [--runtime] -o OUT`. Each
  verb parses and checks the embedded prelude and PROG. `check` prints the
  fate report and `table` prints the table (SPEC section 7). Each tabulates
  one time. `eval` prints the normal form of a def, else `EVAL_NAME`.
  `abi` prints the entries and the `Anchored` log, and `build` writes
  the hex of the contract with the outcome table (chunk 5, SPEC section
  7). Both tabulate one time; a table refusal stops them with its code.
  For a program with more than one constitution (O3, chunks 10 and 11),
  `abi` also prints the `amend` entry and the `Amended` log, and `build`
  writes the slot K + M, the C R rows and the policy records with the
  `amendTo` mask (`test/build.sh`, `test/run.sh`).

## Limits

- One run uses one arena of at most `ANCHOR_ARENA_MAX` = 256 MiB
  (`src/syntax.h`), far under the 4 GB limit for a run.
- The parser and the AST nest at most `ANCHOR_DEPTH_MAX` = 512 levels
  (`PARSE_DEPTH`; `src/syntax.h`, `test/parse.sh`).
- The checker has `CHECK_FUEL` = 2^24 evaluation steps for each
  declaration and for the fork check (`TYPE_FUEL`, `src/check.c`).
- A program has at most 8 constitutions (`AMEND_LIMIT`,
  `examples/mutants/amend-limit.anc`). With C constitutions and R
  tallies, a table has at most 4096 rows, C R (`TABLE_LIMIT`,
  `test/table.sh`). A constitution k >= 1 tabulates only while the
  evaluation of a profile of M ballots stays under `CHECK_DEPTH` = 4096
  nested calls, else `TYPE_FUEL` (SPEC section 7).
- Universes: `Type 0` has the type `Type 1`, and `Type 1` has no type
  (`TYPE_UNIVERSE`). An explicit `Type 1` annotation is refused. Pi,
  Sigma, product and sum formation take the maximum universe of their
  constituents (`src/check.c`, `infer_bind` and `infer_body`). Besides
  `Option : Type 0 -> Type 0`, definitions such as
  `packed : (A : Type 0) * A := (Nat, 7)`,
  `types : prod (Type 0, Type 0) := tuple (Nat, Flag)` and
  `typeSum : sum (Type 0, Type 0) := inj 0 of 2 Nat` check.
- Recursion is structural only, and only the prelude has it: a `def rec`
  is `fun ... => match` on one of its parameters, and each recursive call
  has a smaller argument (`TYPE_REC`). `Nat` has no eliminator (prelude
  note P10). Thus the host has no unfold (F7).
- The surface has no axiom form (SPEC section 2).
- A table holds at most `TABLE_LIMIT` = 4096 tallies, C(members + K - 1,
  K - 1) for K candidates (`src/check.c`; `test/table.sh`: 4095 members
  and 2 candidates pass, 4096 members are `TABLE_LIMIT`).
- The runtime code with the table bytes is at most `EVM_RUNTIME_MAX` =
  24576 bytes (EIP-170). The creation code with the member words is at
  most `EVM_INITCODE_MAX` = 49152 bytes (EIP-3860). Else `EVM_SIZE`, and
  the message names the bound. The table grows with the members, so the
  edges are not fixed numbers: `test/build.sh` finds the EIP-3860 edge of
  `arrow-debreu.anc` by bisection, and `test/evm.sh` tells the two bounds
  apart by the message at the EIP-170 edge with 2 candidates. 0 members
  or 0 candidates is `EVM_LIMIT`.
- The test driver `build/evmtool` clamps its table at 65536 rows
  (`test/evmtool.c`).

## Carried from the origin compiler

The origin compiler is the compiler of SPEC section 8.

- Front end: lexer, parser, printer, arena and diagnostics (`src/`). The
  parse refusals keep their codes: `LEX_TOKEN`, `LEX_NUMBER`,
  `PARSE_PAREN`, `PARSE_EXPECT`, `PARSE_ARITY`, `PARSE_DEPTH`
  (`test/parse.sh`).
- Keccak-256 (`src/keccak.c`). `test/evm.sh` checks the vectors.
- EVM assembler (`src/evm.{h,c}`): opcodes, PUSH widths, labels 0 to 63
  with two passes for the PUSH2 offsets, the dispatch by the selector of a
  signature, and the creation code. The runtime has the dispatch head and
  the revert block only. Chunk 5a adds the constructor, the entries and
  `anchor_abi_write` (SPEC section 7). The target interface is
  `anchor_evm_write`. Codes: `EVM_LIMIT`, `EVM_SIZE`,
  `EVM_USAGE`, `EVM_IO`, `EVM_INTERNAL`.

## Checker

`src/check.{h,c}` is a bidirectional checker with conversion by
normalization, ported from the origin compiler. It checks the embedded
prelude, then the program. The core names of prelude note P1 (`Hash`,
`Time`, `AnchorLog`, `logMember`, `logInsert`, `hashNonZero`) are opaque
globals: they do not reduce.

Program refusals (SPEC section 2):

| Code | Cause | Evidence |
|---|---|---|
| `REFUSE_MEMBERS` | the program does not start with `members` | `test/check.sh` |
| `REFUSE_DATA` | a `mu` in a program | `examples/mutants/data-decl.anc` |
| `REFUSE_REC` | a `def rec` in a program | `examples/mutants/rec-def.anc` |
| `REFUSE_NAME` | a program defines a prelude name or a core name | `examples/mutants/prelude-name.anc`, `examples/mutants/core-name.anc` |
| `REFUSE_FORK` | a `two p q` side that is not frozen, or a fork that the check cannot see | `examples/mutants/fork-unfrozen.anc`, `examples/mutants/amend-fork-unfrozen.anc`, `test/check.sh` |
| `REFUSE_AMEND` | `amendments` with no `amendTo`, or `amendTo` with no `amendments` | `examples/mutants/amend-no-to.anc`, `examples/mutants/amend-to-only.anc` |
| `REFUSE_AXIOM` | cannot be reached: the parser gives `PARSE_EXPECT` | `test/check.sh` |

Type codes: `TYPE_SCOPE`, `TYPE_MISMATCH`, `TYPE_SHAPE`, `TYPE_MATCH`,
`TYPE_ERASED`, `TYPE_INFER`, `TYPE_DUPLICATE`, `TYPE_UNIVERSE`, `TYPE_MU`,
`TYPE_REC`, `TYPE_NAT`, `TYPE_FUEL`, `TYPE_INTERNAL`. The mutants
`hash-projection.anc` (`TYPE_SHAPE`) and `log-match.anc` (`TYPE_MATCH`)
cover the opaque core names. `rule-type.anc` (`TYPE_MISMATCH`) covers the
type of `rule`.

The fork check is conservative (SPEC section 2). It explores each `case`
and `match` arm of the outcomes of `rule` and of their freeze flags. Chunk 4
tabulates the rule and makes the full fork check.

## Planned

M6 (SPEC section 10, chunks 9 to 14). Chunks 9 to 12 are done: chunk 9
adds `tools/port.py` and no capability, and chunks 10 to 12 add a list of
constitutions at compile time, a guarded `amend(uint256)` entry and its
chain tests (O3, see `## Compiler` and `## Gates`). Chunk 13 adds a
`dispute(bytes32,uint256,bytes32)` entry that writes no storage (O7).
Chunk 14 ports the host again to the M5 kit, lang-template
`hosts/tcc-evm-anchor`, with `tools/port.py`. `HashDom` stays `nonZero`
only (O4, SPEC section 9).

## Gates (2026-10-09)

All GREEN on the chunk 12 tree:

- `make` (tcc `-Wall -Werror`).
- `make check-clang`.
- `make test`: `test/parse.sh` 45 cases (20 round trips), `test/evm.sh` 9
  cases, `test/check.sh` 52 cases, `test/table.sh` 21 cases,
  `test/eval.sh` 34 cases, including both recursive prelude defs and both
  laws of F13, `test/build.sh` 46 cases, `test/run.sh` 40 cases, `test/deploy.sh` 17
  cases, `test/diff.sh` 65 cases (5 traces; trace E calls `amend`) and
  `test/laws.sh` 30 cases (with the `amend` laws of O3), the last four on
  geth `evm` 1.14.12 (359 cases in all).
