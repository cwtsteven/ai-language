# Typed pipeline vs direct Cangjie

Compare the same local model, task, token budget, temperature, and seed.
The typed arm plans and generates with `typed-cangjie`, which appends `docs/language.md`
and writes Cangjie.
The Cangjie arm plans and generates with `plain-llama`, which appends `docs/cangjie-subset.md`
and does not run the typed checker. Requires Python 3.10+, both built binaries, and the Cangjie SDK.

```sh
cmake --build build --parallel 4
python3 benchmarks/run.py --validate
python3 benchmarks/run.py --model /path/to/model.gguf --output benchmark-results/my-run
```

A shorter run:

```sh
python3 benchmarks/run.py --model /path/to/model.gguf \
    --tasks he_053_add he_055_fibonacci --n 2048 --samples 1 \
    --output benchmark-results/smoke
```

Model selection accepts the same GGUF files and JSON plugins as `generate`. Supply a
new output directory for each run. Results are saved after every attempt, so an
interrupted run retains completed measurements. An existing directory is never
overwritten. `--validate` checks reference solutions without loading any model.
Every generation run also validates its selected reference solutions first.

A normal run plans once per arm. The typed planner is `typed-cangjie generate --plan-only`.
The Cangjie planner is `plain-llama` with the Cangjie specification and no token cap.
Each arm then generates from its own plan. The typed arm's stdout is already Cangjie.
Cangjie-arm programs are compiled as generated. Both are scored with the
checked-in `std.unittest` files under [cjtests/](cjtests/); the Cangjie arm calls
`solve` where the translated arm calls `typed_fn_solve`. The 366 `@Test` cases are
inputs for the 12 generated functions, not 366 separate prompts. Reference validation
checks the typed evaluator; it is not a model benchmark result.

## Suite and provenance

The bundled `humaneval-scalar-anf-v1` suite adapts **12 of HumanEval's 164 tasks**
to this project's Int64/Bool language. It contains **366 cases**, including **81
distinct literal upstream input cases**, plus fixed boundary and seeded-random
cases. The cases live in [cjtests/](cjtests/) as one `@Test` / `@Expect` per
input against `typed_fn_solve`. [tasks.json](tasks.json) holds signatures,
prompts, and provenance only. The suite does not use the upstream randomized
loops. These are **not official HumanEval or HumanEval+ scores**.

[HumanEval](https://github.com/openai/human-eval) motivates function-level tests
and pass@k estimation. [EvalPlus](https://github.com/evalplus/evalplus) motivates
testing beyond the original examples; our extra cases are independently written,
not imported HumanEval+ tests. The upstream revision and dataset SHA-256 are
recorded in [tasks.json](tasks.json). HumanEval's MIT notice is retained in
[LICENSE-HumanEval](LICENSE-HumanEval).

| Task ID | HumanEval task | Result |
| --- | --- | --- |
| `he_013_gcd` | 13: greatest_common_divisor | Int64 |
| `he_031_is_prime` | 31: is_prime | Bool |
| `he_041_collisions` | 41: car_race_collision | Int64 |
| `he_049_modp` | 49: modp | Int64 |
| `he_053_add` | 53: add | Int64 |
| `he_055_fibonacci` | 55: fib | Int64 |
| `he_059_prime_factor` | 59: largest_prime_factor | Int64 |
| `he_060_sum_to_n` | 60: sum_to_n | Int64 |
| `he_063_fibfib` | 63: fibfib | Int64 |
| `he_097_unit_digits` | 97: multiply | Int64 |
| `he_138_four_even` | 138: is_equal_to_sum_even | Bool |
| `he_150_prime_choice` | 150: x_or_y | Int64 |

Strings, lists, floats, and tuples cannot be expressed by the current grammar.
The selected tasks use bounded Int64 input domains stated in their prompts, with
all expected results fitting Int64. Inputs with zero, negative values, repeated
factors, sequence boundaries, and large results are included where applicable.
Both arms are asked for `solve`. The typed language reserves names such as `add`,
and the checked-in tests call `typed_fn_solve`. The Cangjie harness rewrites
that call to `solve`. Helpers may precede `solve`, and a dummy `main` completes
the program.

Both arms receive the same function signature and problem description. The typed
arm then receives [docs/language.md](../docs/language.md), and the Cangjie arm
receives [docs/cangjie-subset.md](../docs/cangjie-subset.md). Test inputs,
answers, and reference solutions are not included. Reference programs are
positive controls for the typed evaluator; they are never supplied to the model.

## Experimental controls

- Each task/sample uses the same seed in both arms. Sample index `i` uses
  `--seed + i`, wrapping within the unsigned 32-bit range.
- Both arms leave the forced postfix off. The evaluator does not remove Markdown,
  extract code, reformat output, repair errors, or append a deterministic suffix.
- The request text is the same problem statement. Each arm then adds its own output instructions.
  The typed system message appends `docs/language.md`. The Cangjie system message appends
  `docs/cangjie-subset.md`, an English translation of a CangjieSkills excerpt covering functions, bindings,
  conditions, loops, integers, Booleans, operators, and `main`.
- Planning is enabled by default and is separate for each arm. Both planners see the same task,
  seed, and temperature, and each sees only its own specification.
  `benchmarks/run.py --no-plan` skips planning for both arms. That passes
  `--skip-plan` to `typed-cangjie` and runs `plain-llama` with no plan. A failed
  plan fails that arm only.
- Mode order alternates between pairs. Generations run sequentially to avoid
  loading multiple model instances simultaneously.
- Defaults: one sample per task/mode, 2048 code tokens, temperature 0.8, seed 42,
  physical batch size 64. `--ngl 0` selects CPU inference.
- Fibonacci and FibFib require at least **2048 code tokens**, even if a lower
  `--n` is requested. Both arms use that same budget, recorded in
  `code_tokens_by_task` and the per-attempt results.
- Planning, code generation, compilation, and execution have separate wall-clock
  limits (`600`, `300`, `20`, and `5` seconds by default). The execution limit is
  per `std.unittest` case via `--timeout-each`; the process-group backstop is
  that limit times the case count. Timeouts count as failures, not omitted samples.

Use `--samples 5` or more to estimate pass@5, or `--samples 10` or more for pass@10.
The planning stage has no output token cap; `--planning-timeout` limits wall
time for the benchmark. A failed or empty plan fails that arm
without substituting another plan. `--generation-timeout`
applies only to code generation. Seeds do not guarantee identical
outputs across different hardware or numerical backends.

## Measurements

| Measurement | Typed arm | Cangjie arm |
| --- | --- | --- |
| Translation rate | Complete typed programs translated to Cangjie / all attempts | Not applicable; the source is already Cangjie |
| **Compilation success rate** | Translated Cangjie compiles with standalone `cjc` / all attempts | Standalone `cjc` succeeds / all attempts |
| **Functional correctness rate** | Every test passes / all attempts | Every test passes / all attempts |
| Correct given compiled | Every test passes / compiled candidates | Every test passes / compiled candidates |

An unfinished typed program counts as a translation failure, and `cjc` is
not invoked. The Cangjie arm has no translation step, so its translation cell
in the report is `n/a`. No failures disappear from the primary denominators.

Functional evaluation compiles the candidate with the checked-in `*_test.cj`
file under `cjc --test`. Each input is its own `@Test`. The typed arm calls
`typed_fn_solve`. The Cangjie arm calls `solve`. A throw or wrong answer does
not stop later cases.
The generated dummy `main` is renamed to `benchmarkOriginalMain` and not
executed; `test_entry` is the process entry. A missing entry point or wrong
signature fails harness compilation; the candidate's standalone compilation
result is retained. Wrong answers (`FAILED`), runtime exceptions (`ERROR`),
per-case timeouts, and harness compilation failures are distinguished. A
candidate passes only when the unittest summary reports every case `PASSED`.

The report also includes per-task pass@k using the HumanEval estimator
`1 - C(n-c, k) / C(n, k)`, averaged across tasks. Here `n` is the number of samples
and `c` the number passing all tests. Compilation and execution failures remain
incorrect samples. With one sample per task, pass@1 equals functional correctness
rate. Paired counts show where only the typed arm or only the Cangjie arm
succeeds. A two-task smoke run checks the harness; it is not a reliable comparison
of general model quality.

## Artifacts and verification

- `config.json`: settings, selected tasks, model/plugin and suite hashes, CLI and
  runner hashes, compiler version, Git revision and working-tree status.
- `runner.snapshot.py` and `suite.snapshot.json`: the exact runner and task
  manifest used, including per-task minimum token budgets.
- `results.jsonl`: one row per attempted generation, including failures.
- `plans/<task>/sample_<index>_<mode>/`: that arm's plan, system message, and user
  message. `planning_results.jsonl` records one planning result per arm. Each code
  result records its own `plan_sha256` and plan path.
- `summary.json` and `report.md`: aggregate rates, failure categories, per-task
  results, pass@k, and paired comparisons. Partial runs are marked incomplete.
- `references.json` and `references/`: positive-control results and artifacts.
- Each attempt directory contains its system and user messages, raw output, Cangjie
  source, renamed candidate for testing, copied `*_test.cj`, unittest binary,
  stage commands, return codes, timings, stdout, and stderr. A typed attempt that
  exits 2 did not produce Cangjie, so `cjc` is not run.

If one arm's planning fails, that attempt is recorded as a planning failure and
`code_generation_attempted` is false. Those failures remain in the end-to-end
denominators; `code_generation_attempts` reports how many code jobs actually ran.

Outputs go under the Git-ignored `benchmark-results/` directory by default.
Model runs take an exclusive lock so two benchmark runners cannot accidentally
load competing model instances. Interrupting or terminating the runner stops its
active child process group. Engine failures such as GPU out-of-memory errors
invalidate the comparison and stop the run; they are not reported as bad model
code or ordinary compilation failures.
Execution has CPU, file-size, and core-dump limits in addition to process-group
timeouts. The typed arm writes Cangjie directly. Checked-in typed references are
translated with `emit-cangjie` before `cjc`. The Cangjie arm is compiled as generated.

`ctest` includes model-free harness tests when Python is available. With `cjc`,
these also run all reference cases and check rejection, wrong answers, missing
entry points, and nontermination. No automated test loads GPT-OSS or any other
model weights.
