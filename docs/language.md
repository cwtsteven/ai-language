# Typed language specification

This file defines the whole language. A program is helpers followed by `main`. Values are atoms, calls of the remaining list/string helpers, a single non-nested infix or index expression, or prefix `!`. Arguments, indexes, and operator operands are atoms only—never nested calls or nested operators (`a + b * c` and `xs[i + 1]` are rejected).

Punctuation may touch neighboring tokens (`x+1`, `main()`, `xs[0]`). Whitespace is spaces, tabs, or newlines. Every statement ends with `;`.

```text
program       ::= function* "main" "(" ")" ":" "Int64" "{" statement* "}"
function      ::= "func" identifier "(" [parameter ("," parameter)*] ")" ":" Type
                  "{" statement* "}"
parameter     ::= identifier ":" Type
statement     ::= "let" identifier ":" Type "=" value ";"
                | "var" identifier ":" Type "=" value ";"
                | mutable_identifier "=" value ";"
                | "if" "(" value ")" "{" statement* "}" "else" "{" statement* "}"
                | "while" "(" value ")" "{" statement* "}"
                | "break" ";" | "continue" ";"
                | "return" value ";"
Type          ::= "Int64" | "Bool" | "ListInt64" | "String"
atom          ::= integer | "true" | "false" | string | in_scope_identifier
string        ::= '"' printable-ASCII* '"'
value         ::= atom | call | infix | index | "!" bool_atom
call          ::= callable "(" [atom ("," atom)*] ")"
callable      ::= a built-in operation or a visible user function
infix         ::= atom ("+" | "-" | "*" | "==" | "<" | "&&" | "||") atom
index         ::= atom "[" int_atom "]"
int_atom      ::= an Int64 atom
bool_atom     ::= a Bool atom
mutable_identifier ::= an in-scope var
identifier    ::= a lowercase ASCII letter, then ASCII letters, digits, or underscores
integer       ::= an optional "-" and one or more decimal digits, within signed 64-bit range
```

`"..."` is a literal token, `*` means zero or more, and `[...]` is optional.

## Scope

Helpers come before `main`. Each helper sees earlier helpers and itself, so self recursion is allowed. Forward calls, mutual recursion, nested functions, and overloading are rejected. `main` takes no parameters and returns `Int64`. Any other function returns `Int64`, `Bool`, `ListInt64`, or `String`.

Parameters and locals belong to that function. They cannot shadow a visible function or a built-in, and a name cannot be declared again while it is in scope. A name introduced in `if` or `while` ends at that block's `}`.

`let` and parameters are immutable. `var` is updated by `name = value;`, a statement with no result; the value must have the variable's type. A name reads the current value. `let copy: Int64 = count;` copies the value at that statement. Storing a `ListInt64` copies the list, so a later `listPush` does not change the earlier copy. There is no `:=`, `assign()`, or reference cell. Prefix `!` is boolean negation, not dereference.

Reserved names are `main`, `func`, `let`, `var`, `if`, `else`, `while`, `break`, `continue`, `return`, `true`, `false`, the four type names, every operation below, and `assign`, `refInt`, `refBool`, `write`, `readInt`, `readBool`.

## Statements

A value expression is allowed wherever a typed value is required: `let` / `var` initializers, assignment, `return`, and `if` / `while` conditions. Bind intermediate results with `let` or `var` when you need nesting. Operator overloading is by left-operand type: each operator plus its left type picks one right type and one result type.

`return value;` exits the enclosing function and may occur inside `if` or `while`. The value must match that function's return type. The checker does not require a return on every path, and it still typechecks statements after a return. `if` and `while` bodies are statement lists and have no type, so the two branches need not match.

`if` requires `else`. Its condition is a `Bool` value, so `if (count == 0)` is legal. A `while` condition is also a `Bool` value. `while (true)` ends only by `break;`. A condition that should change must be a `Bool` `var` assigned in the body, as in `while (running)`.

`break;` leaves the nearest `while`. `continue;` skips the rest of this iteration and rechecks the condition. Both are legal only inside a loop, including in a nested `if`. The checker does not run the program or prove that a loop ends.

An `Int64` literal may have a leading `-` and leading zeroes. After a finished left atom, `-` is subtraction. A `String` literal is printable ASCII between `"`, may contain spaces, has no escapes, and cannot cross a line. `""` is empty. `ListInt64` has no literal. Indexes start at 0; a negative or past-the-end index is a runtime error. Indexing does not nest: `xs[i]` is legal, `xs[i][j]` is not.

## Operators and operations

Binary, both operands atoms:

- `Int64 + Int64 -> Int64`, `String + String -> String`
- `Int64 - Int64 -> Int64`
- `Int64 * Int64 -> Int64`
- `Int64 == Int64 -> Bool`, `String == String -> Bool`
- `Int64 < Int64 -> Bool`
- `Bool && Bool -> Bool`
- `Bool || Bool -> Bool`

Prefix:

- `! Bool -> Bool`

Indexing (`name[index]`, index an `Int64` atom):

- `ListInt64[Int64] -> Int64`
- `String[Int64] -> String` (one-character string)

Calls that remain:

- `emptyList(): ListInt64`
- `listLen(ListInt64): Int64`
- `listPush(ListInt64, Int64): ListInt64` appends the value and returns the new list
- `strLen(String): Int64`

```text
func groups(n: Int64): Int64 {
    if (n < 10) {
        return 0;
    } else {
        let rest: Int64 = n - 10;
        let counted: Int64 = groups(rest);
        return counted + 1;
    }
}
main(): Int64 {
    let result: Int64 = groups(25);
    return result;
}
```

```text
main(): Int64 {
    let empty: ListInt64 = emptyList();
    let items: ListInt64 = listPush(empty, 4);
    let n: Int64 = items[0];
    var running: Bool = true;
    while (running) {
        if (n == 0) { break; } else { running = false; }
    }
    let text: String = "a b";
    let letter: String = text[0];
    if (letter == "a") { return n; } else { return 0; }
}
```
