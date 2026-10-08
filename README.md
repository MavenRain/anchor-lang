# anchor-lang

anchor-lang (working name) is a small language for one governed anchor log:
an append-only public record of content hashes at block times. A DAO of
fixed members votes on the policy that admits new anchors. The compiler,
`anchorc`, is C99 built by TinyCC. It writes EVM bytecode for one contract.

The core types and operations are only those of `design/DESIGN.md`. The
type formers are those of `formers/FORMERS.md` (from lang-template). The
contract stores hash-time pairs, ballots and member addresses. It stores no
document, no plaintext and no balance.

Status: draft. The front end (lexer, parser, printer), the EVM assembler and
the checker build. The checker refuses the forms of `SPEC.md` section 2. The
`anchorc check` prints the fate report, `anchorc table` prints the outcome
of each tally and `anchorc eval` prints the normal form of a def (`SPEC.md`
sections 6 and 7). `anchorc abi` prints the entries and the log of the contract, and
`anchorc build` writes its creation code or runtime code as hex, with the
outcome table at the end of the runtime (chunk 5, `SPEC.md` section 7).
The tests of chunk 6 (M3) run the contract on geth `evm`: a deploy test, a
differential test against a model of the outcome table and the law tests
of the log.
See `SPEC.md` section 10 for the milestones.

## Build

`make` builds `build/anchorc` with `tcc -std=c99 -Wall -Werror`.
`make check-clang` checks every C file with
`cc -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only`.
`make test` runs `test/parse.sh`, `test/evm.sh`, `test/check.sh`,
`test/table.sh`, `test/eval.sh`, `test/build.sh`, `test/run.sh`,
`test/deploy.sh`, `test/diff.sh` and `test/laws.sh`. The last four run the
contract on geth `evm` (1.14.12), so `make test` needs `evm` on the PATH
to run them. Without `evm`, each of the four prints a message and exits 0.

## Layout

- `SPEC.md`: the language specification (draft) and the open items.
- `design/DESIGN.md`: the denotational design, verbatim.
- `formers/FORMERS.md`: the type former specification.
- `formers/tcc-evm.md`: the status and the evidence of each former on this
  host.
- `probe/CAPABILITY.md`: what the host can do now, and what is PLANNED.
- `src/`: `anchorc`. `syntax.h` has the grammar and the front end API;
  `arena.c`, `diag.c`, `lexer.c`, `parser.c`, `printer.c` are the front end;
  `check.c` is the checker, the table, the fate report and `eval`, and
  `check.h` its interface;
  `evm.c` is the EVM assembler and the contract writer (constructor,
  dispatch, entries, outcome table, `abi` text); `keccak.c` is
  Keccak-256; `main.c` is the command line; `prelude.h` declares the
  embedded prelude.
- `prelude/Prelude.anc`: the prelude (the types and operations of SPEC
  sections 5 and 6), embedded in `anchorc` at build time.
- `examples/programs/`: one program for each fate:
  `arrow-impossibility.anc` (`none`), `arrow-debreu.anc` (`one p`),
  `schelling-ising.anc` (`two p q`).
- `examples/mutants/`: programs that parse and that the checker must
  refuse; the first comment names the code: `fork-unfrozen.anc`
  (`REFUSE_FORK`), `data-decl.anc` (`REFUSE_DATA`), `rec-def.anc`
  (`REFUSE_REC`), `prelude-name.anc` and `core-name.anc` (`REFUSE_NAME`),
  `hash-projection.anc` (`TYPE_SHAPE`), `log-match.anc` (`TYPE_MATCH`),
  `rule-type.anc` (`TYPE_MISMATCH`).
- `tools/embed.c`: writes the prelude as C (`build/prelude.c`).
- `test/`: `parse.sh` (round trips, parse refusals, command line exits),
  `evm.sh` (keccak vectors, the bytes of the contract, the EIP-170
  bound), `build.sh` (`abi` and `build` of each program and mutant, the
  EIP-3860 bound), `run.sh` (the contract on geth `evm`), `deploy.sh` (the
  deploy of each program and the constructor guards on `evm`), `diff.sh`
  (four call traces on `evm` against a model of the table), `laws.sh` (the
  laws of the log on `evm`), `evmchain.sh` (the helpers of these three
  files; no cases), `check.sh` (the prelude, the
  programs, the mutants and the checker regressions), `table.sh` (the
  table of each program, the mutants under `table`, `TABLE_LIMIT`), `eval.sh` (the normal
  form of each def, `EVAL_NAME`, the mutants under `eval`), and the drivers
  `parsetool.c` and `evmtool.c`; `parser-arms.anc` is a parser regression.

## License

MIT OR Apache-2.0. See `LICENSE-MIT` and `LICENSE-APACHE`.
