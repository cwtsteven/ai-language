# Checker and decoder

`generate` plans, then samples code. Planning does not use the checker. It runs until the model emits an end token, with no output token cap, and asks for expected behavior, helper signatures, body steps, assignments, loop exits, and recursion base cases. The plan is included in the code prompt and is guidance, not a proof. The planning prompt includes [the language specification](language.md) as a constraint on the later program. The plan itself is prose and signatures, and the prompt does not check it. The plan appears on stderr only with `--debug` or `--debug-limit`. The sampled typed program is always printed on stderr. A complete program is translated to Cangjie on stdout. An unfinished sample still writes the typed prefix on stdout (exit 2) so harnesses can save it.

`--n` limits code tokens only. `--skip-plan` skips planning. If the context fills, planning is re-evaluated from the original request plus recent output, and the full plan text is kept for code generation. An empty plan or a planner failure stops the run. The planning context is freed before a fresh code context; the weights stay loaded.

`--plan-only` writes the plan to stdout and exits. `--plan-file FILE` reads a nonempty plan, skips planning, and still includes the specification. Both require a description and cannot combine with `--skip-plan`.

`typed::Prefix` accepts arbitrary byte chunks, so one token may split a name or cover several language tokens. It keeps a parser phase, a block stack, typed locals, visible signatures, and a partial token. A candidate is accepted when every byte leaves a state that can still become a valid program. Rejection does not change that state and records one diagnostic (line, column, message) for `check`, `complete`, `format`, and unconstrained `generate`. An end token is accepted only when the program has closed.

Every viable prefix has a constructed completion. `--force-postfix` appends it when the token budget leaves the program open. The default does not. The benchmark's typed arm does not pass the flag; `plain-llama` has neither flag.

Both passes sample at `--temperature` (default `0.8`). Code sampling ranks candidates by `logit + temperature * Gumbel(0, 1)` and tests them in that order until one is viable. The distribution matches masking every invalid token before softmax, while only the candidates ahead of the winner are checked. Rejected candidates do not advance the checker. There is no top-k or top-p cut. A complete program forces an end token.

`--temperature 0` is greedy. `--seed` fixes the seed; the default is random and printed to stderr. Planning and code each start a fresh stream from that seed. The same seed can still differ across hardware or numerical backends.

For vocabulary size `V` and `R` rejected candidates, heap creation is `O(V)` and removals are `O(R log V)`. Each prefix test grows with the piece, the live symbols, and the visible signatures.

A signature is registered before its body, so the function may call itself. A call is offered only when its result type matches and every parameter type has an atom available. Each argument is checked against its declared type. Leaving a function drops its parameters and locals. A function body cannot finish the program; `main` is still required.

`Int64`, `Bool`, and `String` have literals, a block can close, and `main` can return `0`, so every checker state can be completed. `List<T>()` inhabits every list type. A struct atom exists when a value is in scope or every field is inhabitable. Value-type cycles through struct fields are rejected. Calls whose argument types are unavailable are not offered. The checker does not invent types during search, does not run the program, and does not prove termination or the absence of runtime errors.
