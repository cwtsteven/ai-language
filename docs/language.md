# Language reference

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
           | break ;
           | continue ;
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

`break;` exits the nearest enclosing `while`. `continue;` skips the remaining
statements in that loop's current iteration and rechecks its condition. Both
statements require semicolons and are allowed only inside a loop, including
inside its nested `if` branches. They cannot be used as variable names.
For `while (true)`, use `break;` to leave the loop; updating a local flag does
not change a literal `true` condition. The checker validates these rules but
does not execute the control flow or prove termination.

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

Examples: [Fibonacci with break](../examples/fibonacci-5-break.typed),
[Fibonacci with a flag](../examples/fibonacci-5.typed).
