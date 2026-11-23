# ai-language

A Cangjie-inspired experimental language with incremental syntax and type
constraints inside llama.cpp's token selection loop. The model proposes tokens;
the custom sampler rejects tokens whose bytes cannot extend to a well-typed
program. This project generates and checks programs; it does not execute them
or guarantee that they implement the requested algorithm.

## Requirements

- macOS with Apple Silicon (Metal GPU or CPU), or Linux (CPU first).
  Intel Macs can use the CPU build below.
- A C/C++ compiler supporting C++17, CMake 3.16 or newer, and Git.
- For generation, supply a local GGUF model supported by the bundled llama.cpp.
  Disk space and RAM requirements depend on your model and context length.
  Checker-only commands and tests need no model or API key.

llama.cpp is built from the pinned Git submodule commit
`184862801a8747ca9fc0a84501419581aa9d37aa`; there is no separate llama.cpp
installation or server to start. The upstream dependency carries its own
[MIT license](https://github.com/ggml-org/llama.cpp/blob/184862801a8747ca9fc0a84501419581aa9d37aa/LICENSE).

## Install dependencies

### macOS

Install Apple's command line tools if needed:

```sh
xcode-select --install
```

With [Homebrew](https://brew.sh/) installed:

```sh
brew install cmake git
```

For Metal, use Apple's compiler from the command line tools.
If shader compilation reports a missing Metal toolchain, install the toolchain
requested by Xcode, or use the CPU build below.

### Linux (Ubuntu / Debian)

```sh
sudo apt update
sudo apt install -y build-essential cmake git ca-certificates
```

Other distributions need the equivalent C/C++ compiler, CMake, and Git packages.
NVIDIA/CUDA support is optional and outside this initial setup.

## Clone and build

```sh
git clone --recurse-submodules https://github.com/cwtsteven/ai-language.git
cd ai-language
```

For a private repository, authenticate with GitHub first (for example using
`gh auth login`), or clone using your configured GitHub SSH key. If you already
cloned without submodules:

```sh
git submodule update --init --recursive
```

**Apple Silicon with Metal:**

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON -DGGML_METAL_EMBED_LIBRARY=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

**Linux CPU, or macOS CPU:**

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=OFF -DGGML_BLAS=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Use fewer parallel jobs if memory is limited. Run the commands below from the
repository root. GitHub Actions builds the CLI and runs checker tests on macOS
and Linux without downloading a model.

## Generate a program

```sh
./build/typed-llama --model /path/to/model.gguf --task 'Return the sum of 2 and 3.'
```

Generation requires `--model`: supply the path to your own GGUF weights or a
JSON model plugin. There is no default model, bundled model, or download helper.
Keep weights wherever you choose, including on an external drive.

Each task prompt includes the language rules and formal grammar. Generation
uses type constraints and prints formatted output by default. Output is buffered
until the program is complete, then printed with four-space indentation:

```text
main(): Int64 {
    let x: Int64 = 2 + 3;
    return x;
}
```

| Option | Meaning |
| --- | --- |
| `--task TEXT` | Describe the program to generate. |
| `--model FILE` | Required for generation: GGUF weights or a JSON model plugin. |

Use `--help` for basic usage and `--help-advanced` for development options.
The model can still produce incorrect algorithms or infinite loops; type
validity does not prove behavior or termination.

## Select a model and its settings

The same `--model` option accepts GGUF weights or a JSON plugin:

```sh
./build/typed-llama --model /path/to/model.gguf --task 'Return the sum of 2 and 3.'
./build/typed-llama --model /path/to/my-plugin.json --task 'Return the sum of 2 and 3.'
```

Models must be supported by the bundled llama.cpp. The runner reads the chat
template and architecture from the GGUF. For reusable memory settings or a
custom prompt format, create a plugin without recompiling:

```json
{
  "name": "My local model",
  "model": "/path/to/model.gguf",
  "gpu_layers": 16,
  "ubatch": 64
}
```

The `model` field is required when selecting a plugin. Relative model paths
are resolved against the plugin file's directory; absolute paths also work.
Set `gpu_layers` to `0` for CPU inference, including CPU builds on macOS.

| Optional field | Meaning |
| --- | --- |
| `name` | Descriptive label; defaults to the file's stem. |
| `gpu_layers` | Layers to offload to the GPU. Defaults to 99 on macOS, 0 elsewhere. |
| `ubatch` | Positive physical batch size. Smaller batches reduce peak prompt memory; defaults to llama.cpp's setting. |
| `chat_template` | A llama.cpp built-in template name, such as `chatml`, or a recognized template. Defaults to the GGUF's template. |
| `prompt_template` | A complete custom prompt containing `{{system}}` and `{{task}}`. Use when the built-in formatter does not support the model's format. |
| `assistant_prefix` | Text or special tokens before generated code. An empty string disables the automatic Qwen3.5/GPT-OSS prefix. |

Choose `chat_template` or `prompt_template`. Custom templates receive the
language rules and grammar through `{{system}}` and the request through
`{{task}}`. Values are inserted once; placeholders inside them are not evaluated.
Custom templates receive no automatic assistant prefix. See
`plugins/custom.example.json` for a ChatML example.

Plugins are data files for local inference. Standard remote chat APIs would
require generation followed by checking and retries; enforcing types during
remote generation would require token selection control on the server.

## Check code without a model

```sh
./build/typed-llama --check 'main(): Int64 { let x: Int64 = 2 + 3; return x; }'
./build/typed-llama --check 'main(): Int64 { let x: Int64 = 2 +'
```

`--check` reports prefix viability and whether the program is complete. A viable
but unfinished prefix exits successfully; an invalid prefix exits with code 2.

## Development tools

```sh
./build/typed-llama --help-advanced
```

Advanced options include the token budget (`--n`, default 256), prefix seeding,
raw prompts, tracing, unconstrained generation, and memory overrides. Store
GPU layers and batch size in the model plugin for routine use. Raw `--prompt`
input bypasses automatic task prompting; supply the complete prompt yourself.

Generation normally buffers and formats the complete program. Use `--raw` to
stream unformatted model output, including invalid output during experiments:

```sh
./build/typed-llama --model /path/to/model.gguf --task 'Return the sum of 2 and 3.' --debug 2> trace.log
./build/typed-llama --model /path/to/model.gguf --task 'Return the sum of 2 and 3.' --unconstrained --raw
```

Both constrained and unconstrained `--task` prompts contain the same rules and
grammar. Decoding is greedy in both modes. If the token budget is exhausted,
constrained generation constructs a typed suffix and reports this on stderr;
that suffix may not fulfill the request. Qwen3.5 uses a completed thinking
block before code generation; GPT-OSS uses its Harmony final-answer header.

The standalone formatter and prefix completion utility need no model:

```sh
./build/typed-llama --format 'main():Int64{let x:RefInt64=refInt(5);x:=!x+1;return !x;}'
./build/typed-llama --format - < program.txt
./build/typed-llama --complete 'main(): Int64 { let x: Int64 = 2 +'
```

Standalone `--format` with no argument also reads stdin. Invalid, ill-typed,
or unfinished input exits with code 2 and produces no formatted program.
Formatting preserves literal text and does not complete or repair input.
`--complete` prints the supplied prefix plus a deterministically constructed
valid suffix.

## Fibonacci prompt example

A more explicit algorithm prompt is saved in [examples/fibonacci-5-prompt.txt](examples/fibonacci-5-prompt.txt).
It specifies F(0)=0, F(1)=1, the loop invariant, and the update order. In a local
Qwen3.5-9B trial it generated the intended loop; the checker accepted it, and
inspection shows it returns 5. This supplies the algorithm to the model, so it
does not demonstrate that the model can discover Fibonacci's algorithm unaided.

```sh
./build/typed-llama --model /path/to/model.gguf --task "$(cat examples/fibonacci-5-prompt.txt)" --n 512
./build/typed-llama --format - < examples/fibonacci-5.typed
```

The generated example is in [examples/fibonacci-5.typed](examples/fibonacci-5.typed).
There is no runtime interpreter in this project yet; behavioral correctness here
was checked by inspection, not by executing the generated program.

## Troubleshooting

- **Missing llama.cpp:** run `git submodule update --init --recursive`.
- **Cannot load the model:** download it first, check the path, and ensure any
  external SSD is mounted. Use an absolute `--model` path from another directory.
- **Metal initialization error:** set `gpu_layers` to `0` in your model plugin; if necessary, reconfigure with
  `-DGGML_METAL=OFF -DGGML_BLAS=OFF` and rebuild for CPU.
- **Memory pressure:** shorten `--n` and prompts, close other large applications,
  or supply a smaller compatible GGUF using `--model`.

## Language core

The syntax follows Cangjie's `main(): Int64 { ... }`, typed `let`
declarations and braced control flow, with OCaml-style reference operators. This is a small
Cangjie-inspired language, not a complete Cangjie parser or compiler.
Punctuation may touch surrounding tokens: `add(x, 1)` and `main()` are
accepted, as are spaced versions. Whitespace may contain spaces, tabs, or
newlines. Statements currently require semicolons so the prefix checker can
recognize their ends without relying on line breaks.

```text
program  ::= main ( ) : Int64 { statement* return int_atom ; }
statement::= let identifier : Type = rhs ;
           | ref_identifier := rhs ;
           | if ( bool_atom ) { statement* } else { statement* }
           | while ( bool_atom ) { statement* }
Type     ::= Int64 | Bool | RefInt64 | RefBool
atom     ::= integer | true | false | in_scope_identifier | ! ref_identifier
rhs      ::= atom | atom binary_op atom | operation ( atom [, atom] )
```

Binary operators are `+`, `-`, `*` on `Int64`, `<` and `==` for comparison,
and `&&`, `||`, and `==` on `Bool`. Each operand must be atomic; bind a
subexpression first if needed. The named operations remain available for
compatibility with earlier examples. `RefInt64`, `RefBool`, and the ref
operations are custom extensions, not Cangjie standard library types. `!cell`
is treated as an atom, so it also works in calls, infix operands, conditions,
and returns. `! cell` is accepted too. The two characters of `:=` must be adjacent,
though a model token may split between them.

Infix checking uses a fixed table of operator signatures. At the start of a
right-hand side, an atom is viable if it has the expected result type, or if
at least one table entry accepts its type, produces the expected type, and
has an available right operand. For example, `let b: Bool = 2` is viable
because it can continue as `2 < 1` or `2 == 1`; `2;` and `2 +` are rejected
in that position. After an operator, its signature determines the right
operand's type. Model tokens can split an operator (`=` then `=`) or contain
the whole expression (`2+1`). No complete program is generated to make these
decisions.

Conditions and return values currently accept atoms only. Bind comparisons
before `if`, and use `while (!flag)` with a `RefBool` cell for an updatable loop condition.
There are no chained infix expressions or precedence rules yet.

`Int64` literals are signed 64-bit decimal integers, optionally prefixed
with `-`. Variable names start with a lowercase ASCII letter, then contain
letters, digits, or underscores. Variables cannot be redefined in an active
scope; names introduced inside `if` or `while` blocks disappear at the closing
brace. All bindings are immutable. References can be aliased by another binding;
`cell := rhs` mutates the shared cell without changing its type. Dereferencing
`!cell` produces `Int64` for a `RefInt64`, or `Bool` for a `RefBool`. There is no
`var`, plain assignment, `write()`, `readInt()`, or `readBool()` syntax. The intended runtime model has signed
64-bit integers and garbage-collected reference cells; this prototype checks code
but does not execute it.

| Result | Operations |
| --- | --- |
| `Int64` | `+`, `-`, `*`; `add(Int64, Int64)`, `sub(Int64, Int64)`, `mul(Int64, Int64)`, `!cell` for `cell: RefInt64` |
| `Bool` | `<`, `==`, `&&`, `||`; `eq(Int64, Int64)`, `lt(Int64, Int64)`, `not(Bool)`, `and(Bool, Bool)`, `or(Bool, Bool)`, `!cell` for `cell: RefBool` |
| `RefInt64` | `refInt(Int64)` |
| `RefBool` | `refBool(Bool)` |

Example:

```text
main(): Int64 {
  let flag: RefBool = refBool(true);
  let counter: RefInt64 = refInt(0);
  while (!flag) {
    counter := !counter + 1;
    flag := false;
  }
  return !counter;
}
```

The Fibonacci(5) example from the conversation has a terminating loop in this
syntax. `a` holds F(!i), `b` holds F(!i+1), and it returns `5` after five updates:

```text
main(): Int64 {
  let limit: Int64 = 5;
  let i: RefInt64 = refInt(0);
  let a: RefInt64 = refInt(0);
  let b: RefInt64 = refInt(1);
  let running: RefBool = refBool(true);
  while (!running) {
    let done: Bool = !i == limit;
    if (done) {
      running := false;
    } else {
      let next: Int64 = !a + !b;
      a := !b;
      b := next;
      i := !i + 1;
    }
  }
  return !a;
}
```

## Checker and decoder

`typed::Prefix` consumes arbitrary byte chunks, so model tokens can split an
identifier or span several language tokens. It keeps a parser phase, a block
stack, a typed symbol table, and a partial lexical token. A candidate is
accepted if every byte keeps the current state extendable to a valid typed
program. The check is transactional: rejection leaves the state unchanged.
An end token is accepted only when the whole program has closed. The checker
can also construct a completion from every viable prefix. This prevents a
model that keeps extending a literal or identifier from leaving the output
unfinished at the token budget.

The sampler builds a max heap of candidates by model logit and tests them in
that order, choosing the first viable one. This is **greedy constrained
decoding**. It does not preserve the original sampling distribution. For a
vocabulary of size `V` and `R` rejected high-logit candidates, heap creation
and masking cost `O(V)`, heap removals cost `O(R log V)`, and each prefix test
costs `O(piece length × symbol count)` in this first implementation. A typed
trie would speed up identifier checks.

Completeness here means exact prefix viability **for this defined language**.
Every checker state has a completion: `Int64` and `Bool` have literals; reference
types can be created from those literals; a block can close; and the main
body can return `0`. Calls with unavailable reference arguments are never
offered. This does not yet implement user-defined functions, algebraic data
types, general polymorphism, execution, or a general-purpose compiler. The
checker validates syntax and types; it does not prove termination or freedom
from runtime errors.
