# Typed language specification

This file defines the whole language. A program is helpers followed by `main`. Call arguments, conditions, and returned values are atoms (a literal or a variable), never nested calls or infix expressions.

Punctuation may touch neighboring tokens (`add(x, 1)`, `main()`). Whitespace is spaces, tabs, or newlines. Every statement ends with `;`.

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
callable      ::= a built-in operation or a visible user function
int_atom      ::= an Int64 atom
bool_atom     ::= a Bool atom
typed_atom    ::= an atom of the enclosing function's return type
mutable_identifier ::= an in-scope var of the assigned atom's type
identifier    ::= a lowercase ASCII letter, then ASCII letters, digits, or underscores
integer       ::= an optional "-" and one or more decimal digits, within signed 64-bit range
```

`"..."` is a literal token, `*` means zero or more, and `[...]` is optional.

## Scope

Helpers come before `main`. Each helper sees earlier helpers and itself, so self recursion is allowed. Forward calls, mutual recursion, nested functions, and overloading are rejected. `main` takes no parameters and returns `Int64`. Any other function returns `Int64`, `Bool`, `ListInt64`, or `String`.

Parameters and locals belong to that function. They cannot shadow a visible function or a built-in, and a name cannot be declared again while it is in scope. A name introduced in `if` or `while` ends at that block's `}`.

`let` and parameters are immutable. `var` is updated by `name = atom;`, a statement with no result; the atom must have the variable's type. A name reads the current value. `let copy: Int64 = count;` copies the value at that statement. Storing a `ListInt64` copies the list, so a later `listPush` does not change the earlier copy. There is no `:=`, `assign()`, reference cell, or `!`.

Reserved names are `main`, `func`, `let`, `var`, `if`, `else`, `while`, `break`, `continue`, `return`, `true`, `false`, the four type names, every operation below, and `assign`, `refInt`, `refBool`, `write`, `readInt`, `readBool`.

## Statements

A call takes exactly its declared arguments, each an atom of the declared type. Bind the result with `let` or `var` before using it in another call, a condition, or a return. Infix expressions are rejected.

`return atom;` exits the enclosing function and may occur inside `if` or `while`. The atom must match that function's return type. The checker does not require a return on every path, and it still typechecks statements after a return. `if` and `while` bodies are statement lists and have no type, so the two branches need not match.

`if` requires `else`. Its condition is a `Bool` atom, so bind the comparison first: `let done: Bool = eq(counter, limit);` then `if (done)`. A `while` condition is also a `Bool` atom. `while (true)` ends only by `break;`. A condition that should change must be a `Bool` `var` assigned in the body, as in `while (running)`.

`break;` leaves the nearest `while`. `continue;` skips the rest of this iteration and rechecks the condition. Both are legal only inside a loop, including in a nested `if`. The checker does not run the program or prove that a loop ends.

An `Int64` literal may have a leading `-` and leading zeroes. A `String` literal is printable ASCII between `"`, may contain spaces, has no escapes, and cannot cross a line. `""` is empty. `ListInt64` has no literal. Indexes start at 0; a negative or past-the-end index is a runtime error.

## Operations

- `add(Int64, Int64): Int64`
- `sub(Int64, Int64): Int64`
- `mul(Int64, Int64): Int64`
- `listLen(ListInt64): Int64`
- `listGet(ListInt64, Int64): Int64` reads the element at the index
- `strLen(String): Int64`
- `eq(Int64, Int64): Bool`
- `lt(Int64, Int64): Bool`
- `not(Bool): Bool`
- `and(Bool, Bool): Bool`
- `or(Bool, Bool): Bool`
- `strEq(String, String): Bool`
- `emptyList(): ListInt64`
- `listPush(ListInt64, Int64): ListInt64` appends the value and returns the new list
- `concat(String, String): String`
- `strAt(String, Int64): String` returns the one-character string at the index

```text
func groups(n: Int64): Int64 {
    let small: Bool = lt(n, 10);
    if (small) {
        return 0;
    } else {
        let rest: Int64 = sub(n, 10);
        let counted: Int64 = groups(rest);
        let result: Int64 = add(counted, 1);
        return result;
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
    let n: Int64 = listGet(items, 0);
    var running: Bool = true;
    while (running) {
        let done: Bool = eq(n, 0);
        if (done) { break; } else { running = false; }
    }
    let text: String = "a b";
    let letter: String = strAt(text, 0);
    let same: Bool = strEq(letter, "a");
    if (same) { return n; } else { return 0; }
}
```
