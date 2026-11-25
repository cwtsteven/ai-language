# Typed language specification

This file defines the whole language. A program is structs and helpers followed by `main`. Values may nest. Precedence is `||`, then `&&`, then `==` `!=` `<` `<=` `>` `>=`, then `+` and `-`, then `*` `/` `%`, then prefix `!`, then calls, indexing, and member access. Parentheses group a value. `a + b * c` is `a + (b * c)`.

Punctuation may touch neighboring tokens (`x+1`, `main()`, `xs[0]`, `p.x`). Whitespace is spaces, tabs, or newlines. Every statement ends with `;`.

```text
program       ::= item* "main" "(" ")" ":" "Int64" "{" statement* "}"
item          ::= struct | function
struct        ::= "struct" StructName "{" field* method* "}"
field         ::= identifier ":" Type ";"
method        ::= "func" identifier "(" [parameter ("," parameter)*] ")" ":" Type
                  "{" statement* "}"
function      ::= "func" identifier ["<" type_parameter ("," type_parameter)* ">"]
                  "(" [parameter ("," parameter)*] ")" ":" Type
                  "{" statement* "}"
type_parameter::= a letter, then letters, digits, or underscores
parameter     ::= identifier ":" Type
statement     ::= "let" identifier ":" Type "=" value ";"
                | "var" identifier ":" Type "=" value ";"
                | mutable_identifier "=" value ";"
                | "if" "(" value ")" "{" statement* "}" ["else" "{" statement* "}"]
                | "while" "(" value ")" "{" statement* "}"
                | "for" "(" identifier ":" Type "in" iterable ")" "{" statement* "}"
                | "break" ";" | "continue" ";"
                | "return" value ";"
iterable      ::= value | value ".." value
Type          ::= "Int64" | "Bool" | "String"
                | "List" "<" Type ">"
                | StructName
                | type_parameter
                | "(" [Type ("," Type)*] ")" "->" Type
atom          ::= integer | "true" | "false" | string | in_scope_identifier | function_name
lambda        ::= "fun" "(" [parameter ("," parameter)*] ")" ":" Type "=>" value
string        ::= '"' printable-ASCII* '"'
value         ::= atom | call | infix | index | member | "!" value | "(" value ")" | lambda
                | list_ctor | struct_ctor
list_ctor     ::= "List" "<" Type ">" "(" ")"
struct_ctor   ::= StructName "(" [value ("," value)*] ")"
call          ::= callable ["<" Type ("," Type)* ">"] "(" [value ("," value)*] ")"
callable      ::= a built-in operation, a visible user function, or an in-scope function value
infix         ::= value ("+" | "-" | "*" | "/" | "%" | "==" | "!=" | "<" | "<=" | ">" | ">="
                | "&&" | "||") value
index         ::= value "[" value "]"
member        ::= value "." identifier ["(" [value ("," value)*] ")"]
mutable_identifier ::= an in-scope var
identifier    ::= a lowercase ASCII letter, then ASCII letters, digits, or underscores
StructName    ::= an uppercase ASCII letter, then ASCII letters, digits, or underscores
integer       ::= an optional "-" and one or more decimal digits, within signed 64-bit range
```

`"..."` is a literal token, `*` means zero or more, and `[...]` is optional.

## Scope

Structs and helpers come before `main`. Each helper sees earlier helpers and itself, so self recursion is allowed. Forward calls, mutual recursion, nested functions, and overloading are rejected. `main` takes no parameters and returns `Int64`. Any other function returns a written type: a base type, `List<T>`, a declared struct, a type parameter on that function, or a function type.

A struct name is registered when its `{` is accepted, so a field may refer to the struct being declared. A field type must be a type already declared or that current struct. A field must not embed the same struct (or another struct that embeds it) by value: `struct Node { next: Node; }` is rejected, matching Cangjie value structs. `List<Node>` is allowed as a field because a list is not an inline struct. Methods follow all fields. Inside a method, field names are immutable locals and `this` is the receiver. Fields cannot be assigned. A method may call itself through `this` because the method is registered before its body. Forward and mutual method calls are rejected. A field and a method cannot share a name. Methods are not generic and are not overloaded.

Parameters and locals belong to that function or method. They cannot shadow a visible function, struct constructor, or built-in, and a name cannot be declared again while it is in scope. A name introduced in `if`, `while`, or `for` ends at that block's `}`.

`let` and parameters are immutable. `var` is updated by `name = value;`, a statement with no result; the value must have the variable's type. A name reads the current value. `let copy: Int64 = count;` copies the value at that statement. Storing a `List<T>` copies the list, so a later `.add` does not change the earlier copy. Storing a struct copies the struct, including any list fields. There is no `:=`, `assign()`, or reference cell. Prefix `!` is boolean negation, not dereference.

A `let` / `var` / assignment annotation must be inhabitable. `List<T>` is always inhabitable via `List<T>()`. A struct is inhabitable when a value of that type is in scope, or when every field type is inhabitable. A function may still return an uninhabitable type because a body may close without `return`.

Reserved names are `main`, `func`, `fun`, `struct`, `this`, `let`, `var`, `if`, `else`, `while`, `for`, `in`, `break`, `continue`, `return`, `true`, `false`, `List`, `Int64`, `Bool`, `String`, every operation below, and `assign`, `refInt`, `refBool`, `write`, `readInt`, `readBool`.

## Statements

A value expression is allowed in `let` / `var` initializers, assignment, `return`, `if` / `while` conditions, and `for` iterables. A condition's value must have type `Bool`, so `if (n + 1 == 0)` is legal. Operator overloading is by left-operand type: each operator plus its left type picks one right type and one result type. The checker follows operators from the type it has to the type the hole requires, and it only walks types the program has already written.

A generic function declares its type parameters before the value parameters and is called with those arguments written out: `id<Int64>(n)`. A type argument is a written type. The body is checked against the type parameter itself, so a value of type `T` can be passed and returned, and it is not a legal operand of `+` or `==`. Instantiating the function does not create a type that was not written at the call.

A function type is written in an annotation, for example `(Int64) -> Int64`. A named function whose parameters and result match that type may be stored. A lambda may inhabit it when the lambda repeats that type: `fun(x: Int64): Int64 => x + 1`. The lambda body is a value and may nest. A function type that has not been written is not available to the checker.

`return value;` exits the enclosing function or method and may occur inside `if`, `while`, or `for`. The value must match that function's return type. The checker does not require a return on every path, and it still typechecks statements after a return. `if`, `while`, and `for` bodies are statement lists and have no type, so the two branches of an `if` need not match.

`if` takes a `Bool` condition, so `if (count == 0)` and `if (done)` are legal. An `else` branch is optional. A `while` condition is also a `Bool` value. `while (true)` ends only by `break;`. A condition that should change must be a `Bool` `var` assigned in the body, as in `while (running)`.

`for (item: T in xs)` walks a `List<T>`. `for (i: Int64 in start .. end)` walks a half-open `Int64` range. The binding is immutable and typed in the header. `..` is only legal in that header; it is not a value and not a list. Typing details are in [typing.md](typing.md).

`break;` leaves the nearest `while` or `for`. `continue;` skips the rest of this iteration: a `while` rechecks its condition, and a `for` advances to the next element or integer. Both are legal only inside a loop, including in a nested `if`. The checker does not run the program or prove that a loop ends.

An `Int64` literal may have a leading `-` and leading zeroes. After a finished left atom, `-` is subtraction. A `String` literal is printable ASCII between `"`, may contain spaces, has no escapes, and cannot cross a line. `""` is empty. `List<T>` has no element literal; an empty list is `List<T>()`. Indexes start at 0; a negative or past-the-end index is a runtime error. Indexing nests when the result has an index: `text[i]` yields a `String`, and `xs[i + 1]` is legal.

After a finished receiver, `.` is postfix at the same precedence as calls and indexing. `p.x` reads a field. `p.dx(1)` calls the method selected by the receiver's static type and that name. On a `List<T>` receiver, `.size` has type `Int64` and takes no parentheses. `.add(value)` returns a new `List<T>` and requires an inhabitant of `T`; it does not mutate the receiver.

## Operators and operations

Binary. Either operand may itself be a value:

- `Int64 + Int64 -> Int64`, `String + String -> String`
- `Int64 - Int64 -> Int64`
- `Int64 * Int64 -> Int64`
- `Int64 / Int64 -> Int64` (truncates toward zero; division by zero is a runtime error)
- `Int64 % Int64 -> Int64` (`a - b * (a / b)`; remainder by zero is a runtime error)
- `Int64 == Int64 -> Bool`, `String == String -> Bool`
- `Int64 != Int64 -> Bool`, `String != String -> Bool`
- `Int64 < Int64 -> Bool`, `Int64 <= Int64 -> Bool`
- `Int64 > Int64 -> Bool`, `Int64 >= Int64 -> Bool`
- `Bool && Bool -> Bool`
- `Bool || Bool -> Bool`

Prefix:

- `! Bool -> Bool`

Indexing:

- `List<T>[Int64] -> T`
- `String[Int64] -> String` (one-character string)

List and string members:

- `List<T>().size` has type `Int64`
- `List<T>().add(T) -> List<T>`
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
struct Point {
    x: Int64;
    y: Int64;
    func dx(d: Int64): Int64 {
        let n: Int64 = x + d;
        return n;
    }
}
main(): Int64 {
    let empty: List<Int64> = List<Int64>();
    let items: List<Int64> = empty.add(4);
    let n: Int64 = items[0];
    let p: Point = Point(1, 2);
    let shifted: Int64 = p.dx(3);
    var running: Bool = true;
    while (running) {
        let done: Bool = n == 0;
        if (done) { break; }
        running = false;
    }
    var total: Int64 = 0;
    for (item: Int64 in items) {
        total = total + item;
    }
    for (i: Int64 in 0 .. n) {
        total = total + i;
    }
    let q: Int64 = total / 2;
    let r: Int64 = total % 2;
    let bigger: Bool = q >= r;
    let text: String = "a b";
    let letter: String = text[0];
    let matched: Bool = letter != "b";
    if (matched && bigger) { return n + shifted; }
    return 0;
}
```
