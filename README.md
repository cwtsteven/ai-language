# ai-language

Generate programs in a small Cangjie-inspired language using type-constrained
local decoding with llama.cpp. This prototype checks code; it does not execute it
or guarantee algorithm correctness.

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

For CPU builds, replace the Metal options with `-DGGML_METAL=OFF -DGGML_BLAS=OFF`.
Tests do not load model weights or run inference.

## Use

Supply your own compatible GGUF file or [JSON model plugin](docs/model-plugins.md).
There is no default model or download helper. Run from the repository root:

```sh
./build/typed-llama task 'Return the sum of 2 and 3.' --model /path/to/model.gguf --n 128
./build/typed-llama check 'main(): Int64 { return 5; }'
./build/typed-llama complete 'main(): Int64 { return '
./build/typed-llama format - < examples/fibonacci-5-break.typed
```

`check`, `complete`, and `format` need no model. Use `-` to read code from stdin;
`format` also reads stdin when its argument is omitted. Invalid code exits 2.
A viable unfinished prefix passes `check` with `complete=no`.

## Task options

`task --help` lists all generation options. Each other command also has `--help`.

| Option | Meaning / default |
| --- | --- |
| `--model FILE` | Required GGUF weights or JSON plugin. |
| `--n N` | Maximum generated code tokens; default **256**. |
| `--temperature T` | Both passes sample at **0.8** by default; **0** selects greedy. |
| `--seed N` | Repeat a sampling seed (0–4294967295); default random, printed to stderr. |
| `--no-plan` | Skip the default unconstrained planning pass. |
| `--prefix CODE` | Seed with a valid prefix; default empty. |
| `--prompt TEXT` | Prepared raw prompt instead of the positional task description. |
| `--raw` | Stream unformatted output; default is buffered and formatted. |
| `--unconstrained` | Disable token constraints and check the result; default off. |
| `--debug` | Trace selected/rejected tokens to stderr; default off. |
| `--debug-limit N` | Rejections shown per step; default **20**. Enables tracing. |
| `--ngl N` | GPU layers: plugin setting, otherwise **99 on macOS / 0 elsewhere**. |
| `--ubatch N` | Physical batch size: plugin setting, otherwise llama.cpp's default. |
| `-h`, `--help` | Command help. |

Tasks first produce an implementation plan on stderr until the model ends its response,
then generate code using that plan and the language specification. `--no-plan` skips the first
pass; `--prompt` bypasses planning and task formatting. If the code budget runs
out, constrained decoding constructs
a typed suffix and reports this on stderr; the suffix may not fulfill the task.

Code sampling selects only type-valid tokens. Use `--seed 42` to repeat a run
with the same model, prompt, and settings; `--temperature 0` uses greedy decoding.

## Fibonacci example

```sh
./build/typed-llama task "$(cat examples/fibonacci-5-prompt.txt)" --model /path/to/model.gguf --n 512
```

The [prompt](examples/fibonacci-5-prompt.txt) supplies the algorithm explicitly.
A [reference program](examples/fibonacci-5-break.typed) uses `while (true)` and `break;`.

## Language

Immutable `let` bindings; `Int64`, `Bool`, `RefInt64`, and `RefBool`; `!cell` reads
and `cell := value` writes. Statements require semicolons. Conditions are Boolean
atoms; `break;` and `continue;` are allowed only inside loops. Typed `func`
declarations go before `main`; see the [function example](examples/functions.typed).

- [Single language specification](docs/language.md) (also embedded in model prompts at build time)
- [Model plugin settings](docs/model-plugins.md)
- [Checker and decoder internals](docs/decoder.md)
