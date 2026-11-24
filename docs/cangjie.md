# Cangjie translation

`generate` writes standalone Cangjie once the [typed program](language.md) is complete and well-typed. No extra command or Cangjie SDK is required. An unfinished sample writes a diagnostic to stderr and no source. The benchmark's other arm is `plain-llama`, which samples Cangjie from [the Cangjie excerpt](cangjie-subset.md).

```sh
./build/typed-cangjie generate 'Compute Fibonacci(5) iteratively.' --model /path/to/model.gguf --n 512 > fibonacci.cj
cjc fibonacci.cj -o fibonacci --set-runtime-rpath
./fibonacci
echo $?  # 5
```

Checked-in `.typed` fixtures stay in the constraint language. The benchmark and runtime tests translate them with `emit-cangjie`, which reads one program on stdin. Model output does not.

`main`'s `Int64` is the process exit status, not printed output. Operating systems may truncate it; use a Cangjie harness for larger results.

| Source | Cangjie |
| --- | --- |
| `Int64`, `Bool` | The same native types |
| `let`, `var`, `name = value;` | The same forms |
| `+`, `-`, `*` on `Int64`; `+` on `String` | The same operators |
| `==`, `<` on `Int64`; `==` on `String` | The same operators |
| `!`, `&&`, `\|\|` on `Bool` | The same operators |
| `emptyList()`, `listPush`, `listLen`, `xs[i]` | `ArrayList<Int64>` construction, a copying push, `.size`, and indexing |
| `strLen`, `text[i]` | `.size` and a one-character read helper |
| `func`, calls, `if`, `while`, `return`, `break`, `continue` | The corresponding Cangjie forms |

`let` copies the current value. `var` is read again by later uses and by `while`, and assignment updates that binding only. User functions and variables get separate name prefixes so they do not collide with Cangjie keywords or generated runtime names. Decimal literals are normalized without passing through a smaller host integer. Arithmetic uses Cangjie's overflow behavior.

A function need not return on every path. If execution reaches the end, the generated function throws an exception that names it. The generator does not invent a return value. Statements after an unconditional return stay in the output and may draw an unreachable-code warning from `cjc`.

Translation checks the source with `typed::Prefix`, parses it, and emits Cangjie. `cj-codegen-test` needs no SDK. When CMake finds `cjc`, `cj-runtime-test` compiles and runs the fixtures: Fibonacci, recursive factorial, value copies, mutable `Bool` loops, loop control, identifier collisions, integer limits, and fallthrough exceptions. Tested with Cangjie 1.1.0 on macOS arm64.
