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
for task in SUITE["tasks"]:
    task["model_tokens"] = 100
    task["code_tokens"] = bench.code_budget_for(task["model_tokens"])
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
    def setUp(self):
        self.token_counts = patch.object(
            bench, "model_token_counts",
            side_effect=lambda model, paths: [100] * len(list(paths)))
        self.token_counts.start()

    def tearDown(self):
        self.token_counts.stop()

    def test_normal_run_generates_every_task_in_both_modes_with_planning(self):
        self.run_generation_fixture()

    def test_plan_only_writes_plans_and_skips_generation(self):
        commands, plan_commands = [], []

        def fake_execute(command, timeout, **kwargs):
            if "--help" in command and "generate" in command:
                return process(stdout="--force-postfix --plan-only --plan-file")
            planning = "--plan-only" in command or ("--system-file" in command and "--n" not in command)
            if planning:
                plan_commands.append(command)
                return process(stdout=f"plan for {' '.join(command)}\n")
            return process(stdout="fixture version")

        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            model = temporary / "fixture.gguf"
            model.write_bytes(b"model-free fixture, never loaded")
            output = temporary / "plans"
            with patch.object(bench, "execute", side_effect=fake_execute), \
                 patch.object(bench, "evaluate", side_effect=lambda *args: dict(
                     compilation_success=True, functional_success=True, status="pass")), \
                 patch.object(bench, "model_lock", return_value=contextlib.nullcontext()), \
                 contextlib.redirect_stdout(io.StringIO()):
                status = bench.main(["--model", str(model), "--cli", sys.executable, "--translator", sys.executable,
                                     "--plain", sys.executable, "--cjc", sys.executable,
                                     "--plan-only", "--output", str(output), "--tasks", "he_053_add"])
            self.assertEqual(status, 0)
            self.assertEqual(len(plan_commands), 2)
            self.assertEqual(commands, [])
            self.assertFalse((output / "results.jsonl").exists())
            summary = json.loads((output / "plans_summary.json").read_text())
            self.assertEqual(summary["passed"], 2)
            for mode in bench.MODES:
                plan = output / "plans" / "he_053_add" / f"sample_000_{mode}" / "plan.txt"
                self.assertTrue(plan.read_text().strip())
                metadata = json.loads((plan.parent / "plan_metadata.json").read_text())
                self.assertEqual(metadata["status"], "pass")
                self.assertTrue(metadata["local_planner_invoked"])

    def test_saved_plans_skip_the_planner(self):
        commands, plan_commands = [], []

        def fake_execute(command, timeout, **kwargs):
            if "--help" in command and "generate" in command:
                return process(stdout="--force-postfix --plan-only --plan-file")
            planning = "--plan-only" in command or ("--system-file" in command and "--n" not in command)
            if planning:
                plan_commands.append(command)
                return process(stdout="should not plan\n")
            generating = (len(command) > 1 and command[1] == "generate") or "--user-file" in command
            if generating:
                commands.append(command)
                return process(stdout="main(): Int64 { return 0; }")
            return process(stdout="fixture version")

        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            saved = temporary / "saved"
            for mode in bench.MODES:
                directory = saved / "plans" / "he_053_add" / f"sample_000_{mode}"
                directory.mkdir(parents=True)
                (directory / "plan.txt").write_text(f"saved {mode} plan\n")
                (directory / "plan_metadata.json").write_text(json.dumps({"seed": 42}) + "\n")
            model = temporary / "fixture.gguf"
            model.write_bytes(b"model-free fixture, never loaded")
            output = temporary / "results"
            with patch.object(bench, "execute", side_effect=fake_execute), \
                 patch.object(bench, "evaluate", side_effect=lambda *args: dict(
                     compilation_success=True, functional_success=True, status="pass")), \
                 patch.object(bench, "score_translated", side_effect=lambda *args: dict(
                     task_id="he_053_add", translation_success=True, compilation_success=True,
                     harness_compile_success=True, functional_success=True,
                     tests_passed=1, tests_total=1, status="pass")), \
                 patch.object(bench, "evaluate_cangjie", side_effect=lambda *args: dict(
                     task_id="he_053_add", translation_success=None, compilation_success=True,
                     harness_compile_success=True, functional_success=True,
                     tests_passed=1, tests_total=1, status="pass")), \
                 patch.object(bench, "model_lock", return_value=contextlib.nullcontext()), \
                 contextlib.redirect_stdout(io.StringIO()):
                status = bench.main(["--model", str(model), "--cli", sys.executable, "--translator", sys.executable,
                                     "--plain", sys.executable, "--cjc", sys.executable,
                                     "--plans", str(saved), "--output", str(output), "--tasks", "he_053_add"])
            self.assertEqual(status, 0)
            self.assertEqual(plan_commands, [])
            self.assertEqual(len(commands), 2)
            typed = next(command for command in commands if len(command) > 1 and command[1] == "generate")
            self.assertEqual(Path(typed[typed.index("--plan-file") + 1]).read_text(), "saved constrained plan\n")
            cangjie = next(command for command in commands if "--user-file" in command)
            self.assertIn("saved cangjie plan", Path(cangjie[cangjie.index("--user-file") + 1]).read_text())
            config = json.loads((output / "config.json").read_text())
            self.assertEqual(config["plan_source"], "saved")
            self.assertFalse(config["local_planning"])
            rows = [json.loads(line) for line in (output / "results.jsonl").read_text().splitlines()]
            self.assertTrue(all(row["plan_sha256"] for row in rows))
            self.assertTrue(all(row["code_generation_attempted"] for row in rows))

    def test_per_task_plans_are_reused_for_every_sample(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            task = root / "he_053_add"
            task.mkdir()
            (task / "typed.txt").write_text("typed plan\n")
            (task / "cangjie.txt").write_text("cangjie plan\n")
            self.assertTrue(bench.plans_directory_ok(root))
            self.assertFalse(bench.plans_directory_ok(root / "missing"))
            for sample in (0, 3):
                self.assertEqual(bench.saved_plan_file(root, "he_053_add", sample, "constrained").name, "typed.txt")
                self.assertEqual(bench.saved_plan_file(root, "he_053_add", sample, "cangjie").name, "cangjie.txt")
            sample_plan = root / "plans" / "he_053_add" / "sample_001_constrained"
            sample_plan.mkdir(parents=True)
            (sample_plan / "plan.txt").write_text("sample plan\n")
            self.assertEqual(bench.saved_plan_file(root, "he_053_add", 1, "constrained").read_text(), "sample plan\n")
            self.assertEqual(bench.saved_plan_file(root, "he_053_add", 0, "constrained").read_text(), "typed plan\n")

    def test_given_task_plans_skip_the_planner(self):
        commands, plan_commands = [], []

        def fake_execute(command, timeout, **kwargs):
            if "--help" in command and "generate" in command:
                return process(stdout="--force-postfix --plan-only --plan-file")
            planning = "--plan-only" in command or ("--system-file" in command and "--n" not in command)
            if planning:
                plan_commands.append(command)
                return process(stdout="should not plan\n")
            generating = (len(command) > 1 and command[1] == "generate") or "--user-file" in command
            if generating:
                commands.append(command)
                return process(stdout="main(): Int64 { return 0; }")
            return process(stdout="fixture version")

        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            saved = temporary / "given"
            task = saved / "he_053_add"
            task.mkdir(parents=True)
            (task / "typed.txt").write_text("given typed plan\n")
            (task / "cangjie.txt").write_text("given cangjie plan\n")
            model = temporary / "fixture.gguf"
            model.write_bytes(b"model-free fixture, never loaded")
            output = temporary / "results"
            with patch.object(bench, "execute", side_effect=fake_execute), \
                 patch.object(bench, "evaluate", side_effect=lambda *args: dict(
                     compilation_success=True, functional_success=True, status="pass")), \
                 patch.object(bench, "score_translated", side_effect=lambda *args: dict(
                     task_id="he_053_add", translation_success=True, compilation_success=True,
                     harness_compile_success=True, functional_success=True,
                     tests_passed=1, tests_total=1, status="pass")), \
                 patch.object(bench, "evaluate_cangjie", side_effect=lambda *args: dict(
                     task_id="he_053_add", translation_success=None, compilation_success=True,
                     harness_compile_success=True, functional_success=True,
                     tests_passed=1, tests_total=1, status="pass")), \
                 patch.object(bench, "model_lock", return_value=contextlib.nullcontext()), \
                 contextlib.redirect_stdout(io.StringIO()):
                status = bench.main(["--model", str(model), "--cli", sys.executable, "--translator", sys.executable,
                                     "--plain", sys.executable, "--cjc", sys.executable,
                                     "--plans", str(saved), "--samples", "2", "--output", str(output),
                                     "--tasks", "he_053_add"])
            self.assertEqual(status, 0)
            self.assertEqual(plan_commands, [])
            self.assertEqual(len(commands), 4)
            typed = [command for command in commands if len(command) > 1 and command[1] == "generate"]
            self.assertEqual(len(typed), 2)
            self.assertTrue(all(Path(command[command.index("--plan-file") + 1]).read_text() == "given typed plan\n"
                                for command in typed))
            cangjie = [command for command in commands if "--user-file" in command]
            self.assertTrue(all("given cangjie plan" in Path(command[command.index("--user-file") + 1]).read_text()
                                for command in cangjie))
            config = json.loads((output / "config.json").read_text())
            self.assertEqual(config["plan_source"], "saved")
            self.assertFalse(config["local_planning"])

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
            for command in cangjie:
                self.assertNotIn("--skip-plan", command)
                self.assertIn("--n", command)
                user = Path(command[command.index("--user-file") + 1]).read_text()
                self.assertIn("Implementation plan:", user)
                self.assertIn("Implement func", user)
            for task in SUITE["tasks"]:
                self.assertEqual(sum(len(command) > 2 and command[2] == bench.task_prompt(task) for command in typed), 1)
                self.assertEqual(len(seen_plans[bench.task_prompt(task)]), 1)
                expected = str(bench.code_budget(task))
                typed_command = next(command for command in typed if len(command) > 2 and command[2] == bench.task_prompt(task))
                self.assertEqual(typed_command[typed_command.index("--n") + 1], expected)
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
        from prompts import (CANGJIE_SPEC, CODE_PREFIX, PLAN_PREFIX, TYPED_SPEC,
                             code_user, planning_user, problem_statement, system_text,
                             task_text)
        shared = problem_statement(ADD)
        self.assertEqual(task_text(ADD, "constrained"), task_text(ADD, "cangjie"))
        self.assertEqual(code_user(ADD, "constrained"), code_user(ADD, "cangjie"))
        self.assertEqual(planning_user(ADD, "constrained"), planning_user(ADD, "cangjie"))
        self.assertIn(shared, bench.task_prompt(ADD))
        self.assertIn(shared, code_user(ADD, "cangjie"))
        self.assertIn("supplied language", code_user(ADD, "constrained"))
        self.assertTrue(system_text("constrained", False).startswith(CODE_PREFIX))
        self.assertIn(TYPED_SPEC, system_text("constrained", False))
        self.assertIn(TYPED_SPEC, system_text("constrained", True))
        self.assertTrue(system_text("constrained", True).startswith(PLAN_PREFIX))
        self.assertTrue(system_text("cangjie", False).startswith(CODE_PREFIX))
        self.assertTrue(system_text("cangjie", True).startswith(PLAN_PREFIX))
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
        rolling = next(task for task in SUITE["tasks"] if task["id"] == "he_009_rolling_max")
        typed_rolling = code_user(rolling, "constrained")
        cangjie_rolling = code_user(rolling, "cangjie")
        self.assertIn("func solve(numbers: List<Int64>): List<Int64>.", typed_rolling)
        self.assertIn("func solve(numbers: ArrayList<Int64>): ArrayList<Int64>.", cangjie_rolling)
        self.assertNotIn("numbers: List<", cangjie_rolling)
        self.assertEqual(typed_rolling.replace("List<Int64>", "ArrayList<Int64>"), cangjie_rolling)
        planned = code_user(ADD, "cangjie", plan="keep the same seed")
        self.assertIn("Implementation plan:\nkeep the same seed", planned)
        header = (bench.ROOT / "src/language_prompt.h").read_text()
        planning = (bench.ROOT / "src/planning.h").read_text()
        self.assertIn("Write exactly one program satisfying this language specification.", header)
        self.assertIn("Output code only: no Markdown or explanation.", header)
        self.assertNotIn("Use spaces between tokens for clarity.", header)
        self.assertIn("The following specification constrains the program written later.", header)
        self.assertIn("Write signatures only", header)
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

    def test_code_budget_is_twice_the_model_tokens_or_2048(self):
        addition = "func solve(x: Int64, y: Int64): Int64 {\n    let result: Int64 = x + y;\n    return result;\n}\nmain(): Int64 { return 0; }\n"
        self.assertEqual(bench.reference_token_count(addition), bench.reference_token_count(
            (bench.ROOT / "benchmarks/references/he_053_add.typed").read_text()))
        self.assertEqual(bench.reference_token_count('let text: String = "a b";'), 7)
        self.assertEqual(bench.code_budget_for(100), 2048)
        self.assertEqual(bench.code_budget_for(1024), 2048)
        self.assertEqual(bench.code_budget_for(1025), 2050)
        task = copy.deepcopy(ADD)
        with patch.object(bench, "model_token_counts", return_value=[1500]):
            bench.assign_code_budgets([task], Path("model.gguf"), bench.ROOT / "benchmarks")
        self.assertEqual(task["model_tokens"], 1500)
        self.assertEqual(task["code_tokens"], 3000)
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for mode in bench.MODES:
                command = bench.generation_command(settings(), task, mode, 42, directory)
                self.assertEqual(command[command.index("--n") + 1], "3000")

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

    def test_report_rows_use_booleans_and_case_counts(self):
        records = [
            dict(mode="constrained", task_id="one", sample_index=0,
                 translation_success=True, compilation_success=True,
                 functional_success=False, tests_passed=9, tests_total=11,
                 status="wrong_answer", code_generation_attempted=True,
                 generation_success=True),
            dict(mode="cangjie", task_id="one", sample_index=0,
                 translation_success=None, compilation_success=False,
                 functional_success=False, tests_passed=0, tests_total=11,
                 status="compile_failed", code_generation_attempted=True,
                 generation_success=True),
        ]
        config = {"suite_name": "suite", "plan_source": "none", "task_ids": ["one"], "samples": 1,
                  "code_tokens_by_task": {"one": 2048}}
        with tempfile.TemporaryDirectory() as temporary:
            bench.write_report(Path(temporary), config, records)
            text = (Path(temporary) / "report.md").read_text()
        self.assertIn("| one | 2048 | true | true | 9/11 | true | false | 0/11 |", text)
        self.assertIn("| constrained | 1 | 1/1 | 1/1 | 9/11 |", text)
        self.assertIn("| cangjie | 1 | 1/1 | 0/1 | 0/11 |", text)

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
        stuck = process(-6)
        stuck.stderr = "typed sampler: no viable candidate in vocabulary\n"
        # Soft-fail exit from the typed sampler should not abort the suite.
        self.assertFalse(bench.generation_error(stuck))
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
