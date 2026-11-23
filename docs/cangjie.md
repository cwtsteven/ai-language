# Cangjie translation

`typed-cangjie task` samples the [typed language](language.md) and, when that
program is complete and well-typed, writes standalone Cangjie. Translation needs
no extra command and no Cangjie SDK. An unfinished sample writes a diagnostic
to stderr and no source. This is not the benchmark's direct Cangjie arm: that
arm samples Cangjie with `plain-llama` and the [Cangjie excerpt](cangjie-subset.md).

```sh
./build/typed-cangjie task 'Compute Fibonacci(5) iteratively.' --model /path/to/model.gguf --n 512 > fibonacci.cj
cjc fibonacci.cj -o fibonacci --set-runtime-rpath
./fibonacci
echo $?  # 5
```

Checked-in `.typed` fixtures are still the constraint language. The benchmark
and runtime tests translate those with `emit-cangjie`, which reads one program
on stdin. Model output does not go through that step.

`main` keeps its `Int64` return value; the generated executable uses it as its
exit status, rather than printing it. Operating systems may truncate exit status
values, so use a Cangjie harness to inspect larger results.

| Source | Cangjie |
| --- | --- |
| `Int64`, `Bool` | Native `Int64`, `Bool` |
| `let` | `let` |
| `var` | `var` |
| `name = value;` | `name = value` |
| `add`, `sub`, `mul`, `eq`, `lt` | Native arithmetic and comparison expressions |
| `not`, `and`, `or` | Native Boolean expressions |
| `func`, calls, `if`, `while`, `return`, `break`, `continue` | Corresponding Cangjie control flow |

`let` copies the current value. `var` is re-read by later uses and by `while`
conditions, and assignment updates that binding only. User functions and
variables receive separate name prefixes to avoid Cangjie keywords and generated
runtime names. Decimal literals are normalized without converting
through a smaller host integer type. Arithmetic uses Cangjie's integer overflow
behavior.

The grammar permits functions without a return on every path. If execution
reaches the end of such a generated function, it throws an exception identifying
the function. The generator does not invent a return value or reject those source
programs. Statements after an unconditional return remain in the output and may
produce an unreachable-code warning from `cjc`.

The generator validates with `typed::Prefix`, parses a structured syntax tree,
then emits Cangjie. `cj-codegen-test` runs without an SDK. CMake adds
`cj-runtime-test` when it finds `cjc`; this compiles and executes fixtures for
Fibonacci, recursive factorial, value copies, mutable `Bool` loops, loop control,
identifier collisions, integer limits, and fallthrough exceptions. Tested with
Cangjie 1.1.0 on macOS arm64.
