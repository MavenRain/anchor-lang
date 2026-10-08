# anchor-lang

anchor-lang (working name) is a small language for one governed anchor log:
an append-only public record of content hashes at block times. A DAO of
fixed members votes on the policy that admits new anchors. The compiler,
`anchorc`, is C99 built by TinyCC. It writes EVM bytecode for one contract.

The core types and operations are only those of `design/DESIGN.md`. The
type formers are those of `formers/FORMERS.md` (from lang-template). The
contract stores hash-time pairs, ballots and member addresses. It stores no
document, no plaintext and no balance.

Status: draft. The front end (lexer, parser, printer) and the EVM assembler
build; the checker, the evaluator and the contract writer do not exist yet.
See `SPEC.md` section 10 for the milestones.

## Build

`make` builds `build/anchorc` with `tcc -std=c99 -Wall -Werror`.
`make check-clang` checks every C file with
`cc -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only`.
`make test` runs `test/parse.sh` and `test/evm.sh`.

## Layout

- `SPEC.md`: the language specification (draft) and the open items.
- `design/DESIGN.md`: the denotational design, verbatim.
- `formers/FORMERS.md`: the type former specification.
- `src/`: `anchorc`. `syntax.h` has the grammar and the front end API;
  `arena.c`, `diag.c`, `lexer.c`, `parser.c`, `printer.c` are the front end;
  `evm.c` is the EVM assembler and the target interface; `keccak.c` is
  Keccak-256; `main.c` is the command line; `prelude.h` declares the
  embedded prelude.
- `prelude/Prelude.anc`: the prelude (the types and operations of SPEC
  sections 5 and 6), embedded in `anchorc` at build time.
- `examples/programs/`: one program for each fate:
  `arrow-impossibility.anc` (`none`), `arrow-debreu.anc` (`one p`),
  `schelling-ising.anc` (`two p q`).
- `examples/mutants/`: programs that parse and that the checker must
  refuse; the first comment names the code (`fork-unfrozen.anc`:
  `REFUSE_FORK`).
- `tools/embed.c`: writes the prelude as C (`build/prelude.c`).
- `test/`: `parse.sh` (round trips, parse refusals, command line exits),
  `evm.sh` (keccak vectors, assembler bytes), and their drivers
  `parsetool.c` and `evmtool.c`; `parser-arms.anc` is a parser regression.

## License

MIT OR Apache-2.0. See `LICENSE-MIT` and `LICENSE-APACHE`.
