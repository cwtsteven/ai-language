# Language specification

This is the single language specification. The build embeds this file verbatim
in the generation and planning prompts; the C++ prefix checker implements it.

The syntax follows Cangjie's `main(): Int64 { ... }`, typed `let`
declarations and braced control flow, with reference cells and explicit `assign` statements. This is a small
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
                | "assign" "(" ref_identifier "," atom ")" ";"
                | "if" "(" bool_atom ")" "{" statement* "}" "else" "{" statement* "}"
                | "while" "(" bool_atom ")" "{" statement* "}"
                | "break" ";" | "continue" ";"
                | "return" typed_atom ";"
Type          ::= "Int64" | "Bool" | "RefInt64" | "RefBool"
atom          ::= integer | "true" | "false" | in_scope_identifier | "!" ref_identifier
rhs           ::= atom | call
call          ::= callable "(" [atom ("," atom)*] ")"
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
with typed `let` declarations. `RefInt64`, `RefBool`, and the ref
operations are custom extensions, not Cangjie standard library types. `!cell`
is treated as an atom, so it also works in calls, conditions,
and returns. `! cell` is accepted too. `assign(cell, value);` writes an atom
matching the cell's stored type: `Int64` for `RefInt64`, or `Bool` for `RefBool`.
It is a statement with no result, not an expression. Bind a computed value first,
then pass that atom to `assign`; nested calls are not allowed in its arguments.

Conditions and return values currently accept atoms only. Bind comparisons
before `if`, and use `while (!flag)` with a `RefBool` cell for an updatable loop condition.
For example, bind `let done: Bool = eq(!counter, limit);` before `if (done)`.

`break;` exits the nearest enclosing `while`. `continue;` skips the remaining
statements in that loop's current iteration and rechecks its condition. Both
statements require semicolons and are allowed only inside a loop, including
inside its nested `if` branches. They cannot be used as variable names.
For `while (true)`, use `break;` to leave the loop; updating a local flag does
not change a literal `true` condition. The checker validates these rules but
does not execute the control flow or prove termination.

`Int64` literals are signed 64-bit decimal integers, optionally prefixed
with `-`; leading zeroes are allowed. Names start with a lowercase ASCII letter,
then contain ASCII letters, digits, or underscores. Keywords (including `assign`), built-in operation
names, `var`, `write`, `readInt`, and `readBool` are reserved. Variables cannot be redefined in an active
scope; parameters and locals do not leak between functions, and names introduced
inside `if` or `while` blocks disappear at the closing
brace. All bindings are immutable. References can be aliased by another binding;
`assign(cell, value);` mutates the shared cell without changing its type. Dereferencing
`!cell` produces `Int64` for a `RefInt64`, or `Bool` for a `RefBool`. There is no
`var`, `:=`, plain assignment, `write()`, `readInt()`, or `readBool()` syntax. The intended runtime model has signed
64-bit integers and garbage-collected reference cells; this prototype checks code
but does not execute it.

| Result | Operations |
| --- | --- |
| `Int64` | `add(Int64, Int64)`, `sub(Int64, Int64)`, `mul(Int64, Int64)`, `!cell` for `cell: RefInt64` |
| `Bool` | `eq(Int64, Int64)`, `lt(Int64, Int64)`, `not(Bool)`, `and(Bool, Bool)`, `or(Bool, Bool)`, `!cell` for `cell: RefBool` |
| `RefInt64` | `refInt(Int64)` |
| `RefBool` | `refBool(Bool)` |

Examples: [Fibonacci with break](../examples/fibonacci-5-break.typed),
[Fibonacci with a flag](../examples/fibonacci-5.typed),
[typed functions](../examples/functions.typed).

For example, this complete program returns `5`:

```text
func increment(value: Int64): Int64 {
    let next: Int64 = add(value, 1);
    return next;
}
main(): Int64 {
    let result: Int64 = increment(4);
    return result;
}
```
