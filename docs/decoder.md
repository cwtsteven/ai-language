# Checker and decoder

`typed::Prefix` consumes arbitrary byte chunks, so model tokens can split an
identifier or span several language tokens. It keeps a parser phase, a block
stack, a typed symbol table, and a partial lexical token. A candidate is
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
costs `O(piece length × symbol count)` in this first implementation. A typed
trie would speed up identifier checks.

Completeness here means exact prefix viability **for this defined language**.
Every checker state has a completion: `Int64` and `Bool` have literals; reference
types can be created from those literals; a block can close; and the main
body can return `0`. Calls with unavailable reference arguments are never
offered. This does not yet implement user-defined functions, algebraic data
types, general polymorphism, execution, or a general-purpose compiler. The
checker validates syntax and types; it does not prove termination or freedom
from runtime errors.
