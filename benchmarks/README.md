# Typed pipeline vs direct Cangjie

Compare one local model on the same task, token budget, temperature, and seed. The typed arm uses `typed-cangjie`, appends [docs/language.md](../docs/language.md), and writes Cangjie. The Cangjie arm uses `plain-llama`, appends [docs/cangjie-subset.md](../docs/cangjie-subset.md), and does not run the typed checker. Requires Python 3.10+, both binaries, and the Cangjie SDK.

```sh
cmake --build build --parallel 4
python3 benchmarks/run.py --validate
python3 benchmarks/run.py --model /path/to/model.gguf --output benchmark-results/my-run
```

A shorter run:

```sh
python3 benchmarks/run.py --model /path/to/model.gguf \
    --tasks he_053_add he_055_fibonacci --samples 1 \
    --output benchmark-results/smoke
```

Model selection matches `generate`: a GGUF file or a JSON plugin. Use a new output directory each time. Results are saved after every attempt, and an existing directory is never overwritten. `--validate` checks reference solutions without a model; every generation run validates its selected references first.

The runner has two generation modes. Typed stdout is already Cangjie.

End to end is the default. Each arm plans from the prompt, then generates from that plan. The typed planner is `typed-cangjie generate --plan-only`. The Cangjie planner is `plain-llama` with the Cangjie specification and no token cap.

```sh
python3 benchmarks/run.py --model /path/to/model.gguf \
    --output benchmark-results/end-to-end
```

Given-plan mode skips the planner and generates from saved plans. `--plans` accepts a previous `--plan-only` directory, or one folder per task with `typed.txt` and `cangjie.txt`. A per-task file is reused for every sample. A sample file (`plans/<task>/sample_<index>_<mode>/plan.txt`) is used only for that sample. A missing plan fails that arm only.

```sh
python3 benchmarks/run.py --model /path/to/model.gguf --plan-only \
    --output benchmarks/plans/qwen35-9b-seed42
python3 benchmarks/run.py --model /path/to/model.gguf \
    --plans benchmarks/plans/qwen35-9b-seed42 \
    --output benchmark-results/from-saved-plans
python3 benchmarks/run.py --model /path/to/model.gguf \
    --plans benchmarks/plans/composer-2.5-fast \
    --output benchmark-results/from-given-plans
```

The Qwen3.5-9B plans for seed 42 and temperature 0.8 are in [plans/qwen35-9b-seed42](plans/qwen35-9b-seed42). The Composer 2.5 Fast plans, one typed plan and one Cangjie plan per task, are in [plans/composer-2.5-fast](plans/composer-2.5-fast).

Cangjie-arm programs are compiled as generated. Both arms are scored with the `std.unittest` files in [cjtests/](cjtests/). The Cangjie arm calls `solve`; the translated arm calls `typed_fn_solve`. The 592 `@Test` cases are inputs for the 35 generated functions, not 592 prompts. Reference validation checks the typed evaluator; it is not a model score.

## Suite and provenance

The bundled `humaneval-scalar-anf-v1` suite adapts **35 of HumanEval's 164 tasks** to `Int64`, `Bool`, `ListInt64`, and `String`: **592 cases**, of which **212** are distinct literal upstream inputs, plus fixed boundary cases. [cjtests/](cjtests/) has one `@Test` / `@Expect` per input against `typed_fn_solve`. [tasks.json](tasks.json) holds signatures, prompts, and provenance. The suite does not use the upstream randomized loops. These are not official HumanEval or HumanEval+ scores.

[HumanEval](https://github.com/openai/human-eval) motivates function-level tests
and pass@k estimation. [EvalPlus](https://github.com/evalplus/evalplus) motivates
testing beyond the original examples; our extra cases are independently written,
not imported HumanEval+ tests. The upstream revision and dataset SHA-256 are
recorded in [tasks.json](tasks.json). HumanEval's MIT notice is retained in
[LICENSE-HumanEval](LICENSE-HumanEval).

| Task ID | HumanEval task | Result |
| --- | --- | --- |
| `he_003_below_zero` | 3: below_zero | Bool |
| `he_005_intersperse` | 5: intersperse | ListInt64 |
| `he_009_rolling_max` | 9: rolling_max | ListInt64 |
| `he_011_string_xor` | 11: string_xor | String |
| `he_013_gcd` | 13: greatest_common_divisor | Int64 |
| `he_015_string_sequence` | 15: string_sequence | String |
| `he_018_how_many_times` | 18: how_many_times | Int64 |
| `he_023_strlen` | 23: strlen | Int64 |
| `he_024_largest_divisor` | 24: largest_divisor | Int64 |
| `he_025_factorize` | 25: factorize | ListInt64 |
| `he_030_get_positive` | 30: get_positive | ListInt64 |
| `he_031_is_prime` | 31: is_prime | Bool |
| `he_034_unique` | 34: unique | ListInt64 |
| `he_035_max_element` | 35: max_element | Int64 |
| `he_041_collisions` | 41: car_race_collision | Int64 |
| `he_042_incr_list` | 42: incr_list | ListInt64 |
| `he_043_pairs_sum` | 43: pairs_sum_to_zero | Bool |
| `he_048_palindrome` | 48: is_palindrome | Bool |
| `he_049_modp` | 49: modp | Int64 |
| `he_052_below_threshold` | 52: below_threshold | Bool |
| `he_053_add` | 53: add | Int64 |
| `he_054_same_chars` | 54: same_chars | Bool |
| `he_055_fibonacci` | 55: fib | Int64 |
| `he_056_angle_brackets` | 56: correct_bracketing | Bool |
| `he_057_monotonic` | 57: monotonic | Bool |
| `he_059_prime_factor` | 59: largest_prime_factor | Int64 |
| `he_060_sum_to_n` | 60: sum_to_n | Int64 |
| `he_061_parens` | 61: correct_bracketing | Bool |
| `he_063_fibfib` | 63: fibfib | Int64 |
| `he_085_even_at_odd` | 85: add | Int64 |
| `he_097_unit_digits` | 97: multiply | Int64 |
| `he_126_sorted_limited` | 126: is_sorted | Bool |
| `he_138_four_even` | 138: is_equal_to_sum_even | Bool |
| `he_150_prime_choice` | 150: x_or_y | Int64 |
| `he_163_even_digits` | 163: generate_integers | ListInt64 |

Floats, tuples, nested lists, lists of strings, and optional results cannot be expressed. Expected results fit `Int64`, within the bounds stated in each prompt. Zero, negative values, repeated factors, sequence boundaries, and large results are included where they apply.

Both arms are asked for `solve`. The typed language reserves built-in call names such as `emptyList` and `listLen`, so the checked-in tests call `typed_fn_solve` and the Cangjie harness rewrites that call to `solve`. Helpers may precede `solve`, and a dummy `main` completes the program. Both arms get the same signature and problem text, then their own specification. Test inputs, answers, and reference solutions are not shown to the model. References are positive controls for the typed evaluator.

## Experimental controls

- Each task/sample uses the same seed in both arms. Sample index `i` uses
  `--seed + i`, wrapping within the unsigned 32-bit range.
- Both arms leave the forced postfix off. The evaluator does not remove Markdown,
  extract code, reformat output, repair errors, or append a deterministic suffix.
- The request text is the same problem statement, followed by that arm's output instructions. The typed system message appends `docs/language.md`. The Cangjie system message appends `docs/cangjie-subset.md`.
- Planning is separate for each arm. Both planners see the same task, seed, and temperature, and only their own specification. The default run plans, then generates. `--plans DIR` generates from saved plans and does not call the planner. `--no-plan` skips planning for both. `--plan-only` writes plans and does not generate. A failed or missing plan fails that arm only.
- Mode order alternates between pairs. Generations run sequentially to avoid
  loading multiple model instances simultaneously.
- Defaults: one sample per task/mode, temperature 0.8, seed 42, physical batch
  size 64. `--ngl 0` selects CPU inference.
- Each task's `--n` is twice the model's token count of its typed reference,
  or **2048**, whichever is higher. Both arms use that budget. `model_tokens_by_task`
  and `code_tokens_by_task` record the count and the budget.
- Planning, code generation, compilation, and execution have separate wall-clock
  limits (`600`, `300`, `20`, and `5` seconds by default). The execution limit is
  per `std.unittest` case via `--timeout-each`; the process-group backstop is
  that limit times the case count. Timeouts count as failures, not omitted samples.

Use `--samples 5` or more for pass@5, or `--samples 10` or more for pass@10. Planning has no output token cap; `--planning-timeout` limits its wall time. A failed or empty plan fails that arm, with no substitute plan. `--generation-timeout` applies only to code generation. A seed does not fix results across hardware or numerical backends.

## Measurements

| Measurement | Typed arm | Cangjie arm |
| --- | --- | --- |
| Translation rate | Complete typed programs translated to Cangjie / all attempts | Not applicable; the source is already Cangjie |
| **Compilation success rate** | Translated Cangjie compiles with standalone `cjc` / all attempts | Standalone `cjc` succeeds / all attempts |
| **Functional correctness rate** | Every test passes / all attempts | Every test passes / all attempts |
| Correct given compiled | Every test passes / compiled candidates | Every test passes / compiled candidates |

An unfinished typed program is a translation failure, and `cjc` is not invoked. The Cangjie arm has no translation step, so that cell is `n/a`. Failures stay in the primary denominators.

Functional evaluation compiles the candidate with its `*_test.cj` under `cjc --test`. Each input is its own `@Test`. The typed arm calls `typed_fn_solve`; the Cangjie arm calls `solve`. A throw or a wrong answer does not stop later cases. The generated `main` is renamed to `benchmarkOriginalMain` and not executed; `test_entry` is the process entry. A missing entry point or wrong signature fails harness compilation, while the candidate's standalone compilation result is kept. `FAILED`, `ERROR`, per-case timeouts, and harness compilation failures are counted separately. A candidate passes only when every case is `PASSED`.

Per-task pass@k uses the HumanEval estimator `1 - C(n-c, k) / C(n, k)`, averaged across tasks. `n` is the sample count and `c` is the number that pass every test. Compilation and execution failures remain incorrect. With one sample per task, pass@1 equals the functional correctness rate. Paired counts show where only one arm succeeds. A two-task smoke run checks the harness; it is not a model comparison.

## Artifacts and verification

- `config.json`: settings, selected tasks, model/plugin and suite hashes, CLI and
  runner hashes, compiler version, Git revision and working-tree status.
- `runner.snapshot.py` and `suite.snapshot.json`: the exact runner and task
  manifest used. `code_tokens_by_task` records each task's code budget.
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

A planning failure sets `code_generation_attempted` to false and stays in the end-to-end denominators. `code_generation_attempts` counts code jobs that actually ran.

Output defaults to the Git-ignored `benchmark-results/` directory. A model run takes an exclusive lock. Stopping the runner stops its child process group. An engine failure, such as GPU out-of-memory, invalidates the comparison and stops the run; it is not scored as bad model code. Execution also has CPU, file-size, and core-dump limits. The typed arm writes Cangjie directly. Checked-in typed references go through `emit-cangjie` before `cjc`. The Cangjie arm is compiled as generated.

When Python is available, `ctest` runs the harness without a model. With `cjc`, those tests also run every reference case and check rejection, wrong answers, missing entry points, and nontermination. No automated test loads model weights.
