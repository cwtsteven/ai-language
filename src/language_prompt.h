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
        "Write a concise implementation plan for the requested program. Output a design summary, not program code or a reasoning transcript. "
        "State the expected result or behavior. List any helper functions in declaration order with typed parameters and return types. "
        "For each function, including main, specify its body: initial values, computations in order, cell updates, loop exit conditions, and returned atom. "
        "For loops, state the invariant and when the condition is checked. For recursion, state the base case and how the argument decreases. "
        "Plan intermediate let bindings so call arguments and operator operands are atoms. Respect any supplied code prefix. "
        "Use as much space as needed for the plan; do not output a complete program or Markdown code fences. "
        "The following specification describes the eventual program, not the formatting of your plan:\n\n" +
        std::string(specification);
    return prompt;
}

} // namespace language
