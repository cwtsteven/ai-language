#!/usr/bin/env python3
"""Compare the typed pipeline with direct Cangjie generation."""

import argparse
import contextlib
import dataclasses
import datetime
import hashlib
import fcntl
import json
import math
import os
from pathlib import Path
import platform
import re
import resource
import shutil
import signal
import subprocess
import sys
import time


sys.path.insert(0, str(Path(__file__).resolve().parent))
from prompts import code_user, planning_user, system_text, task_text

ROOT = Path(__file__).resolve().parents[1]
MODES = ("constrained", "cangjie")
NAME = re.compile(r"[a-z][a-zA-Z0-9_]*\Z")


@contextlib.contextmanager
def model_lock(path=None):
    """Prevent superseded benchmark runners from competing for model memory."""
    path = Path(path) if path is not None else ROOT / "benchmark-results/.model-run.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            lock.seek(0)
            raise ValueError("another model benchmark is already running: " + lock.read().strip())
        lock.seek(0)
        lock.truncate()
        lock.write(json.dumps({"pid": os.getpid(), "command": sys.argv}))
        lock.flush()
        try:
            yield lock
        finally:
            fcntl.flock(lock, fcntl.LOCK_UN)


@dataclasses.dataclass
class ProcessResult:
    command: list
    returncode: int
    timed_out: bool
    seconds: float
    stdout: str
    stderr: str


def execute(command, timeout, *, stdin=None, cwd=None, runtime_limits=False):
    """Kill the process group on timeout, including compiler/runtime children."""
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_CPU, (math.ceil(timeout) + 1, math.ceil(timeout) + 2))
        resource.setrlimit(resource.RLIMIT_FSIZE, (16 * 1024 * 1024, 16 * 1024 * 1024))

    command = [str(part) for part in command]
    started = time.monotonic()
    process = subprocess.Popen(command, stdin=subprocess.PIPE if stdin is not None else subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd,
                               start_new_session=True, preexec_fn=limits if runtime_limits else None)
    timed_out = False
    try:
        stdout, stderr = process.communicate(None if stdin is None else stdin.encode(), timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = process.communicate()
    except BaseException:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.communicate()
        raise
    return ProcessResult(command, process.returncode, timed_out, time.monotonic() - started,
                         stdout.decode("utf-8", errors="replace"), stderr.decode("utf-8", errors="replace"))


def save_process(directory, stage, result):
    (directory / f"{stage}.stdout").write_text(result.stdout)
    (directory / f"{stage}.stderr").write_text(result.stderr)
    metadata = dataclasses.asdict(result)
    del metadata["stdout"], metadata["stderr"]
    (directory / f"{stage}.json").write_text(json.dumps(metadata, indent=2) + "\n")


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def unittest_path(task, suite_dir=None):
    suite_dir = Path(suite_dir) if suite_dir is not None else ROOT / "benchmarks"
    return suite_dir / "cjtests" / f"{task['id']}_test.cj"


def count_unittest_cases(path):
    text = Path(path).read_text()
    count = len(re.findall(r"(?m)^@Test\b", text))
    if count <= 0:
        raise ValueError(f"unittest file has no @Test cases: {path}")
    return count


def load_suite(path):
    path = Path(path)
    suite = json.loads(path.read_text())
    if suite.get("schema_version") != 1 or not suite.get("tasks"):
        raise ValueError("suite requires schema_version 1 and nonempty tasks")
    ids = set()
    for task in suite["tasks"]:
        if not NAME.fullmatch(task["id"]) or task["id"] in ids:
            raise ValueError("task IDs must be unique safe identifiers")
        ids.add(task["id"])
        minimum = task.get("min_code_tokens", 1)
        if type(minimum) is not int or not 1 <= minimum < 2**31:
            raise ValueError("min_code_tokens must be a positive 32-bit integer")
        if not NAME.fullmatch(task["entry_point"]):
            raise ValueError("invalid task entry point")
        names = set()
        for parameter in task["parameters"]:
            if not NAME.fullmatch(parameter["name"]) or parameter["name"] in names:
                raise ValueError("invalid or duplicate parameter name")
            names.add(parameter["name"])
            if parameter["type"] not in ("Int64", "Bool"):
                raise ValueError("benchmark arguments currently support only Int64 and Bool")
        if task["return_type"] not in ("Int64", "Bool"):
            raise ValueError("tasks require scalar results")
        if "tests" in task:
            raise ValueError("inline tests arrays are obsolete; use benchmarks/cjtests/<id>_test.cj")
        tests = unittest_path(task, path.parent)
        if not tests.is_file():
            raise ValueError(f"missing unittest file: {tests}")
        task["unittest"] = str(tests)
        task["tests_total"] = count_unittest_cases(tests)
        if "typed_fn_" + task["entry_point"] not in tests.read_text():
            raise ValueError(f"unittest must call typed_fn_{task['entry_point']}: {tests}")
    return suite


def task_prompt(task):
    """Typed-arm request text. typed-llama appends the language specification itself."""
    return task_text(task, "constrained")


def write_messages(directory, system, user):
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "system.txt").write_text(system)
    (directory / "user.txt").write_text(user)


def planning_command(args, task, seed, mode, directory):
    if mode == "constrained":
        command = [str(args.cli), "task", task_prompt(task), "--model", str(args.model),
                   "--plan-only", "--temperature", str(args.temperature), "--seed", str(seed),
                   "--ubatch", str(args.ubatch)]
    elif mode == "cangjie":
        write_messages(directory, system_text(mode, True), planning_user(task, mode))
        command = [str(args.plain), "--model", str(args.model),
                   "--system-file", str(directory / "system.txt"),
                   "--user-file", str(directory / "user.txt"),
                   "--temperature", str(args.temperature), "--seed", str(seed),
                   "--ubatch", str(args.ubatch)]
    else:
        raise ValueError(f"unknown mode: {mode}")
    if args.ngl is not None:
        command.extend(["--ngl", str(args.ngl)])
    return command


def generation_command(args, task, mode, seed, directory, plan_file=None):
    plan = plan_file.read_text() if plan_file is not None else None
    write_messages(directory, system_text(mode, False), code_user(task, mode, plan))
    if mode == "constrained":
        command = [str(args.cli), "task", task_prompt(task), "--model", str(args.model),
                   "--n", str(code_budget(args, task)), "--temperature", str(args.temperature), "--seed", str(seed),
                   "--ubatch", str(args.ubatch)]
        if args.plan:
            if plan_file is None:
                raise ValueError("planning-enabled code generation requires a plan file")
            command.extend(["--plan-file", str(plan_file)])
        else:
            command.append("--skip-plan")
    elif mode == "cangjie":
        command = [str(args.plain), "--model", str(args.model),
                   "--system-file", str(directory / "system.txt"),
                   "--user-file", str(directory / "user.txt"),
                   "--n", str(code_budget(args, task)),
                   "--temperature", str(args.temperature), "--seed", str(seed),
                   "--ubatch", str(args.ubatch)]
    else:
        raise ValueError(f"unknown mode: {mode}")
    if args.ngl is not None:
        command.extend(["--ngl", str(args.ngl)])
    return command


def code_budget(args, task):
    return max(args.n, task.get("min_code_tokens", 1))


def failed_result(task, directory, status):
    return {"task_id": task["id"], "translation_success": False, "compilation_success": False,
            "harness_compile_success": False, "functional_success": False,
            "tests_passed": 0, "tests_total": task["tests_total"], "artifact_dir": str(directory),
            "status": status}


def generation_error(result):
    return result.returncode not in (0, 2) or any(marker in result.stderr.lower() for marker in (
        "model decode failed", "insufficient memory", "out of memory", "failed to load model",
        "failed to create context", "sampler emitted an invalid token"))


def prepare_cangjie_harness(source):
    marker = "main(): Int64 {"
    if source.count(marker) == 1:
        return source.replace(marker, "func benchmarkOriginalMain(): Int64 {", 1)
    return source


def cangjie_unittest_text(task):
    text = Path(task["unittest"]).read_text()
    needle = "typed_fn_" + task["entry_point"]
    if needle not in text:
        raise ValueError(f"unittest must call {needle}")
    return text.replace(needle, task["entry_point"])


def rename_candidate_main(cangjie):
    # Under cjc --test the process entry is test_entry. Rename the dummy main so
    # it is not a second entry, without editing the candidate's solve body.
    marker = "main(): Int64 {"
    if cangjie.count(marker) != 1:
        raise ValueError("CJ backend did not emit exactly one main")
    return cangjie.replace(marker, "func benchmarkOriginalMain(): Int64 {", 1)


def strip_ansi(text):
    return re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", text)


def parse_unittest_summary(output):
    match = re.search(
        r"Summary:\s*TOTAL:\s*(\d+)\s*"
        r"PASSED:\s*(\d+),\s*SKIPPED:\s*(\d+),\s*ERROR:\s*(\d+)\s*"
        r"FAILED:\s*(\d+)",
        strip_ansi(output), re.MULTILINE)
    if not match:
        raise ValueError("unittest output is missing a Summary block")
    total, passed, skipped, error, failed = (int(group) for group in match.groups())
    return {"total": total, "passed": passed, "skipped": skipped, "error": error, "failed": failed}


def timeout_each(seconds):
    if not math.isfinite(seconds) or seconds <= 0:
        raise ValueError("run timeout must be finite and positive")
    millis = max(1, math.ceil(seconds * 1000))
    return f"{millis}millis"


def evaluate(task, source, directory, args):
    """Compile the candidate, then score it with a checked-in std.unittest file."""
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "candidate.typed").write_text(source)
    result = {"task_id": task["id"], "translation_success": False, "compilation_success": False,
              "harness_compile_success": False, "functional_success": False,
              "tests_passed": 0, "tests_total": task["tests_total"], "artifact_dir": str(directory)}
    translated = execute([args.cli, "cj", "-"], args.compile_timeout, stdin=source, cwd=directory)
    save_process(directory, "translation", translated)
    if translated.timed_out or translated.returncode != 0:
        result["status"] = "translation_timeout" if translated.timed_out else "translation_failed"
        return result
    result["translation_success"] = True
    native = directory / "candidate.cj"
    native.write_text(translated.stdout)

    compiled = execute([args.cjc, native, "-o", directory / "candidate", "--set-runtime-rpath",
                        "--diagnostic-format", "noColor"], args.compile_timeout, cwd=directory)
    save_process(directory, "compilation", compiled)
    if compiled.timed_out or compiled.returncode != 0:
        result["status"] = "compile_timeout" if compiled.timed_out else "compile_failed"
        return result
    result["compilation_success"] = True
    if args.compile_only:
        result["status"] = "compiled"
        result["functional_success"] = None
        return result

    harness_source = directory / "candidate_for_test.cj"
    harness_source.write_text(rename_candidate_main(translated.stdout))
    unittest_file = Path(task["unittest"])
    shutil.copyfile(unittest_file, directory / unittest_file.name)
    tests_binary = directory / "tests"
    compiled_tests = execute(
        [args.cjc, harness_source.name, unittest_file.name, "--test", "-o", tests_binary.name,
         "--set-runtime-rpath", "--diagnostic-format", "noColor"],
        args.compile_timeout, cwd=directory)
    save_process(directory, "harness_compilation", compiled_tests)
    if compiled_tests.timed_out or compiled_tests.returncode != 0:
        result["status"] = "harness_compile_timeout" if compiled_tests.timed_out else "harness_compile_failed"
        return result
    result["harness_compile_success"] = True

    bundle_timeout = args.run_timeout * task["tests_total"]
    ran = execute([tests_binary, f"--timeout-each={timeout_each(args.run_timeout)}", "--no-color"],
                  bundle_timeout, cwd=directory, runtime_limits=True)
    save_process(directory, "execution", ran)
    if ran.timed_out:
        result["status"] = "runtime_timeout"
        return result
    try:
        summary = parse_unittest_summary(ran.stdout + "\n" + ran.stderr)
    except ValueError:
        result["status"] = "runtime_error"
        return result
    result["tests_passed"] = summary["passed"]
    result["unittest_summary"] = summary
    all_passed = (summary["total"] == task["tests_total"] and summary["passed"] == task["tests_total"]
                  and summary["failed"] == 0 and summary["error"] == 0 and summary["skipped"] == 0)
    result["functional_success"] = all_passed
    if all_passed:
        result["status"] = "pass"
    elif summary["failed"] > 0 and summary["error"] == 0:
        result["status"] = "wrong_answer"
    else:
        result["status"] = "runtime_error"
    return result


def evaluate_cangjie(task, source, directory, args):
    """Score a Cangjie program directly. The checked-in tests call solve, not typed_fn_solve."""
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    native = directory / "candidate.cj"
    native.write_text(source)
    result = {"task_id": task["id"], "translation_success": None, "compilation_success": False,
              "harness_compile_success": False, "functional_success": False,
              "tests_passed": 0, "tests_total": task["tests_total"], "artifact_dir": str(directory)}
    compiled = execute([args.cjc, native, "-o", directory / "candidate", "--set-runtime-rpath",
                        "--diagnostic-format", "noColor"], args.compile_timeout, cwd=directory)
    save_process(directory, "compilation", compiled)
    if compiled.timed_out or compiled.returncode != 0:
        result["status"] = "compile_timeout" if compiled.timed_out else "compile_failed"
        return result
    result["compilation_success"] = True
    if args.compile_only:
        result["status"] = "compiled"
        result["functional_success"] = None
        return result

    harness_source = directory / "candidate_for_test.cj"
    harness_source.write_text(prepare_cangjie_harness(source))
    unittest_source = directory / Path(task["unittest"]).name
    unittest_source.write_text(cangjie_unittest_text(task))
    tests_binary = directory / "tests"
    compiled_tests = execute(
        [args.cjc, harness_source.name, unittest_source.name, "--test", "-o", tests_binary.name,
         "--set-runtime-rpath", "--diagnostic-format", "noColor"],
        args.compile_timeout, cwd=directory)
    save_process(directory, "harness_compilation", compiled_tests)
    if compiled_tests.timed_out or compiled_tests.returncode != 0:
        result["status"] = "harness_compile_timeout" if compiled_tests.timed_out else "harness_compile_failed"
        return result
    result["harness_compile_success"] = True

    bundle_timeout = args.run_timeout * task["tests_total"]
    ran = execute([tests_binary, f"--timeout-each={timeout_each(args.run_timeout)}", "--no-color"],
                  bundle_timeout, cwd=directory, runtime_limits=True)
    save_process(directory, "execution", ran)
    if ran.timed_out:
        result["status"] = "runtime_timeout"
        return result
    try:
        summary = parse_unittest_summary(ran.stdout + "\n" + ran.stderr)
    except ValueError:
        result["status"] = "runtime_error"
        return result
    result["tests_passed"] = summary["passed"]
    result["unittest_summary"] = summary
    all_passed = (summary["total"] == task["tests_total"] and summary["passed"] == task["tests_total"]
                  and summary["failed"] == 0 and summary["error"] == 0 and summary["skipped"] == 0)
    result["functional_success"] = all_passed
    if all_passed:
        result["status"] = "pass"
    elif summary["failed"] > 0 and summary["error"] == 0:
        result["status"] = "wrong_answer"
    else:
        result["status"] = "runtime_error"
    return result


def pass_at_k(n, c, k):
    if not 1 <= k <= n or not 0 <= c <= n:
        raise ValueError("pass@k requires 1 <= k <= samples and 0 <= correct <= samples")
    if n - c < k:
        return 1.0
    return 1.0 - math.prod((n - c - i) / (n - i) for i in range(k))


def summarize(records, expected_attempts):
    modes = {}
    for mode in MODES:
        rows = [r for r in records if r["mode"] == mode]
        if not rows:
            continue
        count = len(rows)
        if mode == "cangjie":
            translated = 0
            translation_rate = None
        else:
            translated = sum(r["translation_success"] for r in rows)
            translation_rate = translated / count
        compiled = sum(r["compilation_success"] for r in rows)
        scored = [r["functional_success"] for r in rows]
        correct = None if any(value is None for value in scored) else sum(scored)
        by_task = {}
        for row in rows:
            by_task.setdefault(row["task_id"], []).append(row)
        available = min(len(group) for group in by_task.values())
        estimates = {} if correct is None else {
            str(k): sum(pass_at_k(len(group), sum(r["functional_success"] for r in group), k)
                        for group in by_task.values()) / len(by_task)
            for k in (1, 5, 10) if k <= available}
        failures = {}
        for row in rows:
            failures[row["status"]] = failures.get(row["status"], 0) + 1
        modes[mode] = {"attempts": count, "expected_attempts": expected_attempts,
                       "code_generation_attempts": sum(r.get("code_generation_attempted", True) for r in rows),
                       "translated": translated, "compiled": compiled, "correct": correct,
                       "translation_rate": translation_rate,
                       "compilation_rate": compiled / count,
                       "functional_rate": None if correct is None else correct / count,
                       "correct_given_compiled": None if correct is None or not compiled else correct / compiled,
                       "native_compile_given_translated": compiled / translated if translated else None,
                       "pass_at_k": estimates, "statuses": failures,
                       "per_task": {task: {"attempts": len(group),
                                           "compiled": sum(r["compilation_success"] for r in group),
                                           "correct": None if any(r["functional_success"] is None for r in group)
                                           else sum(r["functional_success"] for r in group)}
                                    for task, group in by_task.items()}}
    pairs = {}
    for row in records:
        pairs.setdefault((row["task_id"], row["sample_index"]), {})[row["mode"]] = row
    complete_pairs = [pair for pair in pairs.values() if all(mode in pair for mode in MODES)]
    paired = {"pairs": len(complete_pairs)}
    for metric in ("compilation_success", "functional_success"):
        if metric == "functional_success" and any(
                pair[mode][metric] is None for pair in complete_pairs for mode in MODES):
            paired[metric] = None
            continue
        left, right = MODES
        tally = {"both": 0, f"{left}_only": 0, f"{right}_only": 0, "neither": 0}
        for pair in complete_pairs:
            a, b = (pair[mode][metric] for mode in MODES)
            tally["both" if a and b else f"{left}_only" if a else f"{right}_only" if b else "neither"] += 1
        paired[metric] = tally
    complete = all(modes.get(mode, {}).get("attempts") == expected_attempts for mode in MODES)
    infrastructure_errors = sum(r["status"] in ("generation_error", "planning_error") for r in records)
    return {"complete": complete, "valid_comparison": complete and not infrastructure_errors,
            "infrastructure_errors": infrastructure_errors,
            "modes": modes, "paired": paired}


def write_report(output, config, records):
    summary = summarize(records, len(config["task_ids"]) * config["samples"])
    (output / "summary.json").write_text(json.dumps({"config": config, **summary}, indent=2) + "\n")
    text = ["# Typed pipeline vs direct Cangjie", "", f"Suite: `{config['suite_name']}` (adapted HumanEval subset).",
            f"Complete run: **{summary['complete']}**. Valid comparison: **{summary['valid_comparison']}**. Planning input: **{config['plan_source']}**.", "",
            "This run skips planning. Both arms receive the task and their own language specification only."
            if config["plan_source"] == "none" else
            "Each arm plans in its own language from the same task, seed, and temperature. The plans are not shared.",
            "The typed arm is compiled only after `cj` translation and standalone `cjc` both succeed.",
            "The Cangjie arm is compiled as generated. It has no translation step.",
            "Functional tests were not executed for this run." if config.get("compile_only") else
            "Functional success requires every std.unittest @Test case to pass with no FAILED, ERROR, or SKIPPED results.",
            "Both primary rates include failed generation, translation, compilation, and execution attempts.",
            "No code extraction, repair, formatting, or deterministic completion is applied.", "",
            "| Mode | Attempts | Translated | Compiled / attempts | Correct / attempts | Correct / compiled |",
            "| --- | ---: | ---: | ---: | ---: | ---: |"]
    for mode, stats in summary["modes"].items():
        if stats["functional_rate"] is None:
            correct_cell, conditional = "not run", "not run"
        else:
            conditional = "n/a" if stats["correct_given_compiled"] is None else f"{stats['correct_given_compiled']:.1%}"
            correct_cell = f"{stats['correct']} ({stats['functional_rate']:.1%})"
        translated_cell = "n/a" if stats["translation_rate"] is None else str(stats["translated"])
        text.append(f"| {mode} | {stats['attempts']} | {translated_cell} | "
                    f"{stats['compiled']} ({stats['compilation_rate']:.1%}) | "
                    f"{correct_cell} | {conditional} |")
    compiled_headers = " | ".join(f"{mode} compiled" for mode in MODES)
    correct_headers = " | ".join(f"{mode} correct" for mode in MODES)
    text += ["", f"| Task | Code tokens | {compiled_headers} | {correct_headers} |",
             "| --- | ---: | " + " | ".join("---:" for _ in MODES) + " | " + " | ".join("---:" for _ in MODES) + " |"]
    for task in config["task_ids"]:
        cells = []
        for metric in ("compiled", "correct"):
            for mode in MODES:
                group = summary["modes"].get(mode, {}).get("per_task", {}).get(task)
                if group is None:
                    cells.append("pending")
                elif group[metric] is None:
                    cells.append("not run")
                else:
                    cells.append(f"{group[metric]}/{group['attempts']}")
        text.append(f"| {task} | {config['code_tokens_by_task'][task]} | " + " | ".join(cells) + " |")
    text += ["", "`summary.json` also includes pass@k estimates, per-task counts, native compile rates",
             "conditional on translation, failure categories, and paired wins/losses.",
             "`results.jsonl` and each attempt directory retain the raw code, commands, timings, and diagnostics.",
             "These scores are not official HumanEval scores.", ""]
    (output / "report.md").write_text("\n".join(text))


def positive(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--model", type=Path, help="GGUF file or JSON model plugin; required for generation")
    parser.add_argument("--validate", action="store_true", help="compile and execute reference solutions only; no model")
    parser.add_argument("--suite", type=Path, default=ROOT / "benchmarks/tasks.json", help="task manifest JSON")
    parser.add_argument("--cli", type=Path, default=ROOT / "build/typed-llama", help="built typed-language runner")
    parser.add_argument("--plain", type=Path, default=ROOT / "build/plain-llama",
                        help="unconstrained sampler used only for the Cangjie arm")
    parser.add_argument("--cjc", default="cjc", help="Cangjie compiler path or command")
    parser.add_argument("--output", type=Path, help="new results directory; defaults to benchmark-results/<UTC timestamp>")
    parser.add_argument("--tasks", nargs="+", help="task IDs to run; defaults to the whole suite")
    parser.add_argument("--samples", type=positive, default=1, help="samples per task and mode")
    parser.add_argument("--seed", type=int, default=42, help="first seed; sample i uses seed+i in each mode")
    parser.add_argument("--temperature", type=float, default=0.8, help="sampling randomness; 0 selects greedy")
    parser.add_argument("--n", type=positive, default=2048, help="code token budget per attempt, raised to each task's minimum")
    parser.add_argument("--plan", action=argparse.BooleanOptionalAction, default=True,
                        help="plan in each arm's own language; --no-plan skips planning for both")
    parser.add_argument("--ngl", type=int, help="GPU layer override for both modes")
    parser.add_argument("--ubatch", type=positive, default=64, help="physical prompt batch size for both modes")
    parser.add_argument("--planning-timeout", type=float, default=600, help="wall seconds for each arm's planning stage")
    parser.add_argument("--generation-timeout", type=float, default=300, help="wall seconds per code generation attempt")
    parser.add_argument("--compile-timeout", type=float, default=20, help="wall seconds per translation/compilation")
    parser.add_argument("--run-timeout", type=float, default=5,
                        help="wall seconds per std.unittest case; the process-group limit is this times the case count")
    parser.add_argument("--compile-only", action="store_true",
                        help="translate and compile each candidate; do not execute functional tests")
    args = parser.parse_args(argv)
    if not args.validate and args.model is None:
        parser.error("generation requires --model FILE (or use --validate without a model)")
    if not math.isfinite(args.temperature) or args.temperature < 0 or not 0 <= args.seed < 2**32:
        parser.error("temperature must be finite and nonnegative; seed must be an unsigned 32-bit integer")
    if args.ngl is not None and args.ngl < 0:
        parser.error("--ngl must be nonnegative")
    if args.n >= 2**31 or args.ubatch >= 2**31 or (args.ngl is not None and args.ngl >= 2**31):
        parser.error("--n, --ubatch, and --ngl must fit signed 32-bit integers")
    if any(not math.isfinite(t) or t <= 0 for t in
           (args.planning_timeout, args.generation_timeout, args.compile_timeout, args.run_timeout)):
        parser.error("timeouts must be finite and positive")
    resources = contextlib.ExitStack()
    try:
        suite = load_suite(args.suite)
        selected = set(args.tasks or (t["id"] for t in suite["tasks"]))
        tasks = [t for t in suite["tasks"] if t["id"] in selected]
        if selected != {t["id"] for t in tasks}:
            raise ValueError("unknown task ID; inspect benchmarks/tasks.json for available IDs")
        args.cli = args.cli.resolve(strict=True)
        if not args.validate:
            args.plain = args.plain.resolve(strict=True)
        compiler = shutil.which(args.cjc)
        if compiler is None:
            raise ValueError("cjc was not found; install the Cangjie SDK or supply --cjc")
        args.cjc = str(Path(compiler).resolve())
        help_result = execute([args.cli, "task", "--help"], 10)
        if help_result.returncode != 0 or any(option not in help_result.stdout for option in
                                            ("--force-postfix", "--plan-only", "--plan-file")):
            raise ValueError("rebuild typed-llama: benchmark runs require a binary whose forced postfix is opt-in (--force-postfix, --plan-only, --plan-file)")
        model_files = []
        if not args.validate:
            resources.enter_context(model_lock())
            args.model = args.model.resolve(strict=True)
            model_files.append(args.model)
            if args.model.suffix.lower() == ".json":
                weights = json.loads(args.model.read_text())["model"]
                model_files.append((args.model.parent / weights).resolve(strict=True))
        timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        output = (args.output or ROOT / "benchmark-results" / timestamp).resolve()
        output.mkdir(parents=True, exist_ok=False)
        config = {"created_utc": timestamp, "suite_name": suite["name"], "suite_sha256": sha256(args.suite),
                  "runner_sha256": sha256(__file__), "task_ids": [t["id"] for t in tasks],
                  "samples": args.samples, "seed": args.seed, "temperature": args.temperature,
                  "code_tokens": args.n, "planning": args.plan, "automatic_completion": False,
                  "shared_planning": False, "per_arm_planning": args.plan, "planning_timeout": args.planning_timeout,
                  "plan_source": "local_model" if args.plan else "none",
                  "local_planning": args.plan,
                  "code_tokens_by_task": {t["id"]: code_budget(args, t) for t in tasks},
                  "ngl": args.ngl, "ubatch": args.ubatch, "cli": str(args.cli), "cli_sha256": sha256(args.cli),
                  "plain": None if args.validate else str(args.plain),
                  "plain_sha256": None if args.validate else sha256(args.plain),
                  "compiler": args.cjc, "compiler_version": execute([args.cjc, "--version"], 10).stdout.strip(),
                  "generation_timeout": args.generation_timeout, "compile_timeout": args.compile_timeout,
                  "run_timeout": args.run_timeout, "compile_only": args.compile_only, "python": sys.version,
                  "platform": platform.platform(), "machine": platform.machine(),
                  "model_files": [{"path": str(p), "sha256": sha256(p)} for p in model_files],
                  "git_revision": execute(["git", "rev-parse", "HEAD"], 10, cwd=ROOT).stdout.strip(),
                  "git_status": execute(["git", "status", "--porcelain"], 10, cwd=ROOT).stdout}
        (output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
        shutil.copyfile(__file__, output / "runner.snapshot.py")
        shutil.copyfile(args.suite, output / "suite.snapshot.json")
        references = []
        for task in tasks:
            reference = (args.suite.parent / task["reference"]).resolve(strict=True)
            evaluated = evaluate(task, reference.read_text(), output / "references" / task["id"], args)
            evaluated["reference_sha256"] = sha256(reference)
            references.append(evaluated)
            (output / "references.json").write_text(json.dumps(references, indent=2) + "\n")
            reference_ok = evaluated["compilation_success"] if args.compile_only else evaluated["functional_success"]
            if not reference_ok:
                raise ValueError(f"reference failed for {task['id']}: {evaluated['status']}; inspect {output}")
        print(f"Validated {len(tasks)} reference solutions; artifacts: {output}", flush=True)
        if args.validate:
            return 0
        records = []
        write_report(output, config, records)
        with (output / "results.jsonl").open("w") as stream:
            for task_index, task in enumerate(tasks):
                for sample in range(args.samples):
                    seed = (args.seed + sample) % 2**32
                    order = MODES if (task_index + sample) % 2 == 0 else MODES[::-1]
                    for mode in order:
                        directory = output / task["id"] / f"sample_{sample:03d}_{mode}"
                        directory.mkdir(parents=True)
                        plan_file = None
                        plan_sha = None
                        plan_result = None
                        plan_failure = None
                        if args.plan:
                            plan_directory = output / "plans" / task["id"] / f"sample_{sample:03d}_{mode}"
                            write_messages(plan_directory, system_text(mode, True), planning_user(task, mode))
                            print(f"{task['id']} sample={sample} seed={seed} {mode}: planning", flush=True)
                            plan_result = execute(planning_command(args, task, seed, mode, plan_directory), args.planning_timeout)
                            save_process(plan_directory, "planning", plan_result)
                            if plan_result.timed_out:
                                plan_failure = "planning_timeout"
                            elif plan_result.returncode != 0:
                                plan_failure = "planning_error"
                            elif not plan_result.stdout.strip():
                                plan_failure = "empty_plan"
                            else:
                                plan_file = plan_directory / "plan.txt"
                                plan_file.write_text(plan_result.stdout)
                                plan_sha = sha256(plan_file)
                            metadata = {"task_id": task["id"], "sample_index": sample, "mode": mode, "seed": seed,
                                        "status": plan_failure or "pass", "plan_sha256": plan_sha,
                                        "seconds": plan_result.seconds,
                                        "local_planner_invoked": True,
                                        "source": config["plan_source"], "artifact_dir": str(plan_directory)}
                            (plan_directory / "plan_metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
                            with (output / "planning_results.jsonl").open("a") as plans:
                                plans.write(json.dumps(metadata) + "\n")
                            if plan_file is not None:
                                print(f"  saved {mode} plan {plan_sha[:12]} ({len(plan_result.stdout)} characters)", flush=True)
                        generation = None
                        if plan_failure:
                            row = failed_result(task, directory, plan_failure)
                        else:
                            if plan_file is not None and sha256(plan_file) != plan_sha:
                                raise ValueError("plan changed before code generation; refusing an unmatched comparison")
                            print(f"{task['id']} sample={sample} seed={seed} {mode} n={code_budget(args, task)}: generating", flush=True)
                            generation = execute(generation_command(args, task, mode, seed, directory, plan_file), args.generation_timeout)
                            if plan_file is not None and sha256(plan_file) != plan_sha:
                                raise ValueError("plan changed during code generation; refusing an unmatched comparison")
                            save_process(directory, "generation", generation)
                            if generation.timed_out or generation_error(generation):
                                row = failed_result(task, directory, "generation_timeout" if generation.timed_out else "generation_error")
                            elif mode == "cangjie":
                                row = evaluate_cangjie(task, generation.stdout, directory, args)
                            else:
                                row = evaluate(task, generation.stdout, directory, args)
                        row.update(mode=mode, sample_index=sample, seed=seed,
                                   code_tokens=code_budget(args, task),
                                   plan_file=str(plan_file) if plan_file is not None else None,
                                   plan_sha256=plan_sha, planning_seconds=plan_result.seconds if plan_result else 0,
                                   code_generation_attempted=generation is not None,
                                   generation_returncode=generation.returncode if generation else None,
                                   generation_seconds=generation.seconds if generation else 0)
                        records.append(row)
                        stream.write(json.dumps(row) + "\n")
                        stream.flush()
                        write_report(output, config, records)
                        tests = "skipped" if row["functional_success"] is None else f"{row['tests_passed']}/{row['tests_total']}"
                        print(f"  {row['status']}; compiled={row['compilation_success']}; tests={tests}", flush=True)
                        if row["status"] == "generation_error":
                            raise ValueError(f"generation infrastructure failed; run is not a valid comparison; inspect {directory / 'generation.stderr'}")
        print((output / "report.md").read_text(), flush=True)
        return 0
    except (OSError, ValueError, KeyError) as error:
        print(f"benchmark error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("benchmark interrupted; completed results remain saved", file=sys.stderr)
        return 130
    finally:
        resources.close()


if __name__ == "__main__":
    def terminate(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, terminate)
    raise SystemExit(main())
