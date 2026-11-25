# Typed language specification

This file defines the whole language. A program is helpers followed by `main`. Values may nest. Precedence is `||`, then `&&`, then `==` and `<`, then `+` and `-`, then `*`, then prefix `!`, then calls and indexing. Parentheses group a value. `a + b * c` is `a + (b * c)`.

Punctuation may touch neighboring tokens (`x+1`, `main()`, `xs[0]`). Whitespace is spaces, tabs, or newlines. Every statement ends with `;`.

```text
program       ::= function* "main" "(" ")" ":" "Int64" "{" statement* "}"
function      ::= "func" identifier ["<" type_parameter ("," type_parameter)* ">"]
                  "(" [parameter ("," parameter)*] ")" ":" Type
                  "{" statement* "}"
type_parameter::= a letter, then letters, digits, or underscores
parameter     ::= identifier ":" Type
statement     ::= "let" identifier ":" Type "=" value ";"
                | "var" identifier ":" Type "=" value ";"
                | mutable_identifier "=" value ";"
                | "if" "(" value ")" "{" statement* "}" "else" "{" statement* "}"
                | "while" "(" value ")" "{" statement* "}"
                | "break" ";" | "continue" ";"
                | "return" value ";"
Type          ::= "Int64" | "Bool" | "ListInt64" | "String"
                | type_parameter
                | "(" [Type ("," Type)*] ")" "->" Type
atom          ::= integer | "true" | "false" | string | in_scope_identifier | function_name
lambda        ::= "fun" "(" [parameter ("," parameter)*] ")" ":" Type "=>" value
string        ::= '"' printable-ASCII* '"'
value         ::= atom | call | infix | index | "!" value | "(" value ")" | lambda
call          ::= callable ["<" Type ("," Type)* ">"] "(" [value ("," value)*] ")"
callable      ::= a built-in operation, a visible user function, or an in-scope function value
infix         ::= value ("+" | "-" | "*" | "==" | "<" | "&&" | "||") value
index         ::= value "[" value "]"
mutable_identifier ::= an in-scope var
identifier    ::= a lowercase ASCII letter, then ASCII letters, digits, or underscores
integer       ::= an optional "-" and one or more decimal digits, within signed 64-bit range
```

`"..."` is a literal token, `*` means zero or more, and `[...]` is optional.

## Scope

Helpers come before `main`. Each helper sees earlier helpers and itself, so self recursion is allowed. Forward calls, mutual recursion, nested functions, and overloading are rejected. `main` takes no parameters and returns `Int64`. Any other function returns one of the four data types, a type parameter declared on that function, or a function type written on the function.

Parameters and locals belong to that function. They cannot shadow a visible function or a built-in, and a name cannot be declared again while it is in scope. A name introduced in `if` or `while` ends at that block's `}`.

`let` and parameters are immutable. `var` is updated by `name = value;`, a statement with no result; the value must have the variable's type. A name reads the current value. `let copy: Int64 = count;` copies the value at that statement. Storing a `ListInt64` copies the list, so a later `listPush` does not change the earlier copy. There is no `:=`, `assign()`, or reference cell. Prefix `!` is boolean negation, not dereference.

Reserved names are `main`, `func`, `fun`, `let`, `var`, `if`, `else`, `while`, `break`, `continue`, `return`, `true`, `false`, the four type names, every operation below, and `assign`, `refInt`, `refBool`, `write`, `readInt`, `readBool`.

## Statements

A value expression is allowed in `let` / `var` initializers, assignment, `return`, and `if` / `while` conditions. A condition's value must have type `Bool`, so `if (n + 1 == 0)` is legal. Operator overloading is by left-operand type: each operator plus its left type picks one right type and one result type. The checker follows operators from the type it has to the type the hole requires, and it only walks types the program has already written.

A generic function declares its type parameters before the value parameters and is called with those arguments written out: `id<Int64>(n)`. A type argument is one of the four data types, or a type parameter already in scope. The body is checked against the type parameter itself, so a value of type `T` can be passed and returned, and it is not a legal operand of `+` or `==`. Instantiating the function does not create a type that was not written at the call.

A function type is written in an annotation, for example `(Int64) -> Int64`. A named function whose parameters and result match that type may be stored. A lambda may inhabit it when the lambda repeats that type: `fun(x: Int64): Int64 => x + 1`. The lambda body is a value and may nest. A function type that has not been written is not available to the checker.

`return value;` exits the enclosing function and may occur inside `if` or `while`. The value must match that function's return type. The checker does not require a return on every path, and it still typechecks statements after a return. `if` and `while` bodies are statement lists and have no type, so the two branches need not match.

`if` requires `else`. Its condition is a `Bool` value, so `if (count == 0)` and `if (done)` are legal. A `while` condition is also a `Bool` value. `while (true)` ends only by `break;`. A condition that should change must be a `Bool` `var` assigned in the body, as in `while (running)`.

`break;` leaves the nearest `while`. `continue;` skips the rest of this iteration and rechecks the condition. Both are legal only inside a loop, including in a nested `if`. The checker does not run the program or prove that a loop ends.

An `Int64` literal may have a leading `-` and leading zeroes. After a finished left atom, `-` is subtraction. A `String` literal is printable ASCII between `"`, may contain spaces, has no escapes, and cannot cross a line. `""` is empty. `ListInt64` has no literal. Indexes start at 0; a negative or past-the-end index is a runtime error. Indexing nests when the result has an index: `text[i]` yields a `String`, and `xs[i + 1]` is legal.

## Operators and operations

Binary. Either operand may itself be a value:

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
    let small: Bool = n < 10;
    if (small) {
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
        let done: Bool = n == 0;
        if (done) { break; } else { running = false; }
    }
    let text: String = "a b";
    let letter: String = text[0];
    let matched: Bool = letter == "a";
    if (matched) { return n; } else { return 0; }
}
```
