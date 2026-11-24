import argparse
import copy
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from benchmarks import run as bench


SUITE = bench.load_suite(bench.ROOT / "benchmarks/tasks.json")
ADD = next(task for task in SUITE["tasks"] if task["id"] == "he_053_add")
CLI = os.environ.get("BENCHMARK_CLI", str(bench.ROOT / "build/typed-cangjie"))
TRANSLATOR = os.environ.get("BENCHMARK_TRANSLATOR", str(bench.ROOT / "build/emit-cangjie"))
CJC = shutil.which(os.environ.get("BENCHMARK_CJC") or "cjc")


def settings(**overrides):
    values = dict(cli=CLI, translator=Path(TRANSLATOR), plain=Path("plain-llama"), cjc=CJC,
                  compile_timeout=20, run_timeout=5,
                  model=Path("supplied model.gguf"), n=512, temperature=0.8,
                  plan=False, ngl=0, ubatch=64, compile_only=False)
    values.update(overrides)
    return argparse.Namespace(**values)


def process(code=0, stdout="", timed_out=False):
    return bench.ProcessResult(["fake"], code, timed_out, 0.01, stdout, "diagnostic")


class BenchmarkTests(unittest.TestCase):
    def test_normal_run_generates_every_task_in_both_modes_with_planning(self):
        self.run_generation_fixture()

    def run_generation_fixture(self):
        commands = []
        plan_commands = []
        seen_plans = {}

        def fake_execute(command, timeout, **kwargs):
            if "--help" in command and "generate" in command:
                return process(stdout="--force-postfix --plan-only --plan-file")
            planning = "--plan-only" in command or ("--system-file" in command and "--n" not in command)
            if planning:
                plan_commands.append(command)
                return process(stdout="Shared plan with preserved whitespace.\n\n")
            generating = (len(command) > 1 and command[1] == "generate") or "--user-file" in command
            if generating:
                commands.append(command)
                if "--plan-file" in command:
                    plan_path = Path(command[command.index("--plan-file") + 1])
                    seen_plans.setdefault(command[2], []).append((str(plan_path), plan_path.read_bytes()))
                return process(stdout="main(): Int64 { return 0; }")
            return process(stdout="fixture version")

        def fake_evaluate(task, source, directory, args):
            return dict(task_id=task["id"], translation_success=True, compilation_success=True,
                        harness_compile_success=True, functional_success=True,
                        tests_passed=task["tests_total"], tests_total=task["tests_total"], status="pass")

        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            model = temporary / "fixture.gguf"
            model.write_bytes(b"model-free fixture, never loaded")
            output = temporary / "results"
            with patch.object(bench, "execute", side_effect=fake_execute), \
                 patch.object(bench, "evaluate", side_effect=fake_evaluate), \
                 patch.object(bench, "score_translated", side_effect=fake_evaluate), \
                 patch.object(bench, "evaluate_cangjie", side_effect=fake_evaluate), \
                 patch.object(bench, "model_lock", return_value=contextlib.nullcontext()), \
                 contextlib.redirect_stdout(io.StringIO()):
                arguments = ["--model", str(model), "--cli", sys.executable, "--translator", sys.executable,
                             "--plain", sys.executable,
                             "--cjc", sys.executable, "--output", str(output)]
                status = bench.main(arguments)
            self.assertEqual(status, 0)
            typed = [command for command in commands if len(command) > 1 and command[1] == "generate"]
            cangjie = [command for command in commands if "--user-file" in command]
            self.assertEqual(len(typed), 35)
            self.assertEqual(len(cangjie), 35)
            self.assertEqual(len(plan_commands), 70)
            self.assertEqual(sum("--unconstrained" in command for command in commands), 0)
            for command in typed:
                self.assertNotIn("--skip-plan", command)
                self.assertNotIn("--format", command)
                self.assertNotIn("--force-postfix", command)
                self.assertEqual(command[command.index("--n") + 1], "2048")
            for command in cangjie:
                self.assertNotIn("--skip-plan", command)
                self.assertIn("--n", command)
                user = Path(command[command.index("--user-file") + 1]).read_text()
                self.assertIn("Implementation plan:", user)
                self.assertIn("Implement func", user)
            for task in SUITE["tasks"]:
                self.assertEqual(sum(len(command) > 2 and command[2] == bench.task_prompt(task) for command in typed), 1)
                self.assertEqual(len(seen_plans[bench.task_prompt(task)]), 1)
            summary = json.loads((output / "summary.json").read_text())
            self.assertTrue(summary["complete"])
            self.assertTrue(summary["config"]["planning"])
            self.assertFalse(summary["config"]["shared_planning"])
            self.assertTrue(summary["config"]["per_arm_planning"])
            self.assertTrue(summary["config"]["local_planning"])
            self.assertEqual(summary["config"]["plan_source"], "local_model")
            rows = [json.loads(line) for line in (output / "results.jsonl").read_text().splitlines()]
            for task in SUITE["tasks"]:
                group = [row for row in rows if row["task_id"] == task["id"]]
                self.assertEqual(len(group), 2)
                self.assertEqual(len({row["plan_file"] for row in group}), 2)
                self.assertTrue(all(row["plan_sha256"] for row in group))
            for mode in bench.MODES:
                self.assertEqual(summary["modes"][mode]["attempts"], 35)

    def test_suite_and_prompt_separation(self):
        self.assertEqual(len(SUITE["tasks"]), 35)
        self.assertEqual(sum(t["tests_total"] for t in SUITE["tasks"]), 592)
        self.assertEqual(sum(t["upstream_literal_tests"] for t in SUITE["tasks"]), 212)
        for task in SUITE["tasks"]:
            prompt = bench.task_prompt(task)
            self.assertIn("func solve(", prompt)
            self.assertNotIn(task["reference"], prompt)
            self.assertNotIn("typed_fn_solve", prompt)
            self.assertNotIn("@Expect", prompt)
            self.assertTrue((bench.ROOT / "benchmarks" / task["reference"]).is_file())
            unittest_file = Path(task["unittest"])
            self.assertTrue(unittest_file.is_file())
            text = unittest_file.read_text()
            self.assertIn("typed_fn_solve", text)
            self.assertEqual(text.count("@Test"), task["tests_total"])
            self.assertEqual(text.count("@Expect"), task["tests_total"])

    def test_prompts_share_the_request_and_append_each_specification(self):
        from prompts import (CANGJIE_SPEC, TYPED_CODE_PREFIX, TYPED_PLAN_PREFIX, TYPED_SPEC,
                             code_user, planning_user, problem_statement, system_text)
        shared = problem_statement(ADD)
        self.assertIn(shared, bench.task_prompt(ADD))
        self.assertIn(shared, code_user(ADD, "cangjie"))
        self.assertNotIn("atomic arguments", code_user(ADD, "cangjie"))
        self.assertIn("atomic arguments", code_user(ADD, "constrained"))
        self.assertTrue(system_text("constrained", False).startswith(TYPED_CODE_PREFIX))
        self.assertIn(TYPED_SPEC, system_text("constrained", False))
        self.assertIn(TYPED_SPEC, system_text("constrained", True))
        self.assertTrue(system_text("constrained", True).startswith(TYPED_PLAN_PREFIX))
        self.assertIn(CANGJIE_SPEC, system_text("cangjie", False))
        self.assertIn(CANGJIE_SPEC, system_text("cangjie", True))
        self.assertNotIn(TYPED_SPEC, system_text("cangjie", False))
        self.assertNotIn(CANGJIE_SPEC, system_text("constrained", False))
        typed_plan = planning_user(ADD, "constrained")
        cangjie_plan = planning_user(ADD, "cangjie")
        self.assertTrue(typed_plan.startswith("Plan the implementation for this request: "))
        self.assertTrue(cangjie_plan.startswith("Plan the implementation for this request: "))
        self.assertIn(shared, typed_plan)
        self.assertIn(shared, cangjie_plan)
        planned = code_user(ADD, "cangjie", plan="keep the same seed")
        self.assertIn("Implementation plan:\nkeep the same seed", planned)
        header = (bench.ROOT / "src/language_prompt.h").read_text()
        planning = (bench.ROOT / "src/planning.h").read_text()
        self.assertIn("Write exactly one program satisfying this language specification.", header)
        self.assertIn("Use spaces between tokens for clarity.", header)
        self.assertIn("The following specification governs both the planned statement sketches and the eventual program:", header)
        self.assertIn("infix expressions are not allowed.", header)
        self.assertIn("Plan the implementation for this request: ", planning)
        self.assertIn("Produce a program for this request: ", planning)
        self.assertIn("func add(a: Int64, b: Int64): Int64", CANGJIE_SPEC)
        self.assertIn("main", CANGJIE_SPEC)

    def test_paired_commands_keep_planning_and_budgets_aligned(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            constrained = bench.generation_command(settings(), ADD, "constrained", 42, directory)
            cangjie = bench.generation_command(settings(), ADD, "cangjie", 42, directory)
            self.assertIn("--skip-plan", constrained)
            self.assertNotIn("--skip-plan", cangjie)
            self.assertNotIn("--force-postfix", constrained)
            self.assertNotIn("--format", constrained)
            self.assertEqual(constrained[constrained.index("--seed") + 1], "42")
            self.assertEqual(cangjie[cangjie.index("--seed") + 1], "42")
            self.assertEqual(constrained[constrained.index("--n") + 1], cangjie[cangjie.index("--n") + 1])
            plan = directory / "shared plan.txt"
            plan.write_text("plan")
            planned = bench.generation_command(settings(plan=True), ADD, "constrained", 42, directory, plan)
            self.assertNotIn("--skip-plan", planned)
            self.assertEqual(planned[planned.index("--plan-file") + 1], str(plan))
            with self.assertRaises(ValueError):
                bench.generation_command(settings(plan=True), ADD, "constrained", 42, directory)

    def test_planning_is_independent_of_code_mode_and_budget(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            command = bench.planning_command(settings(plan=True, n=1), ADD, 42, "constrained", directory)
            self.assertIn("--plan-only", command)
            self.assertNotIn("--n", command)
            self.assertNotIn("--unconstrained", command)
            self.assertNotIn("--skip-plan", command)
            cangjie = bench.planning_command(settings(plan=True, n=1), ADD, 42, "cangjie", directory)
            self.assertNotIn("--n", cangjie)
            self.assertIn("--system-file", cangjie)
            self.assertEqual(command[command.index("--seed") + 1], cangjie[cangjie.index("--seed") + 1])
            self.assertIn((bench.ROOT / "docs/cangjie-subset.md").read_text(),
                          Path(cangjie[cangjie.index("--system-file") + 1]).read_text())

    def test_fibonacci_budgets_are_at_least_2048_in_both_modes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for task in SUITE["tasks"]:
                if task["id"] not in ("he_055_fibonacci", "he_063_fibfib"):
                    continue
                for mode in bench.MODES:
                    for requested, expected in ((512, 2048), (2048, 2048), (4096, 4096)):
                        command = bench.generation_command(settings(n=requested), task, mode, 42, directory)
                        self.assertEqual(command[command.index("--n") + 1], str(expected))

    def test_unittest_harness_renames_main_and_parses_summary(self):
        renamed = bench.rename_candidate_main("main(): Int64 {\n    return 0\n}\n")
        self.assertIn("func benchmarkOriginalMain(): Int64 {", renamed)
        self.assertNotIn("main(): Int64 {", renamed)
        with self.assertRaises(ValueError):
            bench.rename_candidate_main("main() {}")
        summary = bench.parse_unittest_summary(
            "Summary: TOTAL: 2\n    PASSED: 1, SKIPPED: 0, ERROR: 1\n    FAILED: 0\n")
        self.assertEqual(summary, {"total": 2, "passed": 1, "skipped": 0, "error": 1, "failed": 0})
        self.assertEqual(bench.timeout_each(5), "5000millis")
        self.assertEqual(bench.timeout_each(0.2), "200millis")

    def test_validation_rejects_inline_tests_and_unsafe_names(self):
        suite = json.loads((bench.ROOT / "benchmarks/tasks.json").read_text())
        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            shutil.copytree(bench.ROOT / "benchmarks/cjtests", temporary / "cjtests")
            broken = copy.deepcopy(suite)
            broken["tasks"][0]["tests"] = [{"args": [1, 2], "expected": 3}]
            path = temporary / "suite.json"
            path.write_text(json.dumps(broken))
            with self.assertRaisesRegex(ValueError, "obsolete"):
                bench.load_suite(path)
            broken = copy.deepcopy(suite)
            broken["tasks"][0]["entry_point"] = "solve); injected"
            path.write_text(json.dumps(broken))
            with self.assertRaises(ValueError):
                bench.load_suite(path)
            broken = copy.deepcopy(suite)
            path.write_text(json.dumps(broken))
            (temporary / "cjtests" / f"{broken['tasks'][0]['id']}_test.cj").unlink()
            with self.assertRaisesRegex(ValueError, "missing unittest"):
                bench.load_suite(path)

    def test_failure_denominators_and_pairing(self):
        rows = []
        for mode in bench.MODES:
            for sample in range(2):
                passed = sample == 0
                compiled = passed or mode == "cangjie"
                rows.append(dict(mode=mode, task_id="one", sample_index=sample,
                                 translation_success=compiled, compilation_success=compiled,
                                 functional_success=passed, status="pass" if passed else "wrong_answer"))
        summary = bench.summarize(rows, 2)
        self.assertTrue(summary["complete"])
        self.assertEqual(summary["modes"]["constrained"]["compilation_rate"], 0.5)
        self.assertEqual(summary["modes"]["cangjie"]["compilation_rate"], 1)
        self.assertIsNone(summary["modes"]["cangjie"]["translation_rate"])
        for mode in bench.MODES:
            self.assertEqual(summary["modes"][mode]["functional_rate"], 0.5)
            self.assertEqual(summary["modes"][mode]["pass_at_k"]["1"], 0.5)
        self.assertEqual(summary["paired"]["compilation_success"]["cangjie_only"], 1)
        self.assertFalse(bench.summarize(rows[:-1], 2)["complete"])

    def test_pass_at_k(self):
        self.assertAlmostEqual(bench.pass_at_k(5, 1, 2), 0.4)
        self.assertEqual(bench.pass_at_k(5, 0, 5), 0)
        self.assertEqual(bench.pass_at_k(5, 5, 1), 1)
        with self.assertRaises(ValueError):
            bench.pass_at_k(1, 1, 5)

    def test_process_timeout(self):
        result = bench.execute([sys.executable, "-c", "import time; time.sleep(30)"], 0.1)
        self.assertTrue(result.timed_out)
        self.assertNotEqual(result.returncode, 0)

    def test_model_lock_rejects_overlapping_benchmark_runs(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "model.lock"
            with bench.model_lock(path):
                with self.assertRaisesRegex(ValueError, "already running"):
                    with bench.model_lock(path):
                        self.fail("overlapping runner acquired the model lock")
            with bench.model_lock(path):
                self.assertTrue(path.is_file())

    def test_decode_failures_are_not_misreported_as_bad_generated_code(self):
        self.assertFalse(bench.generation_error(process(2, stdout="unfinished code")))
        broken = process(2)
        broken.stderr = "Insufficient Memory; model decode failed"
        self.assertTrue(bench.generation_error(broken))
        self.assertTrue(bench.generation_error(process(1)))

    def test_stage_failures_and_wrong_answers_remain_separate(self):
        native = "main(): Int64 {\n    return 0\n}\n"
        failed_summary = ("Summary: TOTAL: 41\n    PASSED: 40, SKIPPED: 0, ERROR: 0\n"
                          "    FAILED: 1\n")
        error_summary = ("Summary: TOTAL: 41\n    PASSED: 40, SKIPPED: 0, ERROR: 1\n"
                         "    FAILED: 0\n")
        cases = [([process(2)], "translation_failed", False),
                 ([process(stdout=native), process(1)], "compile_failed", False),
                 ([process(stdout=native), process(), process(1)], "harness_compile_failed", True),
                 ([process(stdout=native), process(), process(), process(stdout=failed_summary)],
                  "wrong_answer", True),
                 ([process(stdout=native), process(), process(), process(stdout=error_summary)],
                  "runtime_error", True),
                 ([process(stdout=native), process(), process(), process(-9, timed_out=True)],
                  "runtime_timeout", True)]
        for replies, status, compiled in cases:
            with self.subTest(status=status), tempfile.TemporaryDirectory() as temporary:
                with patch.object(bench, "execute", side_effect=replies):
                    result = bench.evaluate(ADD, "raw source", temporary, settings())
                self.assertEqual(result["status"], status)
                self.assertEqual(result["compilation_success"], compiled)
                self.assertFalse(result["functional_success"])
                self.assertEqual((Path(temporary) / "candidate.typed").read_text(), "raw source")
                if status == "wrong_answer":
                    self.assertEqual(result["tests_passed"], 40)


@unittest.skipUnless(CJC and Path(CLI).is_file() and Path(TRANSLATOR).is_file(),
                     "requires the built generator, translator, and a Cangjie compiler")
class NativeBenchmarkTests(unittest.TestCase):
    def test_all_reference_solutions(self):
        with tempfile.TemporaryDirectory(prefix="ai language benchmarks ") as temporary:
            for task in SUITE["tasks"]:
                with self.subTest(task=task["id"]):
                    reference = bench.ROOT / "benchmarks" / task["reference"]
                    result = bench.evaluate(task, reference.read_text(), Path(temporary) / task["id"], settings())
                    self.assertTrue(result["compilation_success"], result)
                    self.assertTrue(result["functional_success"], result)
                    self.assertEqual(result["tests_passed"], task["tests_total"])

    def test_invalid_wrong_signature_wrong_answer_and_nontermination(self):
        fixtures = [
            ("main(): Int64 {return true;}", {"translation_failed"}, False),
            ("main(): Int64 {return 0;}", {"harness_compile_failed"}, True),
            ("func solve(x: Int64, y: Int64): Int64 {return 0;} main(): Int64 {return 0;}",
             {"wrong_answer"}, True),
            ("func solve(x: Int64, y: Int64): Int64 {while(true){continue;}} main(): Int64 {return 0;}",
             {"runtime_timeout", "runtime_error"}, True),
        ]
        with tempfile.TemporaryDirectory() as temporary:
            for index, (source, statuses, compiled) in enumerate(fixtures):
                with self.subTest(source=source[:40]):
                    timeout = 0.2 if any("runtime" in status for status in statuses) else 5
                    result = bench.evaluate(ADD, source, Path(temporary) / str(index), settings(run_timeout=timeout))
                    self.assertIn(result["status"], statuses, result)
                    self.assertEqual(result["compilation_success"], compiled)
                    self.assertFalse(result["functional_success"])

    def test_direct_cangjie_addition_uses_solve(self):
        source = ("func solve(x: Int64, y: Int64): Int64 {\n"
                  "    return x + y\n"
                  "}\n\n"
                  "main(): Int64 {\n"
                  "    return 0\n"
                  "}\n")
        with tempfile.TemporaryDirectory() as temporary:
            result = bench.evaluate_cangjie(ADD, source, Path(temporary), settings())
            self.assertTrue(result["compilation_success"], result)
            self.assertTrue(result["functional_success"], result)
            self.assertEqual(result["tests_passed"], ADD["tests_total"])
            self.assertIsNone(result["translation_success"])


if __name__ == "__main__":
    unittest.main()
