"""Prompt text for the typed pipeline and the direct Cangjie arm.

Both arms share the same request wrappers and output instructions. The only
difference in the task text is signature type spelling (`List<Int64>` vs
`ArrayList<Int64>`). The typed strings are also embedded in src/language_prompt.h
and src/planning.h; tests check that those headers still contain them. Each arm
appends its own language specification: docs/language.md for typed,
docs/cangjie-subset.md for Cangjie.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPED_SPEC = (ROOT / "docs/language.md").read_text()
CANGJIE_SPEC = (ROOT / "docs/cangjie-subset.md").read_text()

CODE_PREFIX = (
    "Write exactly one program satisfying this language specification. "
    "Output code only: no Markdown or explanation.\n\n")
PLAN_PREFIX = (
    "Write a concise implementation plan for the requested program. Output a design summary in prose, with no program syntax and no reasoning transcript. "
    "State the expected result or behavior. List any helper functions in declaration order with typed parameters and return types. "
    "For each function, including main, describe its body in prose: initial values, computations in order, assignments, loop exit conditions, and the returned value. "
    "For loops, state the invariant and when the condition is checked. For recursion, state the base case and how the argument decreases. "
    "Name the operations and the types. Write signatures only; leave bindings, calls, braces, and function bodies for the later program. "
    "Use as much space as needed for the plan. Do not output a complete program or Markdown code fences. "
    "The following specification constrains the program written later. Do not transcribe it into the plan:\n\n")

# Kept as aliases so older imports and header sync checks keep working.
TYPED_CODE_PREFIX = CODE_PREFIX
TYPED_PLAN_PREFIX = PLAN_PREFIX
CANGJIE_CODE_PREFIX = CODE_PREFIX
CANGJIE_PLAN_PREFIX = PLAN_PREFIX

PLAN_USER_PREFIX = "Plan the implementation for this request: "
CODE_USER_PREFIX = "Produce a program for this request: "
PLAN_GUIDANCE = (
    "\n\nImplementation plan:\n{plan}"
    "\n\nUse this plan as guidance. Correct any conflict with the original request or language rules. Output code only.")

SHARED_PROBLEM_TAIL = "The function will be tested with multiple inputs; do not compute just one example."
SHARED_INSTRUCTIONS = (
    "Output a complete program in the supplied language. You may declare helper functions "
    "before this function. After it, include main(): Int64 { return 0; }. "
    "Use only the syntax, built-ins, and rules in the supplied specification.")


def cangjie_type(type_name):
    """The task file stores typed-language names. Cangjie spells a list of integers as ArrayList<Int64>."""
    if type_name == "List<Int64>":
        return "ArrayList<Int64>"
    return type_name


def problem_statement(task, mode="constrained"):
    render = cangjie_type if mode == "cangjie" else lambda type_name: type_name
    parameters = ", ".join(p["name"] + ": " + render(p["type"]) for p in task["parameters"])
    return (f"Implement func {task['entry_point']}({parameters}): {render(task['return_type'])}.\n"
            f"{task['description']}\n"
            f"{SHARED_PROBLEM_TAIL}")


def task_text(task, mode):
    return problem_statement(task, mode) + "\n" + SHARED_INSTRUCTIONS


def system_text(mode, planning):
    prefix = PLAN_PREFIX if planning else CODE_PREFIX
    if mode == "constrained":
        return prefix + TYPED_SPEC
    if mode == "cangjie":
        return prefix + CANGJIE_SPEC
    raise ValueError(f"unknown mode: {mode}")


def planning_user(task, mode):
    return PLAN_USER_PREFIX + task_text(task, mode)


def code_user(task, mode, plan=None):
    text = CODE_USER_PREFIX + task_text(task, mode)
    if plan is not None:
        text += PLAN_GUIDANCE.format(plan=plan)
    return text
