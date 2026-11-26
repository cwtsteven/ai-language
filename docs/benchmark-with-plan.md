# Benchmark: with planning

Paired comparison of the typed pipeline (`typed-decode-cj`) against direct Cangjie (`plain-llama`) on suite `humaneval-scalar-anf-v1` (**35** tasks, **592** unittest cases). Each arm plans from the same task, seed, and temperature, then generates from its own plan. Plans are not shared. Specifications differ: [language.md](language.md) for typed, [cangjie-subset.md](cangjie-subset.md) for Cangjie.

## Setup

| Setting | Value |
| --- | --- |
| Model | Qwen3.5 9B Q4_K_M GGUF (local weights; not in this repo) |
| Temperature | 0.8 |
| Seed | 42 (one sample per task) |
| Code budget | 2048 tokens per attempt |
| Planning | on; each arm’s local model planner, no output token cap |
| Forced postfix | off |

Generated = the model finished within the code budget. Compiled = `cjc` accepted the candidate. Tests = passed unittest cases / 592. Correct = tasks where every case passed (pass@1 with `n=1`). A failed or empty plan fails that arm only and skips code generation for it. No code extraction, repair, formatting, or deterministic completion is applied. An unfinished typed program is a translation failure (`cjc` is not run). These are not official HumanEval scores. How to reproduce: [README](../README.md#benchmarks) and [benchmarks/README.md](../benchmarks/README.md).

## Summary

| Mode | Generated | Compiled | Tests | Correct |
| --- | ---: | ---: | ---: | ---: |
| constrained | 20/35 | 19/35 | 260/592 | 8/35 |
| cangjie | 35/35 | 5/35 | 104/592 | 4/35 |

## Per task

| Task | Budget | constrained generated | constrained compiled | constrained tests | cangjie generated | cangjie compiled | cangjie tests |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| he_003_below_zero | 2048 | true | true | 11/11 | true | false | 0/11 |
| he_005_intersperse | 2048 | true | true | 2/7 | true | false | 0/7 |
| he_009_rolling_max | 2048 | true | true | 1/8 | true | false | 0/8 |
| he_011_string_xor | 2048 | false | false | 0/7 | true | false | 0/7 |
| he_013_gcd | 2048 | false | false | 0/31 | true | false | 0/31 |
| he_015_string_sequence | 2048 | true | true | 0/6 | true | false | 0/6 |
| he_018_how_many_times | 2048 | false | false | 0/8 | true | false | 0/8 |
| he_023_strlen | 2048 | true | true | 1/6 | true | true | 6/6 |
| he_024_largest_divisor | 2048 | true | true | 4/10 | true | false | 0/10 |
| he_025_factorize | 2048 | false | false | 0/13 | true | false | 0/13 |
| he_030_get_positive | 2048 | false | false | 0/8 | true | false | 0/8 |
| he_031_is_prime | 2048 | true | true | 28/44 | true | false | 0/44 |
| he_034_unique | 2048 | false | false | 0/6 | true | false | 0/6 |
| he_035_max_element | 2048 | false | false | 0/6 | true | false | 0/6 |
| he_041_collisions | 2048 | true | true | 14/14 | true | true | 14/14 |
| he_042_incr_list | 2048 | true | true | 3/6 | true | false | 0/6 |
| he_043_pairs_sum | 2048 | true | true | 13/13 | true | false | 0/13 |
| he_048_palindrome | 2048 | true | true | 6/11 | true | false | 0/11 |
| he_049_modp | 2048 | false | false | 0/33 | true | false | 0/33 |
| he_052_below_threshold | 2048 | false | false | 0/10 | true | false | 0/10 |
| he_053_add | 2048 | true | true | 41/41 | true | true | 41/41 |
| he_054_same_chars | 2048 | true | false | 0/11 | true | false | 0/11 |
| he_055_fibonacci | 2048 | false | false | 0/21 | true | false | 0/21 |
| he_056_angle_brackets | 2048 | false | false | 0/16 | true | false | 0/16 |
| he_057_monotonic | 2048 | true | true | 13/13 | true | false | 0/13 |
| he_059_prime_factor | 2048 | true | true | 11/18 | true | false | 0/18 |
| he_060_sum_to_n | 2048 | true | true | 37/37 | true | false | 0/37 |
| he_061_parens | 2048 | false | false | 0/16 | true | false | 0/16 |
| he_063_fibfib | 2048 | true | true | 4/21 | true | false | 0/21 |
| he_085_even_at_odd | 2048 | false | false | 0/8 | true | false | 0/8 |
| he_097_unit_digits | 2048 | true | true | 32/32 | true | false | 0/32 |
| he_126_sorted_limited | 2048 | true | true | 1/17 | true | false | 0/17 |
| he_138_four_even | 2048 | true | true | 38/38 | true | true | 38/38 |
| he_150_prime_choice | 2048 | false | false | 0/36 | true | false | 0/36 |
| he_163_even_digits | 2048 | false | false | 0/9 | true | true | 5/9 |
