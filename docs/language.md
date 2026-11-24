# Typed language specification

This is the specification of the typed ANF language. The build embeds this file
verbatim in `typed-cangjie` planning and code prompts, and the C++ prefix checker
implements it. The benchmark's direct Cangjie arm does not use this file; it
appends [docs/cangjie-subset.md](cangjie-subset.md) instead.

The syntax follows Cangjie's `main(): Int64 { ... }`, typed `let` and `var`
declarations, and braced control flow. A name reads the binding's current value.
`var` bindings can be updated with `name = atom;`. This is a small
Cangjie-inspired language, not a complete Cangjie parser or compiler.
Punctuation may touch surrounding tokens: `add(x, 1)` and `main()` are
accepted, as are spaced versions. Whitespace may contain spaces, tabs, or
newlines. Statements currently require semicolons so the prefix checker can
recognize their ends without relying on line breaks.

```text
program       ::= function* "main" "(" ")" ":" "Int64" "{" statement* "}"
function      ::= "func" identifier "(" [parameter ("," parameter)*] ")" ":" Type
                  "{" statement* "}"
parameter     ::= identifier ":" Type
statement     ::= "let" identifier ":" Type "=" rhs ";"
                | "var" identifier ":" Type "=" rhs ";"
                | mutable_identifier "=" atom ";"
                | "if" "(" bool_atom ")" "{" statement* "}" "else" "{" statement* "}"
                | "while" "(" bool_atom ")" "{" statement* "}"
                | "break" ";" | "continue" ";"
                | "return" typed_atom ";"
Type          ::= "Int64" | "Bool" | "ListInt64" | "String"
atom          ::= integer | "true" | "false" | string | in_scope_identifier
string        ::= '"' printable-ASCII* '"'
rhs           ::= atom | call
call          ::= callable "(" [atom ("," atom)*] ")"
callable      ::= a built-in operation name or a visible user function name
int_atom      ::= an atom of type Int64
bool_atom     ::= an atom of type Bool
typed_atom    ::= an atom of the enclosing function's declared return type
mutable_identifier ::= an in-scope var of the assigned atom's type
identifier    ::= lowercase ASCII letter followed by ASCII letters, digits, or underscores
integer       ::= optional "-" followed by one or more decimal digits, within signed 64-bit range
```

Quoted strings are literal tokens, `*` means zero or more, and square brackets
mark an optional part. This grammar describes structure; the type and scope
rules below determine which atoms, calls, and operators are valid.

Functions are declared before `main`. Parameters and return values are `Int64`,
`Bool`, `ListInt64`, or `String`. Each helper sees earlier helpers and itself, so self recursion
is allowed; forward calls, mutual recursion, nested functions, and overloading
are not supported. `main` takes no parameters and returns `Int64`.

Calls require exactly the declared number and types of atomic arguments. Bind
call results before using them in another call, condition, or return.
`return atom;` is an ordinary statement and may appear inside `if` or `while`
blocks. It exits the enclosing function. Every returned atom must match that function's declared return
type (`Int64` in `main`). The checker does not require a final return or check
that every path returns; it also typechecks statements after a return.
Parameters and locals are scoped to that body, and cannot shadow
visible function names or built-in operations. See [the recursive factorial
example](../examples/functions.typed).

Arithmetic, comparisons, and Boolean computations use named calls:
`add`, `sub`, `mul`, `eq`, `lt`, `not`, `and`, and `or`. Infix expressions are not
supported. Every call argument must be atomic; bind intermediate call results
with typed `let` or `var` declarations. `let` bindings and parameters cannot be
assigned. `var count: Int64 = 0;` introduces a mutable `Int64`. Using `count`
reads its current value; there is no dereference operator. `count = next;`
stores an atom of the same type. Assignment is a statement with no result.
Bind a computed value first; the right-hand side cannot be a call.

Conditions and return values currently accept atoms only. Bind comparisons
before `if`. A `while` condition that should change must be a `Bool` var updated
in the body, as in `while (running)`. An immutable `Bool` bound before the loop
does not change. For example, bind `let done: Bool = eq(counter, limit);` before
`if (done)`.

`break;` exits the nearest enclosing `while`. `continue;` skips the remaining
statements in that loop's current iteration and rechecks its condition. Both
statements require semicolons and are allowed only inside a loop, including
inside its nested `if` branches. They cannot be used as variable names.
For `while (true)`, use `break;` to leave the loop; updating a local flag does
not change a literal `true` condition. The checker validates these rules but
does not execute the control flow or prove termination.

`Int64` literals are signed 64-bit decimal integers, optionally prefixed
with `-`; leading zeroes are allowed. A `String` literal is a pair of double quotes
around printable ASCII. A space may occur inside the quotes. The literal has no
backslash escapes and cannot cross a line. `""` is the empty string. `ListInt64`
has no literal: `emptyList()` creates an empty list, and `listPush(items, value)`
returns a new list with `value` added at the end. `listLen` and `listGet` read a
list. Indexes start at 0 and must be inside the list; a negative or past-the-end
index is a runtime error. `strLen` counts the characters of an ASCII string,
`strEq` compares two strings, `concat` joins them, and `strAt(text, index)`
returns the one-character string at that character index.
Names start with a lowercase ASCII letter,
then contain ASCII letters, digits, or underscores. Keywords, built-in operation
names, `assign`, `refInt`, `refBool`, `write`, `readInt`, and `readBool` are reserved.
Variables cannot be redefined in an active scope; parameters and locals do not
leak between functions, and names introduced inside `if` or `while` blocks
disappear at the closing brace. `let` and parameters are immutable. `var` stores
a value, so `let copy: Int64 = count;` keeps the value `count` had at that
statement; a later assignment to `count` does not change `copy`. Storing a
`ListInt64` name copies the list, so a later `listPush` does not change the
earlier copy. There is no `:=`, `assign()`, reference cell, or `!` syntax. The intended runtime model has
signed 64-bit integers; this prototype checks code but does not execute it.

| Result | Operations |
| --- | --- |
| `Int64` | `add(Int64, Int64)`, `sub(Int64, Int64)`, `mul(Int64, Int64)`, `listLen(ListInt64)`, `listGet(ListInt64, Int64)`, `strLen(String)` |
| `Bool` | `eq(Int64, Int64)`, `lt(Int64, Int64)`, `not(Bool)`, `and(Bool, Bool)`, `or(Bool, Bool)`, `strEq(String, String)` |
| `ListInt64` | `emptyList()`, `listPush(ListInt64, Int64)` |
| `String` | `concat(String, String)`, `strAt(String, Int64)` |

Full programs are not included here. See [Fibonacci with break](../examples/fibonacci-5-break.typed),
[Fibonacci with a flag](../examples/fibonacci-5.typed),
[typed functions](../examples/functions.typed),
and [lists and strings](../examples/collections.typed).
