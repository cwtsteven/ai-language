"""Prompt text for the typed pipeline and the direct Cangjie arm.

The typed strings are the same sentences embedded in src/language_prompt.h and
src/planning.h. Tests check that those headers still contain them. The Cangjie
arm uses the same request wrappers and appends docs/cangjie-subset.md where the
typed arm appends docs/language.md.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPED_SPEC = (ROOT / "docs/language.md").read_text()
CANGJIE_SPEC = (ROOT / "docs/cangjie-subset.md").read_text()

TYPED_CODE_PREFIX = (
    "Write exactly one program satisfying this language specification. "
    "Output code only: no Markdown or explanation. Use spaces between tokens for clarity.\n\n")
TYPED_PLAN_PREFIX = (
    "Write a concise implementation plan for the requested program. Output a design summary with typed ANF statement sketches, not a complete program or a reasoning transcript. "
    "State the expected result or behavior. List any helper functions in declaration order with typed parameters and return types. "
    "For each function, including main, specify its body: initial values, computations in order, cell updates, loop exit conditions, and returned atom. "
    "For loops, state the invariant and when the condition is checked. For recursion, state the base case and how the argument decreases. "
    "The language grammar, type rules, and scope rules apply to every pseudocode fragment and statement sketch in the plan itself. "
    "Use actual language syntax with explicit typed let bindings for intermediate computations. "
    "Use named calls such as add, sub, mul, eq, lt, not, and, or, emptyList, listLen, listGet, listPush, strLen, strEq, concat, and strAt; infix expressions are not allowed. "
    "Call arguments must be atoms; never nest calls or compound expressions. "
    "Bind a call result before using it in another call, a condition, or a return. "
    "Use var for a binding that later statements assign. Assignment is name = atom; and the name reads its current value. "
    "let bindings and parameters cannot be assigned. Do not use reference cells, !, assign(), or :=. "
    "Conditions must be Bool atoms and return values must be atoms of the function's declared return type; bind comparisons before if. "
    "For a changing loop condition, assign a Bool var in the body and use that var as the while condition, or use while (true) with an if/break exit. "
    "An immutable Bool bound before a loop does not update when its inputs change. "
    "Use prose for goals and invariants, but do not use syntax from other languages as pseudocode. "
    "Use as much space as needed for the plan; do not output a complete program or Markdown code fences. "
    "The following specification governs both the planned statement sketches and the eventual program:\n\n")
CANGJIE_CODE_PREFIX = (
    "Write exactly one program satisfying this language specification. "
    "Output code only: no Markdown or explanation.\n\n")
CANGJIE_PLAN_PREFIX = (
    "Write a concise implementation plan for the requested program. Output a design summary with typed statement sketches, not a complete program or a reasoning transcript. "
    "State the expected result or behavior. List any helper functions in declaration order with typed parameters and return types. "
    "For each function, including main, specify its body: initial values, computations in order, updates, loop exit conditions, and the returned value. "
    "For loops, state the invariant and when the condition is checked. For recursion, state the base case and how the argument decreases. "
    "The language grammar and type rules apply to every pseudocode fragment and statement sketch in the plan itself. "
    "Use actual Cangjie syntax from the specification: func definitions, let and var bindings, parenthesized if and while conditions, and the integer and Boolean operators listed there. "
    "Use prose for goals and invariants, but do not use syntax from other languages as pseudocode. "
    "Use as much space as needed for the plan; do not output a complete program or Markdown code fences. "
    "The following specification governs both the planned statement sketches and the eventual program:\n\n")

PLAN_USER_PREFIX = "Plan the implementation for this request: "
CODE_USER_PREFIX = "Produce a program for this request: "
PLAN_GUIDANCE = (
    "\n\nImplementation plan:\n{plan}"
    "\n\nUse this plan as guidance. Correct any conflict with the original request or language rules. Output code only.")

SHARED_PROBLEM_TAIL = "The function will be tested with multiple inputs; do not compute just one example."
TYPED_INSTRUCTIONS = (
    "Output a complete program in the supplied typed language. You may declare helper functions "
    "before this function. After it, include main(): Int64 { return 0; }. "
    "Use only the specified built-ins, atomic arguments, and typed intermediate bindings.")
CANGJIE_INSTRUCTIONS = (
    "Output a complete program in the supplied Cangjie language. You may declare helper functions "
    "before this function. After it, include main(): Int64 { return 0 }. "
    "Use the syntax and operators in the supplied specification.")


def problem_statement(task):
    parameters = ", ".join(p["name"] + ": " + p["type"] for p in task["parameters"])
    return (f"Implement func {task['entry_point']}({parameters}): {task['return_type']}.\n"
            f"{task['description']}\n"
            f"{SHARED_PROBLEM_TAIL}")


def task_text(task, mode):
    instructions = TYPED_INSTRUCTIONS if mode == "constrained" else CANGJIE_INSTRUCTIONS
    return problem_statement(task) + "\n" + instructions


def system_text(mode, planning):
    if mode == "constrained":
        return (TYPED_PLAN_PREFIX if planning else TYPED_CODE_PREFIX) + TYPED_SPEC
    if mode == "cangjie":
        return (CANGJIE_PLAN_PREFIX if planning else CANGJIE_CODE_PREFIX) + CANGJIE_SPEC
    raise ValueError(f"unknown mode: {mode}")


def planning_user(task, mode):
    return PLAN_USER_PREFIX + task_text(task, mode)


def code_user(task, mode, plan=None):
    text = CODE_USER_PREFIX + task_text(task, mode)
    if plan is not None:
        text += PLAN_GUIDANCE.format(plan=plan)
    return text
