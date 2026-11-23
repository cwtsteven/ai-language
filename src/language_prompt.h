#pragma once

#include <string>
#include <string_view>

namespace language {

inline constexpr std::string_view grammar = R"GRAMMAR(

Formal grammar (EBNF; quoted text is literal, * means zero or more):
program       ::= "main" "(" ")" ":" "Int64" "{" statement* "return" int_atom ";" "}"
statement     ::= "let" identifier ":" Type "=" rhs ";"
                | ref_identifier ":=" rhs ";"
                | "if" "(" bool_atom ")" "{" statement* "}" "else" "{" statement* "}"
                | "while" "(" bool_atom ")" "{" statement* "}"
                | "break" ";" | "continue" ";"
Type          ::= "Int64" | "Bool" | "RefInt64" | "RefBool"
atom          ::= integer | "true" | "false" | in_scope_identifier | "!" ref_identifier
rhs           ::= atom | atom binary_op atom | unary_op "(" atom ")"
                | binary_call "(" atom "," atom ")"
binary_op     ::= "+" | "-" | "*" | "<" | "==" | "&&" | "||"
unary_op      ::= "not" | "refInt" | "refBool"
binary_call   ::= "add" | "sub" | "mul" | "eq" | "lt" | "and" | "or"
int_atom      ::= an atom of type Int64
bool_atom     ::= an atom of type Bool
ref_identifier ::= an in-scope identifier of type RefInt64 or RefBool
identifier    ::= a lowercase ASCII letter followed by ASCII letters, digits, or underscores
integer       ::= an optional '-' followed by decimal digits, within signed 64-bit range
All names must be fresh in active scopes and must not be reserved words or operation names.
All operands, arguments, right-hand sides, assignments, and return values must match the type signatures above.
Dereferencing !cell has the cell's contents type. Only reference cells can be assigned with :=.
Conditions must be Boolean atoms. Bind comparisons before using them as conditions.
break and continue are allowed only inside a while body, including nested if branches.
break exits the nearest enclosing while. continue skips the rest of its body and rechecks its condition.
)GRAMMAR";

inline const std::string & system_prompt() {
    static const std::string prompt =
        "Write exactly one program in the following small Cangjie-inspired typed language. Output code only: no Markdown or explanation. "
        "Put whitespace between every token, including punctuation. "
        "Start with: main ( ) : Int64 { . End with: return INT_ATOM ; } . "
        "The body may use let name : Type = rhs ;, cell := rhs ;, "
        "if ( bool_atom ) { statements } else { statements }, or "
        "while ( bool_atom ) { statements }, with break ; or continue ; inside loops. "
        "Types are Int64, Bool, RefInt64, RefBool. Atoms are integer literals, true, false, in-scope variables, or !cell to dereference a cell. Whitespace after ! is allowed. Keep := together. "
        "Right-hand sides may be atoms, one binary expression of atomic operands, or these calls with atomic arguments. "
        "Binary operators: Int64 + Int64, Int64 - Int64, Int64 * Int64 produce Int64; "
        "Int64 < Int64 and Int64 == Int64 produce Bool; Bool && Bool, Bool || Bool, and Bool == Bool produce Bool. "
        "add(Int64,Int64):Int64, sub(Int64,Int64):Int64, mul(Int64,Int64):Int64, "
        "eq(Int64,Int64):Bool, lt(Int64,Int64):Bool, not(Bool):Bool, and(Bool,Bool):Bool, or(Bool,Bool):Bool, "
        "refInt(Int64):RefInt64, refBool(Bool):RefBool. "
        "Every computed intermediate value must be bound by let or assigned to a cell. Do not nest operations. "
        "All let bindings are immutable. For mutation, allocate a RefInt64 or RefBool cell. "
        "Read it with !cell and update its contents with cell := value. Never use var, write(), readInt(), or readBool(). "
        "A while (!flag) loop rereads a RefBool cell on each iteration. "
        "A while (true) loop can stop with break ;. A continue ; skips the rest of the current iteration and rechecks the condition. "
        "Both statements target the nearest enclosing while and may appear inside its nested if branches. "
        "Declaring or updating a local stopFlag does not stop while (true); use break ; instead. "
        "For a counter loop, compare the current counter with the limit inside the loop, "
        "then exit the loop if done or update the counter. Return only after the loop, not inside it. "
        "Arithmetic example: main ( ) : Int64 { let x : Int64 = 2 + 3 ; return x ; } "
        "Mutable cell example: main ( ) : Int64 { let x : RefInt64 = refInt ( 5 ) ; "
        "x := ! x + 1 ; return ! x ; }" + std::string(grammar);
    return prompt;
}

} // namespace language
