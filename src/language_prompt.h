#pragma once

#include "language_spec.h"

#include <string>

namespace language {

inline const std::string & system_prompt() {
    static const std::string prompt =
        "Write exactly one program satisfying this language specification. "
        "Output code only: no Markdown or explanation. Use spaces between tokens for clarity.\n\n" +
        std::string(specification);
    return prompt;
}

inline const std::string & planning_prompt() {
    static const std::string prompt =
        "Write a concise implementation plan for the requested program. Output a design summary with typed ANF statement sketches, not a complete program or a reasoning transcript. "
        "State the expected result or behavior. List any helper functions in declaration order with typed parameters and return types. "
        "For each function, including main, specify its body: initial values, computations in order, cell updates, loop exit conditions, and returned atom. "
        "For loops, state the invariant and when the condition is checked. For recursion, state the base case and how the argument decreases. "
        "The language grammar, type rules, and scope rules apply to every pseudocode fragment and statement sketch in the plan itself. "
        "Use actual language syntax with explicit typed let bindings for intermediate computations. "
        "Use named calls such as add, sub, mul, eq, lt, not, and, and or; infix expressions are not allowed. "
        "Call arguments must be atoms; never nest calls or compound expressions. "
        "Bind a call result before using it in another call, a condition, or a return. "
        "Write reference cells with assign(cell, value); using atomic arguments. Bind computed values before assign; do not use :=. "
        "Conditions must be Bool atoms and return values must be atoms of the function's declared return type; bind comparisons before if. "
        "For a changing loop condition, use a RefBool atom updated in the body, or while (true) with a comparison bound inside the loop and an if/break exit. "
        "An immutable Bool bound before a loop does not update when its inputs change. "
        "Use prose for goals and invariants, but do not use syntax from other languages as pseudocode. "
        "Use as much space as needed for the plan; do not output a complete program or Markdown code fences. "
        "The following specification governs both the planned statement sketches and the eventual program:\n\n" +
        std::string(specification);
    return prompt;
}

} // namespace language
