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
- Python 3.8+ and curl for the optional model download helper. No pip packages.
- For the default 9B model, allow about 5.68 GB for weights plus build files.
  Plan for at least 16 GB RAM; actual usage depends on context length.
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
brew install cmake git python
```

macOS includes curl. For Metal, use Apple's compiler from the command line tools.
If shader compilation reports a missing Metal toolchain, install the toolchain
requested by Xcode, or use the CPU build below.

### Linux (Ubuntu / Debian)

```sh
sudo apt update
sudo apt install -y build-essential cmake git python3 curl ca-certificates
```

Other distributions need the equivalent C/C++ compiler, CMake, Git, Python, and
curl packages. NVIDIA/CUDA support is optional and outside this initial setup.

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

## Check code without a model

```sh
./build/typed-llama --check 'main(): Int64 { let x: Int64 = 2 + 3; return x; }'
./build/typed-llama --check 'main(): Int64 { let x: Int64 = 2 +'
./build/typed-llama --complete 'main(): Int64 { let x: Int64 = 2 +'
```

`--check` reports prefix viability and whether the program is complete. A viable
but unfinished prefix exits successfully; an invalid prefix exits with code 2.
`--complete` prints the prefix plus a deterministically constructed valid suffix.

## Pretty-print a program

The formatter needs no model. It uses four-space indentation, puts statements
on separate lines, normalizes operator spacing, and prints dereferences as
`!cell`. It preserves literal text (including signs and leading zeroes).

```sh
./build/typed-llama --format 'main():Int64{let x:RefInt64=refInt(5);x:=!x+1;return !x;}'
./build/typed-llama --format - < program.txt
```

Output:

```text
main(): Int64 {
    let x: RefInt64 = refInt(5);
    x := !x + 1;
    return !x;
}
```

To format the model's generated output:

```sh
./build/typed-llama --task 'Return the sum of 2 and 3.' --n 128 --format
```

`--format` alongside `--task` (or `--model` with a raw prompt) buffers generation
and prints the formatted program once it is finished. Token selection still
uses the incremental type checker. Debug messages remain on stderr.

Standalone `--format -` reads stdin; standalone `--format` without an argument
also reads stdin. Invalid, ill-typed, or unfinished input produces an
error on stderr and exits with code 2, without printing a formatted program.
Formatting does not complete or repair input.

## Download a model

The default is [Qwen3.5-9B Q4_K_M from Unsloth](https://huggingface.co/unsloth/Qwen3.5-9B-GGUF).
The helper downloads a pinned revision, resumes interrupted transfers, and
verifies the file's SHA-256. It reuses an existing file if its checksum matches.
The weights have their own model license; see the linked model card.

```sh
python3 scripts/download_model.py
```

This creates `models/Qwen3.5-9B-Q4_K_M.gguf`. Weights and build outputs are
excluded from Git. To store the model on an external SSD instead:

```sh
# macOS example; choose an existing writable mount on your machine.
python3 scripts/download_model.py --output-dir /Volumes/MySSD/ai-language-models
./build/typed-llama --model /Volumes/MySSD/ai-language-models/Qwen3.5-9B-Q4_K_M.gguf --task 'Return the sum of 2 and 3.' --n 128
```

On Linux, use your SSD's mount path, such as `/mnt/ssd/ai-language-models`, and
pass that file path to `--model`. The drive must remain mounted during inference.
Other compatible GGUF models can also be passed with `--model`; `--task` needs
a supported model chat template. The pinned llama.cpp revision determines
which model architectures are supported.

To try GPT-OSS-20B with the pinned Q4_K_M weights (approximately 11.62 GB):

```sh
python3 scripts/download_model.py --model gpt-oss-20b --output-dir /Volumes/MySSD/ai-language-models
./build/typed-llama --model /Volumes/MySSD/ai-language-models/gpt-oss-20b-Q4_K_M.gguf --task 'Return the sum of 2 and 3.' --n 128 --ngl 99 --ubatch 64
```

Choose an existing writable mount for `--output-dir`, or omit it to use `models/`.
On a 16 GB Mac, keep the context short and allow memory for macOS and other apps.
GPT-OSS uses its Harmony final-answer channel for constrained code generation.

## Model plugins

Supply any local GGUF supported by the bundled llama.cpp with `--model FILE.gguf`.
The runner reads its chat template and architecture from the file. Model-specific
prompt adapters live in `src/model_plugin.cpp`, independently of the typed decoder.

For reusable settings or a custom prompt format, supply a JSON plugin:

```sh
./build/typed-llama --model-plugin plugins/qwen3.5-9b.json --task 'Return the sum of 2 and 3.' --format
./build/typed-llama --model-plugin plugins/gpt-oss-20b.json --task 'Return the sum of 2 and 3.' --format
```

The GPT-OSS plugin starts with 16 GPU layers and a 64-token microbatch to reduce
GPU memory pressure. Adjust these for your hardware. On the tested 16 GB M4,
full GPU offload failed with insufficient memory; the CPU/GPU split completed
the sum example but took approximately two minutes, including loading and setup.

Create your own plugin without recompiling:

```json
{
  "name": "My local model",
  "model": "../models/my-model.gguf",
  "gpu_layers": 16,
  "ubatch": 64
}
```

`model` paths are relative to the plugin file's directory. Absolute paths also
work. `--model`, `--ngl`, and `--ubatch` override the corresponding plugin settings.
Omit `model` to reuse an adapter with weights supplied through `--model`.

| Optional field | Meaning |
| --- | --- |
| `name` | Descriptive label; defaults to the file's stem. |
| `chat_template` | A llama.cpp built-in template name, such as `chatml`, or a template recognized by its formatter. Defaults to the GGUF's template. |
| `prompt_template` | A complete custom prompt containing `{{system}}` and `{{task}}`. Use this when the built-in formatter does not support the model's format. |
| `assistant_prefix` | Text or special tokens appended before generated code. An empty string disables the automatic Qwen3.5/GPT-OSS prefix. |
| `gpu_layers` | Number of layers to offload; zero uses the CPU. Defaults to the CLI's platform default. |
| `ubatch` | Positive physical batch size. Defaults to llama.cpp's setting. |

Choose `chat_template` or `prompt_template`. The custom template inserts the
language rules and user request once, without evaluating placeholders inside
either value. Custom templates receive no automatic assistant prefix; specify
one explicitly if needed. See `plugins/custom.example.json` for a ChatML example.
`--prompt` continues to accept a fully prepared raw prompt.

Plugins are data files and do not load executable code. They currently use local
GGUF inference. A remote provider would need the typed sampler on its server or
an API with sufficient token selection control to preserve the same guarantee.
A normal chat API could support generation followed by checking and retries.

## Ask the model for a program

After the default model download:

Qwen3.5-9B Q4_K_M is the default when no model or plugin is selected. Every
`--task` prompt includes the language rules and formal grammar, whether type
constraints are enabled or `--unconstrained` is used. Custom plugins receive
the same rules and grammar through `{{system}}`.

```sh
./build/typed-llama --task 'Return the sum of 2 and 3.' --n 128
```

Example output:

```text
main(): Int64 {
  let x: Int64 = 2 + 3;
  return x;
}
```

For a CPU build, explicitly set `--ngl 0` (including Intel Macs):

```sh
./build/typed-llama --task 'Return the sum of 2 and 3.' --n 128 --ngl 0
```

| Option | Meaning |
| --- | --- |
| `--task TEXT` | Natural-language request, sent with the language rules through the model's chat template. |
| `--model FILE` | GGUF weights; defaults to `models/Qwen3.5-9B-Q4_K_M.gguf` when using `--task`. |
| `--n N` | Maximum generated model tokens, default 256. Tokens are text pieces, not words. |
| `--ngl N` | GPU layers: default 99 on macOS, 0 elsewhere. Use 0 for CPU. |
| `--ubatch N` | Maximum tokens processed per physical batch; smaller values reduce peak prompt memory. Defaults to llama.cpp's setting. |
| `--prefix CODE` | Seed the checker and model with an existing valid program prefix. |
| `--debug` | Print rejected and selected candidates to stderr. Code stays on stdout. |
| `--debug-limit N` | Maximum rejected candidates printed per step (default 20). Also enables debug. |
| `--unconstrained` | Use ordinary greedy decoding with the same prompt, then report checker validity. |
| `--prompt TEXT` | Raw prompt instead of `--task`; requires `--model`. |
| `--check CODE` | Check a prefix without loading a model. |
| `--complete CODE` | Construct a valid suffix without loading a model. |
| `--format [CODE]` | With generation, pretty-print the finished output. Standalone, format CODE or read stdin (`-` or no argument). |

The sampler ends generation after a complete program. If `--n` is exhausted,
it constructs a typed suffix and prints a diagnostic on stderr. That suffix
may not fulfill your request. Decoding is greedy in both comparison modes.

To watch rejected tokens or compare with prompt-only generation:

```sh
./build/typed-llama --task 'Return the sum of 2 and 3.' --debug 2> trace.log
./build/typed-llama --task 'Return the sum of 2 and 3.' --unconstrained
./build/typed-llama --task 'Create an integer cell containing 5, increase it to 6, and return its contents.' --prefix 'main(): Int64 { '
```

For Qwen3.5, the CLI prefills a completed thinking block before code generation.
For GPT-OSS, it prefills the Harmony final-answer header.
The model can still produce logically incorrect programs, including incorrect
or infinite loops; type validity does not prove behavior or termination.

## Fibonacci prompt example

A more explicit algorithm prompt is saved in [prompts/fibonacci-5.txt](prompts/fibonacci-5.txt).
It specifies F(0)=0, F(1)=1, the loop invariant, and the update order. In a local
Qwen3.5-9B trial it generated the intended loop; the checker accepted it, and
inspection shows it returns 5. This supplies the algorithm to the model, so it
does not demonstrate that the model can discover Fibonacci's algorithm unaided.

```sh
./build/typed-llama --task "$(cat prompts/fibonacci-5.txt)" --n 512 --format
./build/typed-llama --format - < examples/fibonacci-5.typed
```

The generated example is in [examples/fibonacci-5.typed](examples/fibonacci-5.typed).
There is no runtime interpreter in this project yet; behavioral correctness here
was checked by inspection, not by executing the generated program.

## Troubleshooting

- **Missing llama.cpp:** run `git submodule update --init --recursive`.
- **Cannot load the model:** download it first, check the path, and ensure any
  external SSD is mounted. Use an absolute `--model` path from another directory.
- **Metal initialization error:** use `--ngl 0`; if necessary, reconfigure with
  `-DGGML_METAL=OFF -DGGML_BLAS=OFF` and rebuild for CPU.
- **Memory pressure:** shorten `--n` and prompts, close other large applications,
  or supply a smaller compatible GGUF using `--model`.
- **SHA-256 mismatch:** move or delete the file named in the error and retry the
  helper. Do not use the file until verification succeeds.

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
