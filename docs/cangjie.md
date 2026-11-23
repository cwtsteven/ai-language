# Cangjie translation

`cj` translates a complete, well-typed program in the [typed language](language.md)
to standalone Cangjie source. Emitting that source needs no model and no Cangjie SDK.
This is not the benchmark's
direct Cangjie arm: that arm samples Cangjie with `plain-llama` and the
[Cangjie excerpt](cangjie-subset.md). `cj` only translates a program the checker
has already accepted.

`task` writes the sampled token stream unless `--format` is set. `cj` accepts
that stream when the program is complete and well-typed.

```sh
./build/typed-llama cj - < examples/fibonacci-5-break.typed > fibonacci.cj
cjc fibonacci.cj -o fibonacci --set-runtime-rpath
./fibonacci
echo $?  # 5
```

You can also pass a quoted program as the argument, or omit `-` to read stdin.
To translate model output, pipe `task` into `cj`:

```sh
./build/typed-llama task 'Compute Fibonacci(5) iteratively.' --model /path/to/model.gguf --n 512 |
    ./build/typed-llama cj > fibonacci.cj
```

The command checks the whole input before emitting anything. Invalid or
unfinished input exits 2 with a diagnostic on stderr and no source on stdout.
`main` keeps its `Int64` return value; the generated executable uses it as its
exit status, rather than printing it. Operating systems may truncate exit status
values, so use a Cangjie harness to inspect larger results.

| Source | Cangjie |
| --- | --- |
| `Int64`, `Bool` | Native `Int64`, `Bool` |
| `RefInt64`, `RefBool` | Generated `TypedIntCell`, `TypedBoolCell` classes |
| `refInt(value)`, `refBool(value)` | Cell constructors |
| `!cell` | `cell.value` |
| `assign(cell, value);` | `cell.value = value` |
| `add`, `sub`, `mul`, `eq`, `lt` | Native arithmetic and comparison expressions |
| `not`, `and`, `or` | Native Boolean expressions |
| `func`, calls, `if`, `while`, `return`, `break`, `continue` | Corresponding Cangjie control flow |

Cells are class instances, so aliases, function arguments, and returned cells
share mutations. Loop conditions reread cell fields on each iteration. User
functions and variables receive separate name prefixes to avoid Cangjie keywords
and generated runtime names. Decimal literals are normalized without converting
through a smaller host integer type. Arithmetic uses Cangjie's integer overflow
behavior.

The grammar permits functions without a return on every path. If execution
reaches the end of such a generated function, it throws an exception identifying
the function. The generator does not invent a return value or reject those source
programs. Statements after an unconditional return remain in the output and may
produce an unreachable-code warning from `cjc`.

The generator validates with `typed::Prefix`, parses a structured syntax tree,
then emits Cangjie. `cj-codegen-test` and CLI tests run without an SDK. CMake adds
`cj-runtime-test` when it finds `cjc`; this compiles and executes fixtures for
Fibonacci, recursive factorial, reference aliasing, Boolean cells, loop control,
identifier collisions, integer limits, and fallthrough exceptions. Tested with
Cangjie 1.1.0 on macOS arm64.
