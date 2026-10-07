# anchor-lang

anchor-lang (working name) is a small language for one governed anchor log:
an append-only public record of content hashes at block times. A DAO of
fixed members votes on the policy that admits new anchors. The compiler,
`anchorc`, is C99 built by TinyCC. It writes EVM bytecode for one contract.

The core types and operations are only those of `design/DESIGN.md`. The
type formers are those of `formers/FORMERS.md` (from lang-template). The
contract stores hash-time pairs, ballots and member addresses. It stores no
document, no plaintext and no balance.

Status: draft. Only the specification exists. See `SPEC.md` section 10 for
the milestones.

## Layout

- `SPEC.md`: the language specification (draft) and the open items.
- `design/DESIGN.md`: the denotational design, verbatim.
- `formers/FORMERS.md`: the type former specification.

## License

MIT OR Apache-2.0. See `LICENSE-MIT` and `LICENSE-APACHE`.
