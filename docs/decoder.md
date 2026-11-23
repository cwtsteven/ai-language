# Checker and decoder

`task` starts with an unconstrained greedy planning pass using the supplied
model, continuing until the model emits an end token with no output token cap.
It requests an implementation plan: expected
behavior, typed helper signatures, function-body steps, mutations, loop exits,
and recursion base cases. The plan goes to stderr and is included with the
original request in the code-generation prompt. It is guidance, not a proof of
correctness. `--n` limits code tokens independently; `--no-plan` or a raw
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

The sampler builds a max heap of candidates by model logit and tests them in
that order, choosing the first viable one. This is **greedy constrained
decoding**. It does not preserve the original sampling distribution. For a
vocabulary of size `V` and `R` rejected high-logit candidates, heap creation
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
