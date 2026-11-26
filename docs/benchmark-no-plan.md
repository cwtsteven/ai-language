# Benchmark: without planning

Paired comparison of the typed pipeline (`typed-decode-cj`) against direct Cangjie (`plain-llama`) on suite `humaneval-scalar-anf-v1` (**35** tasks, **592** unittest cases). Both arms skip the planner (`--no-plan`): each sees only the shared request text and its own language specification ([language.md](language.md) vs [cangjie-subset.md](cangjie-subset.md)).

## Setup

| Setting | Value |
| --- | --- |
| Model | Qwen3.5 9B Q4_K_M GGUF (local weights; not in this repo) |
| Temperature | 0.8 |
| Seed | 42 (one sample per task) |
| Code budget | 2048 tokens per attempt |
| Planning | off for both arms |
| Forced postfix | off |

Generated = the model finished within the code budget. Compiled = `cjc` accepted the candidate. Tests = passed unittest cases / 592. On the constrained row, percentages are relative change versus cangjie: (constrained - cangjie) / cangjie. No code extraction, repair, formatting, or deterministic completion is applied. An unfinished typed program is a translation failure (`cjc` is not run). These are not official HumanEval scores. How to reproduce: [README](../README.md#benchmarks) and [benchmarks/README.md](../benchmarks/README.md).

## Summary

| Mode | Generated | Compiled | Tests |
| --- | ---: | ---: | ---: |
| cangjie | 35/35 | 3/35 | 84/592 |
| constrained | 20/35 (-43%) | 14/35 (+367%) | 155/592 (+85%) |

## Per task

| Task | Budget | constrained generated | constrained compiled | constrained tests | cangjie generated | cangjie compiled | cangjie tests |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| he_003_below_zero | 2048 | true | true | 11/11 | true | false | 0/11 |
| he_005_intersperse | 2048 | false | false | 0/7 | true | false | 0/7 |
| he_009_rolling_max | 2048 | true | true | 1/8 | true | false | 0/8 |
| he_011_string_xor | 2048 | false | false | 0/7 | true | false | 0/7 |
| he_013_gcd | 2048 | true | false | 0/31 | true | false | 0/31 |
| he_015_string_sequence | 2048 | false | false | 0/6 | true | false | 0/6 |
| he_018_how_many_times | 2048 | true | true | 4/8 | true | false | 0/8 |
| he_023_strlen | 2048 | true | false | 0/6 | true | false | 0/6 |
| he_024_largest_divisor | 2048 | false | false | 0/10 | true | false | 0/10 |
| he_025_factorize | 2048 | false | false | 0/13 | true | false | 0/13 |
| he_030_get_positive | 2048 | true | true | 3/8 | true | false | 0/8 |
| he_031_is_prime | 2048 | false | false | 0/44 | true | false | 0/44 |
| he_034_unique | 2048 | false | false | 0/6 | true | false | 0/6 |
| he_035_max_element | 2048 | true | true | 3/6 | true | false | 0/6 |
| he_041_collisions | 2048 | true | false | 0/14 | true | false | 0/14 |
| he_042_incr_list | 2048 | true | true | 1/6 | true | false | 0/6 |
| he_043_pairs_sum | 2048 | true | true | 11/13 | true | false | 0/13 |
| he_048_palindrome | 2048 | true | false | 0/11 | true | true | 11/11 |
| he_049_modp | 2048 | true | true | 6/33 | true | false | 0/33 |
| he_052_below_threshold | 2048 | true | true | 10/10 | true | false | 0/10 |
| he_053_add | 2048 | true | true | 41/41 | true | true | 41/41 |
| he_054_same_chars | 2048 | false | false | 0/11 | true | false | 0/11 |
| he_055_fibonacci | 2048 | true | false | 0/21 | true | false | 0/21 |
| he_056_angle_brackets | 2048 | false | false | 0/16 | true | false | 0/16 |
| he_057_monotonic | 2048 | true | true | 12/13 | true | false | 0/13 |
| he_059_prime_factor | 2048 | false | false | 0/18 | true | false | 0/18 |
| he_060_sum_to_n | 2048 | false | false | 0/37 | true | false | 0/37 |
| he_061_parens | 2048 | false | false | 0/16 | true | false | 0/16 |
| he_063_fibfib | 2048 | true | false | 0/21 | true | false | 0/21 |
| he_085_even_at_odd | 2048 | false | false | 0/8 | true | false | 0/8 |
| he_097_unit_digits | 2048 | true | true | 12/32 | true | true | 32/32 |
| he_126_sorted_limited | 2048 | true | true | 4/17 | true | false | 0/17 |
| he_138_four_even | 2048 | false | false | 0/38 | true | false | 0/38 |
| he_150_prime_choice | 2048 | true | true | 36/36 | true | false | 0/36 |
| he_163_even_digits | 2048 | false | false | 0/9 | true | false | 0/9 |
