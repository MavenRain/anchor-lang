The contract is not a storage layout. Its meaning is an append-only public witness of content-hashes at times, admitted only under a policy that the DAO constitutes of itself. Governance is the object already fixed in `self-referential-dao`; the anchor log is an algebra that aggregation acts on, not a second aggregation. The file is the same design.




## 1. Stance

Denotational design in Conal Elliott's sense: fix the model, then a representation, then a total meaning \(\llbracket \cdot \rrbracket\), and admit an operation only when the same operation exists on the model and the meaning is a homomorphism for it.

\[
\llbracket \mathrm{op}\, a\, b \rrbracket \;=\; \mathrm{op}'\, \llbracket a \rrbracket\, \llbracket b \rrbracket
\]

The instance's meaning is the meaning's instance. A failed homomorphism is an abstraction leak and is rejected. The model is not storage, an event log, gas, or an EVM trace. Correctness of later bytecode is agreement with this denotation, not resemblance to it.

## 2. What is being denoted

A lawyer, or a tool the lawyer runs, writes a hash and a timestamp of an agreement, filing, photograph, or chain-of-custody log. The document stays off-chain. Anyone later holding the file recomputes the hash and shows that this byte-string was the one recorded at that time.

The anchor denotes a witnessed existence claim about a digest. It does not denote authorship, truth, title, or consideration, and it custodies no client funds. That is why it supports authentication under FRE 901 and 902(13)–(14) and answers "which version was anchored when," without becoming a new legal instrument. Arizona and Vermont already treat a blockchain record as an electronic record; elsewhere the hash is ordinary evidence of integrity. Rule 1.6 exposure is limited exactly by the absence, in the model, of a projection to plaintext.

## 3. Semantic domain

Governance is reused unchanged:

\[
\llbracket \mathrm{DAO} \rrbracket \;=\; \mathrm{Aggregation}\,\mathrm{act}\, F \;=\; \mathrm{LeftKanExtension}\,(\mathrm{orbitProjection}\,\mathrm{act})\, F.
\]

Reusing `Aggregation` inherits voter anonymity (the orbit quotient), legitimacy (the Lan universal property), and the Arrow trichotomy. For a lawyer-facing service the symmetry group may be trivial. Orbits then collapse to points, the Kan extension still exists whenever \(F\) does, and anonymity is vacuous. That should be stated, not assumed.

The acted-on algebra does not contain documents, funds, or plaintext.

\[
\mathrm{AnchorLog} \;\coloneqq\; \mathrm{Set}\,(\mathrm{Hash} \times \mathrm{Time})
\]

\(\mathrm{Hash}\) is an external collision-resistant digest. \(\mathrm{Time}\) is an external total order (block time or attested time); the constitution may select which readings are admissible, but it does not construct the order. \(\mathrm{Doc}\) and \(\mathrm{hash} : \mathrm{Doc} \to \mathrm{Hash}\) appear only in the verify observation. Collision resistance is a hypothesis on the external function, not a theorem of the DAO. Optional public metadata (chain reference, schema id) may ride along only if it has no projection to plaintext.

The paired meaning is

\[
\llbracket \mathrm{AnchorDAO} \rrbracket \;=\; \Sigma\,(L : \mathrm{Aggregation}\,\mathrm{act}\, F).\; \mathrm{AnchorLog}.
\]

A policy is not a set of hashes, and a set of hashes is not a vote.

What \(F.\mathrm{obj}\, X\) denotes is a policy: who may append (`Admit`), the accepted digest domain (`HashDom`), the admissible clock (`Clock`), a challenge window, a schema version that only increases, and whether an Ising fork freezes new admits. Retention is a view, not a deletion: any retention map is a monotone projection on the log. The log is not a parameter of \(F\).

Admission is read off the governance operator, not off a stored flag:

\[
\mathrm{admit}(L, X) \;\equiv\; (\mathrm{Gov}\, L).\mathrm{obj}\, X = \mathrm{Allow}.
\]

## 4. Self-reference

Unchanged from the DAO design.

\[
\mathrm{Gov}\,(L : \mathrm{Aggregation}\,\mathrm{act}\, F) \;:\equiv\; \mathrm{orbitProjection}\,\mathrm{act} \ggg L.\mathrm{functor}
\]

\[
\mathrm{IsSelfConstituting}\, F \;\equiv\; \exists L,\; \forall X,\; (\mathrm{Gov}\, L).\mathrm{obj}\, X = F.\mathrm{obj}\, X
\]

If \(L\) witnesses that fixed point, the policy the next anchor reads is the policy members constituted. Amendment is the one operation whose meaning is forced:

\[
\llbracket \mathrm{amend}\, \Phi \rrbracket\,(L, \mathrm{log}) \;=\; (\mathrm{GovPhi}\, \Phi\, L,\; \mathrm{log}),
\]

with \(\mathrm{GovPhi}\,(\mathrm{canonicalAmendment}\,\mathrm{act})\, L = \mathrm{Gov}\, L\). Amendment rewrites the constitution the next admission reads. It does not rewrite the log.

## 5. Dictionary

**Anchor** is union, guarded by the verdict. Both branches are the set operations, so the guard is not a leak.

\[
\llbracket \mathrm{anchor}\, h\, t \rrbracket\,(L, \mathrm{log}) \;=\;
\begin{cases}
(L,\; \mathrm{log} \cup \{(h,t)\}) & \text{if }\mathrm{admit}(L,X) \land \mathrm{Clock}(t) \land h \in \mathrm{HashDom}\\
(L,\; \mathrm{log}) & \text{otherwise}
\end{cases}
\]

**Verify** is not a state change. It is a predicate into \(\mathrm{Prop}\), independent of \(L\) once the log is fixed. Governance cannot make a past success fail.

\[
\llbracket \mathrm{verify} \rrbracket\,(\mathrm{file}, h, t, \mathrm{log}) \;\equiv\; \mathrm{hash}(\mathrm{file}) = h \land (h,t) \in \mathrm{log}
\]

Under a fork, the log in that equation is the shared prefix of section 6, not a branch-local successor. Cast is the unit of the Kan extension and appends nothing. A dispute annotation is metadata, never removal.

## 6. Laws, and the three fates

From the set model, free once \(\llbracket \cdot \rrbracket\) is a monoid homomorphism on the log: monotonicity, idempotence, commutativity of distinct anchors, and no deletion. There is no operation whose meaning is a proper subset.

Integrity is one direction only: a successful verify implies existed-at-time, under collision resistance. Absence of an anchor is not proof of non-existence. Confidentiality is the absence of a morphism to bytes. Non-custody is the absence of a balance monoid — unlike the escrow pairing, where the acted-on algebra is a ledger. Non-authorship is the absence of a morphism to an author: FRE 901, FRE 902(13)–(14), and 12 V.S.A. § 1913 authenticate the record of the hash-at-time, not the instrument. On a family of drafts, timestamps induce a preorder answering "which version was anchored earlier," not "which version was signed."

A representation lands in exactly one fate, and no fate erases a recorded pair.

- **Arrow-Impossibility.** No aggregation, so `admit` is nowhere and the log is frozen, not erased.
- **Arrow-Debreu.** One legitimate policy. The meaning of "the rules" is a function. This is the regime an evidence consumer should be in.
- **Schelling-Ising.** Two legitimate future policies over one shared prefix. Not two rewrites of the past, and not a silent choice of canonical chain.

\[
\mathrm{prefix}(F) \;\equiv\; \bigcap \mathrm{Logs}(F)
\qquad
\llbracket \mathrm{verify} \rrbracket\,(\mathrm{file}, h, t) \;\equiv\; \mathrm{hash}(\mathrm{file}) = h \land (h,t) \in \mathrm{prefix}(F)
\]

A pair recorded before the fork verifies in every branch. A pair admitted on only one branch is a branch-local annotation, not a public existence claim, until the regime is unique again or the branches are explicitly named as alternative policies. The denotation never says both "existed" and "did not" of one pair. Intersection rather than union is the evidence reading: FRE 901/902 authenticates a record, not a disputed alternative. The recommended constitution sets `ForkFreeze`, so an Ising configuration admits nothing new until the regime is unique.

## 7. Honesty

Inherited from the DAO design, and not relaxed here. For the shipped constant constitution, \(\mathrm{IsSelfConstituting}\) is single-valued at every \(\beta\). The jump at \(\beta_c = 1\) is a jump in the number of object-distinct aggregations, not in the fixpoint predicate. A self-constitution that genuinely bifurcates needs a non-constant constitution, which is not delivered. Proposal functoriality is vacuous over the discrete witnesses. Quorum and treasury stay out of scope; treasury is absent because this contract custodies nothing, not because a balance map was forgotten. Naturality of \(F \mapsto \mathrm{Aggregation}\,\mathrm{act}\, F\) is not claimed.

An implementation owes this denotation exactly four things: public state is a set of hash-time pairs and nothing that could be a document or a balance; append is union under `admit` and identity otherwise; amendment is `Gov` on the policy and the identity on the pair set; a fork is a disjunction of future policies over the shared prefix. Gas, layout, and the choice of hash function are representation.
