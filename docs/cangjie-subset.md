# Cangjie specification excerpt

This file is the Cangjie counterpart of [docs/language.md](language.md).
`plain-llama` appends it to the benchmark's Cangjie planning and code prompts.
`typed-cangjie` does not. The pages are copied from CangjieSkills,
https://github.com/SunriseSummer/CangjieSkills branch `cangjie-1.0.5`, under
`references/language`. They cover the same kind of fragment as the typed
specification: functions, bindings, conditions, loops, integers, Booleans,
operators, and `main`.

Generator comments and parent navigation links are omitted. One upstream page
included a complete `main` that reassigns a `let`; that program is omitted here
so this prompt does not contain a whole program. The prose below is an English
translation of the remaining upstream text. Code examples are unchanged. Those
pages describe Cangjie 1.0.5. The local compiler used for scoring may be newer.

Source pages:
- `references/language/function/1-函数定义/1-1-基本语法.md`
- `references/language/function/1-函数定义/1-2-参数列表.md`
- `references/language/function/1-函数定义/1-3-返回类型.md`
- `references/language/function/1-函数定义/1-4-函数体.md`
- `references/language/basic_concepts/2-程序结构/2-2-变量定义.md`
- `references/language/basic_concepts/3-表达式/3-2-if-条件判断.md`
- `references/language/basic_concepts/3-表达式/3-3-while-do-while-循环.md`
- `references/language/basic_concepts/3-表达式/3-5-break-continue.md`
- `references/language/basic_data_type/1-整数类型/1-1-类型列表.md`
- `references/language/basic_data_type/1-整数类型/1-2-整数字面量.md`
- `references/language/basic_data_type/1-整数类型/1-5-整数支持的运算.md`
- `references/language/basic_data_type/11-基本运算符/11-2-算术运算符.md`
- `references/language/basic_data_type/11-基本运算符/11-4-关系运算符.md`
- `references/language/basic_data_type/11-基本运算符/11-5-逻辑运算符.md`
- `references/language/basic_data_type/3-布尔类型.md`
- `references/language/package/3-程序入口/3-1-main-函数.md`


## Source: `references/language/function/1-函数定义/1-1-基本语法.md`

# 1.1 Basic syntax


A function starts with `func`, then declares its name, parameter list, optional return type, and body. The last expression in the body can be the return value.

```cangjie cjtest=syntax id=syntax-42d5419be4-1 form=unit
func add(a: Int64, b: Int64): Int64 {
    a + b
}
```
- Starts with the keyword `func`
- Parameters are inside `()`, separated by commas
- The return type follows `:` after the parameter list (optional; the compiler can infer it)
- The body is inside `{}`


## Source: `references/language/function/1-函数定义/1-2-参数列表.md`

# 1.2 Parameter list


- **Positional parameter**: `p: T` — a positional parameter, with no label at the call
- **Named parameter**: `p!: T` — the definition uses a `!` suffix; the call uses `p: value` (**the call does not write `!`**)
- **Named parameter with a default**: `p!: T = expr` — only a named parameter may have a default, assigned with `=`
- **Order**: positional parameters come before named parameters. A positional parameter cannot follow a named parameter
- **Immutability**: every function parameter is immutable inside the function body and cannot be assigned again
- **Scope**: a parameter name is visible from its definition through the end of the function body. Redefining the same name inside the body is an error
- **Named-parameter example**:
  ```cangjie cjtest=syntax id=syntax-67a1fac9ca-1 form=stmt
  // indent is a named parameter whose default value is 2
  func formatNumber(value: Int64, indent!: Int64 = 2): String {
      " " * indent + value.toString()
  }

  // The call passes indent by name and does not write !
  println(formatNumber(42, indent: 4))  // "    42"

  // Omitting the named parameter uses its default
  println(formatNumber(42))  // "  42" (indent uses the default 2)
  ```


## Source: `references/language/function/1-函数定义/1-3-返回类型.md`

# 1.3 Return type


- It may be **written explicitly** or **omitted** (the compiler infers it)
- When it is declared, every `return e` expression and the body's final expression must be a subtype of the declared type
- When the return type is `Unit`, the compiler inserts `return ()` at every return point
- If inference fails, the compiler reports an error


## Source: `references/language/function/1-函数定义/1-4-函数体.md`

# 1.4 Function body


- Contains variable definitions, expressions, and optional nested function definitions
- **`return expr`**: stops execution and returns `expr` (it must match the return type)
- **`return`**: equivalent to `return ()`, and requires the return type `Unit`
- **The type of a `return` expression is `Nothing`**
- **The body's type** is the type of its last item: an expression has that expression's type; a variable definition, function declaration, or empty body has type `Unit`

---


## Source: `references/language/basic_concepts/2-程序结构/2-2-变量定义.md`

# 2.2 Variable definitions


- Syntax: `modifier name: type = value`
- Modifiers:
  - `let`: immutable, assigned once
  - `var`: mutable, may be assigned more than once
  - `const`: a compile-time constant
- The type annotation may be omitted when it can be inferred
- Global and static variables must be initialized
- A local variable may be initialized later, but it must be assigned before use

A `let` binding cannot be assigned again. After `let answer = 41`, writing `answer = 42` is rejected with the diagnostic `cannot assign to immutable value`. A `var` binding can be assigned.


## Source: `references/language/basic_concepts/3-表达式/3-2-if-条件判断.md`

# 3.2 if conditions


- Syntax: `if (cond) { exprs1 } else { exprs2 }`. `cond` is a `Bool` expression
- `else if` chains are supported
- When used as a value: with `else`, the type is decided by context or by the least common supertype of the branches, and the value is the last expression of the branch that runs. Without `else`, the type is always `Unit`
- A condition may use `let pattern <- expr` for pattern matching


## Source: `references/language/basic_concepts/3-表达式/3-3-while-do-while-循环.md`

# 3.3 while / do-while loops


- Syntax: `while (cond) { exprs }` and `do { exprs } while (cond)`. `cond` is a `Bool` expression
- The type is always `Unit`
- A condition may use `let pattern <- expr` for pattern matching


## Source: `references/language/basic_concepts/3-表达式/3-5-break-continue.md`

# 3.5 break / continue


- `break` leaves the loop. `continue` skips to the next iteration
- Both have type `Nothing`


## Source: `references/language/basic_data_type/1-整数类型/1-1-类型列表.md`

# 1.1 Integer types


- **Signed integers**: `Int8`, `Int16`, `Int32`, `Int64`, `IntNative`
- **Unsigned integers**: `UInt8`, `UInt16`, `UInt32`, `UInt64`, `UIntNative`
- A signed N-bit range is −2^(N−1) through 2^(N−1)−1. An unsigned N-bit range is 0 through 2^N−1
- `IntNative` and `UIntNative` have a platform-dependent width
- The default integer type is `Int64`. Without context, an integer literal is inferred as `Int64`


## Source: `references/language/basic_data_type/1-整数类型/1-2-整数字面量.md`

# 1.2 Integer literals


- **Binary**: prefix `0b` or `0B`, for example `0b00011000`
- **Octal**: prefix `0o` or `0O`, for example `0o30`
- **Decimal**: no prefix, for example `24`
- **Hexadecimal**: prefix `0x` or `0X`, for example `0x18`
- Underscores `_` may separate digits: `0b0001_1000`
- A value outside the type's range is a compile error


## Source: `references/language/basic_data_type/1-整数类型/1-5-整数支持的运算.md`

# 1.5 Operations on integers


- Arithmetic, bitwise operations, relational operations, increment and decrement (`++` and `--`), and compound assignment
- An integer can be converted to another integer type, to or from a floating-point type, and to `Rune`


## Source: `references/language/basic_data_type/11-基本运算符/11-2-算术运算符.md`

# 11.2 Arithmetic operators


| Operator | Meaning | Operand types |
| --- | --- | --- |
| `-` (unary) | Negation | Numeric types |
| `+` | Addition, also string concatenation | The same numeric type |
| `-` | Subtraction | The same numeric type |
| `*` | Multiplication | The same numeric type |
| `/` | Division. Integer division truncates toward zero | The same numeric type |
| `%` | Remainder, `a - b*(a/b)` | Integers only |
| `**` | Exponentiation | `Int64^UInt64→Int64`, `Float64^(Int64\|Float64)→Float64` |

- Overflow is detected and reported as a runtime error


## Source: `references/language/basic_data_type/11-基本运算符/11-4-关系运算符.md`

# 11.4 Relational operators


`==`, `!=`, `<`, `<=`, `>`, `>=`


## Source: `references/language/basic_data_type/11-基本运算符/11-5-逻辑运算符.md`

# 11.5 Logical operators


Quick reference: `!` is not (a unary prefix); `&&` is and; `||` is or.

| Operator | Meaning | Short-circuit |
| --- | --- | --- |
| `!` | Not (unary prefix) | Not applicable |
| `&&` | And | Yes: `false && x` → `false` |
| `\|\|` | Or | Yes: `true \|\| x` → `true` |


## Source: `references/language/basic_data_type/3-布尔类型.md`

# 3. Boolean type


`Bool` has only the values `true` and `false`. It is used in conditions and logical operations.

- Type: `Bool`
- Literals: `true`, `false`
- Supported operators:
  - Logical: `!` (not), `&&` (and), `||` (or)
  - Relational: `==`, `!=`
  - Compound assignment: `&&=`, `||=`

---


## Source: `references/language/package/3-程序入口/3-1-main-函数.md`

# 3.1 The `main` function


- `main` is the entry point. A root package has at most one
- It cannot have an access modifier
- It is not imported when the package is imported
- Parameters: none, or `Array<String>`
- Return type: `Unit` or an integer type
- The definition does not use the `func` keyword
