# ai-language

Generate programs in a small typed language in A-normal form (ANF), using local
llama.cpp models. The decoder enforces the language's grammar and types.
The `cj` command translates
checked programs into Cangjie source, which you can compile and run with `cjc`.
Typechecking does not guarantee algorithm correctness.

## Build

Requires macOS, Xcode command line tools, CMake 3.16+, and Git.

```sh
brew install cmake git
git clone --recurse-submodules https://github.com/cwtsteven/ai-language.git
cd ai-language
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON -DGGML_METAL_EMBED_LIBRARY=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

For CPU builds, replace the Metal options with `-DGGML_METAL=OFF -DGGML_BLAS=OFF`
and run tasks with `--ngl 0`. Tests need no model weights. If CMake finds `cjc`,
they also compile and run generated Cangjie; the SDK is otherwise optional.

## Use

`task` requires your own compatible GGUF file or [JSON model plugin](docs/model-plugins.md).
The other commands need no model. Run from the repository root:

```sh
./build/typed-llama task 'Return the sum of 2 and 3.' --model /path/to/model.gguf --n 128
./build/typed-llama check 'main(): Int64 { return 5; }'
./build/typed-llama complete 'main(): Int64 { return '
./build/typed-llama format - < examples/fibonacci-5-break.typed
```

`check` accepts complete programs or valid prefixes; `complete` finishes a valid
prefix without a model. `format` and `cj` require a complete program.
These commands accept quoted code or `-` for stdin; `format` and `cj` default to
stdin when code is omitted. Invalid input exits 2. A valid unfinished prefix
passes `check` with `complete=no`.

## Task options

By default, `task` first generates an implementation plan, then generates code
using that plan and the language grammar. Planning has no output token cap;
`--n` limits only code generation. The plan is shown only in debug mode.
Code goes to stdout; diagnostics go to stderr. Each command has `--help`.

| Option | Meaning / default |
| --- | --- |
| `--model FILE` | Required GGUF weights or JSON plugin. |
| `--n N` | Maximum generated code tokens; default **256**. |
| `--temperature T` | Sampling randomness for planning and code generation. Default **0.8**; **0** selects greedy decoding. |
| `--seed N` | Fix the sampling seed (0–4294967295); default random, printed to stderr. |
| `--no-plan` | Skip planning and generate code directly from the task and grammar. |
| `--prefix CODE` | Continue a valid program prefix; default empty. |
| `--prompt TEXT` | Use a raw prompt instead of the task description; skip planning and automatic grammar/chat formatting. |
| `--raw` | Stream code as tokens arrive; otherwise buffer and format it. |
| `--unconstrained` | Disable code token filtering and typecheck the result afterward. |
| `--debug` | Show the implementation plan and token traces on stderr; default off. |
| `--debug-limit N` | Rejected candidates shown per code token; default **20**. Enables debug mode. |
| `--ngl N` | GPU layers: plugin setting, otherwise **99** on macOS. Use **0** for CPU inference. |
| `--ubatch N` | Physical batch size: plugin setting, otherwise llama.cpp's default. |
| `-h`, `--help` | Command help. |

Raising the temperature increases the chance of choosing lower-scoring tokens;
greedy decoding always chooses the highest-scoring token. Code generation accepts
only tokens that preserve syntax and type correctness, unless `--unconstrained`
is set. If `--n` runs out, constrained decoding finishes the program
deterministically and reports this on stderr; the result may not fulfill the task.

## Fibonacci example

```sh
./build/typed-llama task "$(cat examples/fibonacci-5-prompt.txt)" --model /path/to/model.gguf --n 512
```

The [prompt](examples/fibonacci-5-prompt.txt) supplies the algorithm explicitly.
To translate and run the checked [reference program](examples/fibonacci-5-break.typed),
install the Cangjie SDK and run:

```sh
./build/typed-llama cj - < examples/fibonacci-5-break.typed > fibonacci.cj
cjc fibonacci.cj -o fibonacci --set-runtime-rpath
./fibonacci
echo $?  # 5: the program's return value is its exit status
```

## Language

Immutable `let` bindings; `Int64`, `Bool`, `RefInt64`, and `RefBool`; `!cell` reads
and `assign(cell, value);` writes an atomic value. An atom is a literal, an
in-scope variable, or a reference read. Statements require semicolons. Conditions
are Boolean atoms; `break;` and `continue;` are allowed only inside loops. Typed `func`
declarations go before `main`; see the [function example](examples/functions.typed).
Computations use calls such as `add(x, 1)` and `eq(x, 5)`, with atomic arguments;
infix expressions are not supported.

- [Single language specification](docs/language.md) (also embedded in model prompts at build time)
- [Model plugin settings](docs/model-plugins.md)
- [Cangjie generation](docs/cangjie.md)
- [Checker and decoder internals](docs/decoder.md)
