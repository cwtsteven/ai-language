# Language specification

This is the single language specification. The build embeds this file verbatim
in the generation and planning prompts; the C++ prefix checker implements it.

The syntax follows Cangjie's `main(): Int64 { ... }`, typed `let`
declarations and braced control flow, with OCaml-style reference operators. This is a small
Cangjie-inspired language, not a complete Cangjie parser or compiler.
Punctuation may touch surrounding tokens: `add(x, 1)` and `main()` are
accepted, as are spaced versions. Whitespace may contain spaces, tabs, or
newlines. Statements currently require semicolons so the prefix checker can
recognize their ends without relying on line breaks.

```text
program       ::= function* "main" "(" ")" ":" "Int64" "{" statement* "return" int_atom ";" "}"
function      ::= "func" identifier "(" [parameter ("," parameter)*] ")" ":" Type
                  "{" statement* "return" typed_atom ";" "}"
parameter     ::= identifier ":" Type
statement     ::= "let" identifier ":" Type "=" rhs ";"
                | ref_identifier ":=" rhs ";"
                | "if" "(" bool_atom ")" "{" statement* "}" "else" "{" statement* "}"
                | "while" "(" bool_atom ")" "{" statement* "}"
                | "break" ";" | "continue" ";"
Type          ::= "Int64" | "Bool" | "RefInt64" | "RefBool"
atom          ::= integer | "true" | "false" | in_scope_identifier | "!" ref_identifier
rhs           ::= atom | atom binary_op atom | call
call          ::= callable "(" [atom ("," atom)*] ")"
binary_op     ::= "+" | "-" | "*" | "<" | "==" | "&&" | "||"
callable      ::= a built-in operation name or a visible user function name
int_atom      ::= an atom of type Int64
bool_atom     ::= an atom of type Bool
typed_atom    ::= an atom of the enclosing function's declared return type
ref_identifier ::= an in-scope identifier of type RefInt64 or RefBool
identifier    ::= lowercase ASCII letter followed by ASCII letters, digits, or underscores
integer       ::= optional "-" followed by one or more decimal digits, within signed 64-bit range
```

Quoted strings are literal tokens, `*` means zero or more, and square brackets
mark an optional part. This grammar describes structure; the type and scope
rules below determine which atoms, calls, and operators are valid.

Functions are declared before `main`. Parameters and return values may use any
of the four types. Each helper sees earlier helpers and itself, so self recursion
is allowed; forward calls, mutual recursion, nested functions, and overloading
are not supported. `main` takes no parameters and returns `Int64`.

Calls require exactly the declared number and types of atomic arguments. Bind
call results before using them in another call, operator, condition, or return.
Every body must end with `return atom;` at its outer level; nested returns are
not supported. Parameters and locals are scoped to that body, and cannot shadow
visible function names or built-in operations. See [the recursive factorial
example](../examples/functions.typed).

Binary operators are `+`, `-`, `*` on `Int64`, `<` and `==` on `Int64`
producing `Bool`, and `&&`, `||`, and `==` on `Bool`. Each operand must be atomic; bind a
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

`break;` exits the nearest enclosing `while`. `continue;` skips the remaining
statements in that loop's current iteration and rechecks its condition. Both
statements require semicolons and are allowed only inside a loop, including
inside its nested `if` branches. They cannot be used as variable names.
For `while (true)`, use `break;` to leave the loop; updating a local flag does
not change a literal `true` condition. The checker validates these rules but
does not execute the control flow or prove termination.

`Int64` literals are signed 64-bit decimal integers, optionally prefixed
with `-`; leading zeroes are allowed. Names start with a lowercase ASCII letter,
then contain ASCII letters, digits, or underscores. Keywords, built-in operation
names, `var`, `write`, `readInt`, and `readBool` are reserved. Variables cannot be redefined in an active
scope; parameters and locals do not leak between functions, and names introduced
inside `if` or `while` blocks disappear at the closing
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

Examples: [Fibonacci with break](../examples/fibonacci-5-break.typed),
[Fibonacci with a flag](../examples/fibonacci-5.typed),
[typed functions](../examples/functions.typed).

For example, this complete program returns `5`:

```text
func increment(value: Int64): Int64 {
    let next: Int64 = value + 1;
    return next;
}
main(): Int64 {
    let result: Int64 = increment(4);
    return result;
}
```
