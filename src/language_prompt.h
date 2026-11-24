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
        "Write a concise implementation plan for the requested program. Output a design summary in prose, with no program syntax and no reasoning transcript. "
        "State the expected result or behavior. List any helper functions in declaration order with typed parameters and return types. "
        "For each function, including main, describe its body in prose: initial values, computations in order, assignments, loop exit conditions, and the returned value. "
        "For loops, state the invariant and when the condition is checked. For recursion, state the base case and how the argument decreases. "
        "Name the operations and the types. Write signatures only; leave bindings, calls, braces, and function bodies for the later program. "
        "Use as much space as needed for the plan. Do not output a complete program or Markdown code fences. "
        "The following specification constrains the program written later. Do not transcribe it into the plan:\n\n" +
        std::string(specification);
    return prompt;
}

} // namespace language
