# Checker and decoder

`task` starts with an unconstrained planning pass using the supplied
model, continuing until the model emits an end token with no output token cap.
It requests an implementation plan: expected
behavior, typed helper signatures, function-body steps, mutations, loop exits,
and recursion base cases. Planning messages and the plan text go to stderr only
in debug mode (`--debug`, also enabled by `--debug-limit`). The plan is included
with the original request in the code-generation prompt. It is guidance, not a proof of
correctness. The planning prompt includes the full language specification and
requires pseudocode and typed statement sketches to obey its ANF, type, and scope
rules, including named calls with atomic arguments, conditions, and return values.
Planning remains unconstrained text generation; these are prompt instructions,
not a typechecker guarantee for the plan. `--n` limits code tokens independently; `--no-plan` or a raw
`--prompt` skips planning. When the context fills, it re-evaluates the original
planning request plus recent output while retaining the entire plan text for
code generation. Empty plans or planner failures stop generation. The planning context is freed before creating a fresh
code-generation context, while model weights remain loaded.

`typed::Prefix` consumes arbitrary byte chunks, so model tokens can split an
identifier or span several language tokens. It keeps a parser phase, a block
stack, typed local symbols, visible function signatures, and a partial lexical token. A candidate is
accepted if every byte keeps the current state extendable to a valid typed
program. The check is transactional: rejection leaves the state unchanged.
An end token is accepted only when the whole program has closed. The checker
can also construct a completion from every viable prefix. This prevents a
model that keeps extending a literal or identifier from leaving the output
unfinished at the token budget.

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
Every checker state has a completion: `Int64` and `Bool` have literals; reference
types can be created from those literals; a block can close; and the main
body can return `0`. For a reference-returning helper, completion can allocate
a cell before returning it. Calls with unavailable reference arguments are never
offered. This does not yet implement algebraic data
types, general polymorphism, execution, or a general-purpose compiler. The
checker validates syntax and types; it does not prove termination or freedom
from runtime errors.
