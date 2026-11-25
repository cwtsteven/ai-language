# typed-cangjie

Typed constrained code generation for a subset of Cangjie. A local llama.cpp model samples
a small typed language, and a complete program is translated to Cangjie before
anything is written. You compile that output with `cjc`.
Typechecking does not guarantee algorithm correctness.

## Build

Requires CMake 3.16+, a C++17 compiler, and Git. Tests need no model weights.
If CMake finds `cjc`, they also compile and run generated Cangjie; the SDK is
otherwise optional.

### macOS

Metal is on by default, and its shader library is embedded in the binary.

```sh
brew install cmake git
git clone --recurse-submodules https://github.com/cwtsteven/typed-cangjie.git
cd typed-cangjie
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Configure with `-DGGML_METAL=OFF` for a build that has no Metal backend.

### Linux

This is a CPU build. `GGML_NATIVE` stays on, so the binary uses the host CPU (AVX-512 on this class of Xeon).

```sh
sudo apt install cmake g++ git
git clone --recurse-submodules https://github.com/cwtsteven/typed-cangjie.git
cd typed-cangjie
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

GPU offload needs a backend compiled in. Add `-DGGML_CUDA=ON` for NVIDIA or `-DGGML_VULKAN=ON` for Vulkan, then rebuild.

## Use

`generate` requires your own compatible GGUF file or [JSON model plugin](docs/model-plugins.md).
The other commands need no model. Run from the repository root:

```sh
./build/typed-cangjie generate 'Return the sum of 2 and 3.' --model /path/to/model.gguf --n 128
./build/typed-cangjie check 'main(): Int64 { return 5; }'
./build/typed-cangjie complete 'main(): Int64 { return '
./build/typed-cangjie format - < examples/fibonacci-5-break.typed
```

`generate` writes a Cangjie program when the typed sample is complete, and writes
nothing when it is not. `check`, `complete`, and `format` inspect the typed
language without a model. `check` accepts complete programs or valid prefixes;
`complete` finishes a valid prefix. `format` requires a complete typed program.
These commands accept quoted code or `-` for stdin; `format` defaults to stdin
when code is omitted. Invalid input exits 2 and prints a compiler-style error
on stderr. A valid unfinished prefix passes `check` with `complete=no`.

## Generate options

By default, `generate` first produces an implementation plan, then generates code
using that plan and the language grammar. Planning has no output token cap;
`--n` limits only code generation. The plan is shown only in debug mode.
Code goes to stdout; diagnostics go to stderr. Each command has `--help`.

| Option | Meaning / default |
| --- | --- |
| `--model FILE` | Required GGUF weights or JSON plugin. |
| `--n N` | Maximum generated code tokens; default **256**. |
| `--temperature T` | Sampling randomness for planning and code generation. Default **0.8**; **0** selects greedy decoding. |
| `--seed N` | Fix the sampling seed (0–4294967295); default random, printed to stderr. |
| `--skip-plan` | Skip planning and generate code directly from the description and grammar. |
| `--plan-only` | Write only the implementation plan to stdout, without generating code. |
| `--plan-file FILE` | Generate code from a saved plan; skip new planning. |
| `--force-postfix` | If the token budget ends an open program, append a typed postfix and then translate. Off by default. |
| `--unconstrained` | Disable code token filtering. Translate only when the text is a complete typed program. |
| `--debug` | Show the implementation plan, token traces, and typed program on stderr; default off. |
| `--debug-limit N` | Rejected candidates shown per code token; default **20**. Enables debug mode. |
| `--threads N` | CPU threads for prompt processing and generation. Default **4**. |
| `--ngl N` | GPU layers to offload. Default **99** on macOS and **0** elsewhere. **0** runs on the CPU. |
| `--ubatch N` | Physical prompt batch size. Default **512**, limited to the prompt length. |
| `-h`, `--help` | Command help. |

Higher temperature makes lower-scoring tokens more likely; temperature 0 is greedy. Code tokens must keep the program syntactically and type-correct unless `--unconstrained` is set. A complete program is translated to Cangjie. If `--n` runs out first, stdout stays empty. `--force-postfix` closes that prefix, translates it, and says so on stderr; the constructed ending may not fulfill the task.

Save a plan with `generate TEXT --model FILE --plan-only > plan.txt`, then pass `--plan-file plan.txt`. The [benchmark](benchmarks/README.md) does not share that plan. It runs `typed-cangjie` and `plain-llama` on the same task, seed, temperature, and token budget. The Cangjie arm uses [its own excerpt](docs/cangjie-subset.md).

## Fibonacci example

```sh
./build/typed-cangjie generate "$(cat examples/fibonacci-5-prompt.txt)" --model /path/to/model.gguf --n 512 > fibonacci.cj
cjc fibonacci.cj -o fibonacci --set-runtime-rpath
./fibonacci
echo $?  # 5: the program's return value is its exit status
```

The [prompt](examples/fibonacci-5-prompt.txt) supplies the algorithm explicitly.
`generate` writes the Cangjie program; there is no separate translation command.

## Language

`let` is immutable. `var` is updated with `name = value;`. Types are `Int64`, `Bool`, `ListInt64`, and `String`. Conditions are `Bool` expressions. `break;` and `continue;` occur only inside loops. Helpers are `func` declarations before `main`; see the [function example](examples/functions.typed). Values may nest (`1 + 2 * 3`, `n + 1 == 0`, `xs[i + 1]`). Generic functions take explicit type arguments (`id<Int64>(n)`). A written function type may hold a named function or an annotated lambda (`fun(x: Int64): Int64 => x + 1`). The [specification](docs/language.md) lists every form.

- [Typed language specification](docs/language.md) (embedded in `typed-cangjie` prompts at build time)
- [Cangjie prompt excerpt](docs/cangjie-subset.md) (appended by the benchmark's Cangjie arm)
- [Model plugin settings](docs/model-plugins.md)
- [Cangjie translation](docs/cangjie.md) (`generate` writes the translated program)
- [Checker and decoder internals](docs/decoder.md)
- [Typed pipeline vs direct Cangjie](benchmarks/README.md)
