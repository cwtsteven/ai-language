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
| `--n N` | Maximum generated tokens; default **256**. |
| `--prefix CODE` | Seed with a valid prefix; default empty. |
| `--prompt TEXT` | Prepared raw prompt instead of the positional task description. |
| `--raw` | Stream unformatted output; default is buffered and formatted. |
| `--unconstrained` | Disable token constraints and check the result; default off. |
| `--debug` | Trace selected/rejected tokens to stderr; default off. |
| `--debug-limit N` | Rejections shown per step; default **20**. Enables tracing. |
| `--ngl N` | GPU layers: plugin setting, otherwise **99 on macOS / 0 elsewhere**. |
| `--ubatch N` | Physical batch size: plugin setting, otherwise llama.cpp's default. |
| `-h`, `--help` | Command help. |

Task descriptions receive the language rules and grammar. `--prompt` bypasses
that formatting. If the token budget runs out, constrained decoding constructs
a typed suffix and reports this on stderr; the suffix may not fulfill the task.

## Fibonacci example

```sh
./build/typed-llama task "$(cat examples/fibonacci-5-prompt.txt)" --model /path/to/model.gguf --n 512
```

The [prompt](examples/fibonacci-5-prompt.txt) supplies the algorithm explicitly.
A [reference program](examples/fibonacci-5-break.typed) uses `while (true)` and `break;`.

## Language

Immutable `let` bindings; `Int64`, `Bool`, `RefInt64`, and `RefBool`; `!cell` reads
and `cell := value` writes. Statements require semicolons. Conditions are Boolean
atoms; `break;` and `continue;` are allowed only inside loops.

- [Grammar and type rules](docs/language.md)
- [Model plugin settings](docs/model-plugins.md)
- [Checker and decoder internals](docs/decoder.md)
