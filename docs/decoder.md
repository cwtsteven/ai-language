# Checker and decoder

`generate` starts with a planning pass that does not use the code checker. It uses the supplied
model and continues until the model emits an end token, with no output token cap.
It requests an implementation plan: expected
behavior, typed helper signatures, function-body steps, mutations, loop exits,
and recursion base cases. Planning messages and the plan text go to stderr only
in debug mode (`--debug`, also enabled by `--debug-limit`). The plan is included
with the original request in the code-generation prompt. It is guidance, not a proof of
correctness. The planning prompt includes the full typed language specification and
requires pseudocode and typed statement sketches to obey its ANF, type, and scope
rules, including named calls with atomic arguments, conditions, and return values.
Those rules are prompt instructions, not a typechecker guarantee for the plan.
Code generation keeps the typed sample in memory. When the program is complete
and well-typed, `generate` translates it to Cangjie and writes that program.
An unfinished sample produces no Cangjie source. `--n` limits
code tokens independently. `--skip-plan` skips planning. When the context fills, it re-evaluates the original
planning request plus recent output while retaining the entire plan text for
code generation. Empty plans or planner failures stop generation. The planning context is freed before creating a fresh
code-generation context, while model weights remain loaded.

`generate --plan-only` writes the complete plan to stdout and exits before code
generation. `generate --plan-file FILE` reads a saved nonempty plan and skips the
planner. It builds the code prompt with the same helper as integrated planning,
preserving the plan text exactly and still including the language specification.
These options require a description and cannot combine with `--skip-plan`.
Normal `generate` runs still show the plan only in debug mode.

`typed::Prefix` consumes arbitrary byte chunks, so model tokens can split an
identifier or span several language tokens. It keeps a parser phase, a block
stack, typed local symbols, visible function signatures, and a partial lexical token. A candidate is
accepted if every byte keeps the current state extendable to a valid typed
program. The check is transactional: rejection leaves the state unchanged and
records one located diagnostic (line, column, message) for `check`, `complete`,
`format`, and unconstrained `generate` to print like a compiler error.
An end token is accepted only when the whole program has closed. The checker
can also construct a postfix from every viable prefix. `--force-postfix` appends
that constructed ending when the token budget leaves the program open. The
default leaves the sampled prefix as it is. The benchmark's typed arm does not
pass `--force-postfix`. Its Cangjie arm is `plain-llama`, which
has neither flag.

Both passes use temperature sampling (`--temperature`, default `0.8`). Code
sampling conditions the temperature-scaled model distribution on type-valid
tokens. It assigns each candidate a random Gumbel priority
`logit + temperature * Gumbel(0, 1)`, builds a max heap, and tests candidates
in that order until one is viable. This has the same distribution as masking
all invalid tokens before softmax sampling, while checking only the candidates
that precede the winner. Rejected candidates never advance the checker.
No top-k or top-p truncation is applied. A complete program forces an end token.

`--temperature 0` removes the random perturbation and restores greedy decoding.
`--seed` fixes the random seed; the default seed is random and printed to stderr.
The planner and code pass each start a fresh random stream from that seed.
Repeating a run requires the same model, inputs, settings, and numerical backend;
a seed does not guarantee identical results across different hardware.

For a vocabulary of size `V` and `R` rejected candidates, heap creation
and masking cost `O(V)`, heap removals cost `O(R log V)`, and each prefix test
cost grows with piece length, live symbols, and visible function signatures in
this implementation. A typed trie would speed up identifier checks.

A function signature is registered before its body, allowing self recursion.
Calls are offered only when their result type matches and all parameter types
have atomic arguments available. Each argument is checked against its declared
type. Leaving a function clears its parameters and locals. A function body
cannot end the program: a complete `main` is still required.

Completeness here means exact prefix viability **for this defined language**.
Every checker state has a completion: `Int64` and `Bool` have literals, a block
can close, and the main body can return `0`. Calls whose argument types are not
available are never offered. This does not yet implement algebraic data
types, general polymorphism, execution, or a general-purpose compiler. The
checker validates syntax and types; it does not prove termination or freedom
from runtime errors.
