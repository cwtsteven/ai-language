# Cangjie

This file is the Cangjie fragment for a complete program: functions, bindings, conditions, loops, integers, Booleans, operators, lists, strings, and `main`. It describes Cangjie 1.0.5. A newer compiler may accept more.

`Unit` has one value, `()`. A definition, an assignment, a loop, and an empty body have type `Unit`. `Nothing` has no values. It is the type of `return`, `break`, and `continue`, and it is a subtype of every type, so a branch that does not continue may appear where another branch produces a value. A `Rune` is one Unicode character.

## Functions

A function starts with `func`, then its name, parameter list, optional return type, and body. The last expression in the body can be the return value.

```cangjie
func add(a: Int64, b: Int64): Int64 {
    a + b
}
```

- The keyword is `func`.
- Parameters sit inside `()`, separated by commas.
- The return type follows `:` after the parameter list. It may be omitted when the compiler can infer it.
- The body is inside `{}`.

## Parameters

- Positional parameter: `p: T`. The call passes a value with no label.
- Named parameter: `p!: T`. The definition writes `!`; the call writes `p: value` and does not write `!`.
- Default: `p!: T = expr`. Only a named parameter may have a default.
- Positional parameters come before named parameters.
- A parameter is immutable in the body.
- A parameter name is visible from its definition through the end of the body. Declaring that name again in the body is an error.

```cangjie
// indent is a named parameter. Its default is 2.
// toString() renders an Int64 as a String. s * n repeats the string s, n times.
func formatNumber(value: Int64, indent!: Int64 = 2): String {
    " " * indent + value.toString()
}

formatNumber(42, indent: 4)  // "    42"
formatNumber(42)             // "  42"
```

## Return type

- The return type may be written or inferred. If inference fails, the compiler reports an error.
- When it is declared, every `return e` and the body's final expression must be a subtype of that type.
- `return expr` stops execution and returns `expr`.
- `return`, with no expression, is `return ()` and requires the return type `Unit`.
- A `return` expression has type `Nothing`.
- The body's type is the type of its last item. An expression has that expression's type. A variable definition, a function declaration, or an empty body has type `Unit`.
- When the return type is `Unit`, the compiler inserts `return ()` at every return point.
- The body may contain variable definitions, expressions, and nested functions.

## Variables

Syntax: `modifier name: type = value`. The type may be omitted when it can be inferred.

- `let` is immutable and assigned once. `let answer = 41` followed by `answer = 42` is rejected: `cannot assign to immutable value`.
- `var` is mutable and may be assigned again.
- `const` is a compile-time constant.
- Global and static variables must be initialized.
- A local variable may be initialized later, but it must be assigned before use.

## if

`if (cond) { exprs1 } else { exprs2 }`. `cond` is a `Bool` expression. `else if` chains are allowed. A condition may instead be `let pattern <- expr`: `expr` is matched against `pattern`, and the names in the pattern are bound in the branch that runs.

Used as a value, an `if` with `else` has the type required by context, or the least common supertype of the branches. Its value is the last expression of the branch that runs. Without `else`, the type is `Unit`.

## while

`while (cond) { exprs }` and `do { exprs } while (cond)`. `cond` is a `Bool` expression and may be a `let` pattern, as in `if`. The loop's type is `Unit`.

## break and continue

`break` leaves the loop. `continue` skips to the next iteration. Both have type `Nothing`.

## Integers

Signed: `Int8`, `Int16`, `Int32`, `Int64`, `IntNative`. Unsigned: `UInt8`, `UInt16`, `UInt32`, `UInt64`, `UIntNative`. A signed N-bit range is −2^(N−1) through 2^(N−1)−1. An unsigned N-bit range is 0 through 2^N−1. `IntNative` and `UIntNative` follow the platform width. The default integer type is `Int64`; an integer literal with no other context is `Int64`.

Literals may be binary (`0b00011000` or `0B`), octal (`0o30` or `0O`), decimal (`24`), or hexadecimal (`0x18` or `0X`). Underscores may separate digits: `0b0001_1000`. A value outside the type's range is a compile error.

Integers support arithmetic, bitwise operations, relational operations, `++`, `--`, and compound assignment. An integer converts to another integer type, to or from a floating-point type, and to `Rune`.

## Arithmetic

| Operator | Meaning | Operands | Result |
| --- | --- | --- | --- |
| `-` (unary) | Negation | Numeric | The same numeric type |
| `+` | Addition, also string concatenation | The same numeric type, or `String` | The same type |
| `-` | Subtraction | The same numeric type | The same numeric type |
| `*` | Multiplication | The same numeric type | The same numeric type |
| `/` | Division. Integer division truncates toward zero | The same numeric type | The same numeric type |
| `%` | Remainder, `a - b*(a/b)` | Integers | The same integer type |
| `**` | Exponentiation | `Int64` and `UInt64`, or `Float64` and `Int64` or `Float64` | `Int64` or `Float64`, matching the base |

Overflow is a runtime error.

## Relations and logic

`==`, `!=`, `<`, `<=`, `>`, `>=` each take two operands of the same comparable type and return `Bool`.

| Operator | Meaning | Result | Short-circuit |
| --- | --- | --- | --- |
| `!` | Not (unary prefix) | `Bool` | None |
| `&&` | And | `Bool` | `false && x` is `false` |
| `\|\|` | Or | `Bool` | `true \|\| x` is `true` |

## Bool

`Bool` has the values `true` and `false`. It is used in conditions and logic. Its operators, each returning `Bool`, are `!`, `&&`, `||`, `==`, `!=`, `&&=`, and `||=`.

## main

`main` is the entry point. A root package has at most one. It has no access modifier, and importing the package does not import it. Its parameter list is empty or `Array<String>`, a list of the program's string arguments. Its return type is `Unit` or an integer type. The definition does not use `func`.

## Lists and strings

`import std.collection.ArrayList` before using `ArrayList`.

- `ArrayList<Int64>()` has type `ArrayList<Int64>` and is empty.
- `items.add(value)` appends one element.
- `items.size` has type `Int64`.
- `items[index]` has type `Int64`. Indexes start at 0.
- `items.clone()` has type `ArrayList<Int64>` and copies the list.

A string literal is `"text"`. `text.size` has type `Int64` for an ASCII string. `left + right` has type `String`. `left == right` has type `Bool`. `text.toRuneArray()[index]` reads one `Rune`.

```cangjie
func groups(n: Int64): Int64 {
    let small: Bool = n < 10
    if (small) {
        return 0
    } else {
        let rest: Int64 = n - 10
        let counted: Int64 = groups(rest)
        let result: Int64 = counted + 1
        return result
    }
}

main(): Int64 {
    let result: Int64 = groups(25)
    return result
}
```
