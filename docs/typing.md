# Typing rules

This file records the type judgments for the typed language. It is not embedded in prompts. The surface forms live in [language.md](language.md). The checker that enforces these rules is described in [decoder.md](decoder.md).

A judgment `Γ ⊢ e : T` means expression `e` has type `T` under environment `Γ`. A judgment `Γ ⊢ s` means statement `s` is well-typed under `Γ`. Bindings introduced by a form are written next to the premise that adds them.

## Operators

```text
Γ ⊢ a : Int64    Γ ⊢ b : Int64
─────────────────────────────────────────
Γ ⊢ a / b : Int64    Γ ⊢ a % b : Int64

Γ ⊢ a : Int64    Γ ⊢ b : Int64
──────────────────────────────────────────────────────────────
Γ ⊢ a == b : Bool   Γ ⊢ a != b : Bool
Γ ⊢ a < b : Bool    Γ ⊢ a <= b : Bool
Γ ⊢ a > b : Bool    Γ ⊢ a >= b : Bool

Γ ⊢ a : String    Γ ⊢ b : String
────────────────────────────────────
Γ ⊢ a == b : Bool   Γ ⊢ a != b : Bool
```

`/` truncates toward zero. `%` is `a - b * (a / b)`. A zero divisor is a runtime error. The checker does not prove non-zero divisors.

## if

```text
Γ ⊢ cond : Bool
Γ ⊢ then
─────────────────────────
Γ ⊢ if (cond) { then }

Γ ⊢ cond : Bool
Γ ⊢ then
Γ ⊢ else
──────────────────────────────────────
Γ ⊢ if (cond) { then } else { else }
```

`else` is optional. Bodies are statement lists and have no type.

## for-in

```text
Γ ⊢ xs : List<T>
Γ, i : T ⊢ body
──────────────────────────────
Γ ⊢ for (i: T in xs) { body }

Γ ⊢ start : Int64
Γ ⊢ end : Int64
T = Int64
Γ, i : Int64 ⊢ body
──────────────────────────────────────
Γ ⊢ for (i: T in start .. end) { body }
```

- The colon and type are required. The checker does not infer `T`.
- `i` is a fresh immutable binding. It cannot shadow a visible name, and it is not assignable. Its scope ends at the loop's `}`.
- A list loop requires the written type to be the element type of `xs`. A mismatch is rejected.
- A range loop requires the written type and both endpoints to be `Int64`. `Bool`, `String`, a list, or a struct cannot be a range endpoint. The range is half-open: `start`, then `start + 1`, stopping before `end`. When `start >= end`, the body runs zero times.
- `start .. end` is not a value and not a `List<Int64>`. `..` is legal only in this header. It cannot be stored or passed.
- `String` is not iterable. Characters are read with indexing.
- `break;` and `continue;` are valid in the body, including in a nested `if`, and target the nearest enclosing loop (`for` or `while`). `continue;` advances to the next element or the next integer.
