#include "planning.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

int main() {
    std::ifstream specification_file(LANGUAGE_SPEC_SOURCE);
    CHECK(specification_file.good());
    const std::string specification_text(std::istreambuf_iterator<char>{specification_file}, {});
    CHECK(language::specification == specification_text);
    CHECK(language::system_prompt().find(specification_text) != std::string::npos);
    CHECK(language::planning_prompt().find(specification_text) != std::string::npos);

    const std::string task = "Compute factorial(5) using a helper.";
    const std::string plan = "Expected: 120. factorial(n: Int64): Int64 multiplies 1 through n and returns the product. "
                             "main binds factorial(5) and returns the result.";
    int calls = 0;
    const auto prepared = planning::prepare(task, "", true, [&](const planning::Request & request) {
        ++calls;
        CHECK(request.user.find(task) != std::string::npos);
        CHECK(request.system.find(language::specification) != std::string::npos);
        CHECK(request.system.find("For each function, including main") != std::string::npos);
        CHECK(request.system.find("loop exit conditions") != std::string::npos);
        CHECK(request.system.find("base case") != std::string::npos);
        CHECK(request.system.find("Output code only") == std::string::npos);
        return planning::Plan{ plan, true };
    });
    CHECK(calls == 1);
    CHECK(prepared.plan && prepared.plan->complete);
    CHECK(prepared.system == language::system_prompt());
    CHECK(prepared.user.find(task) != std::string::npos);
    CHECK(prepared.user.find(plan) != std::string::npos);
    CHECK(prepared.user.find("Correct any conflict") != std::string::npos);
    CHECK(prepared.user.find("Output code only") != std::string::npos);

    const auto direct = planning::prepare(task, "", false, [&](const planning::Request &) -> planning::Plan {
        CHECK(false);
        return {};
    });
    CHECK(!direct.plan);
    CHECK(direct.user == "Produce a program for this request: " + task);

    const std::string prefix = "func factorial(n: Int64): Int64 { ";
    const auto continued = planning::prepare(task, prefix, true, [&](const planning::Request & request) {
        CHECK(request.user.find(prefix) != std::string::npos);
        return planning::Plan{ plan, true };
    });
    CHECK(continued.plan && continued.plan->complete);

    const std::string long_plan = std::string(20000, 'x') + "END OF COMPLETE PLAN";
    const auto long_prepared = planning::prepare(task, "", true, [&](const planning::Request &) {
        return planning::Plan{ long_plan, true };
    });
    CHECK(long_prepared.plan->text == long_plan);
    CHECK(long_prepared.user.find(long_plan) != std::string::npos);
    CHECK(long_prepared.user.find("truncated") == std::string::npos);

    bool incomplete_failed = false;
    try {
        planning::prepare(task, "", true, [](const planning::Request &) {
            return planning::Plan{ "An interrupted plan", false };
        });
    } catch (const std::runtime_error &) {
        incomplete_failed = true;
    }
    CHECK(incomplete_failed);

    // Repeated context recycling must retain the request and newest token,
    // without limiting the separately accumulated plan.
    const std::vector<int> prompt_tokens{ -4, -3, -2, -1 };
    std::vector<int> history, complete_output;
    size_t position = prompt_tokens.size();
    size_t replays = 0;
    for (int token = 0; token < 10000; ++token) {
        history.push_back(token);
        complete_output.push_back(token);
        if (position >= 16) {
            const auto replay = planning::replay_tokens(prompt_tokens, history, 16);
            CHECK(std::vector<int>(replay.begin(), replay.begin() + 4) == prompt_tokens);
            CHECK(replay.back() == token);
            CHECK(replay.size() < 16);
            position = replay.size();
            ++replays;
        } else ++position;
    }
    CHECK(replays > 1000);
    CHECK(complete_output.size() == 10000 && complete_output.back() == 9999);
    bool invalid_window = false;
    try {
        planning::replay_tokens(prompt_tokens, history, 5);
    } catch (const std::runtime_error &) { invalid_window = true; }
    CHECK(invalid_window);

    for (const auto & result : { "", " \n\t\r" }) {
        bool failed = false;
        try {
            planning::prepare(task, "", true, [&](const planning::Request &) {
                return planning::Plan{ result, true };
            });
        } catch (const std::runtime_error & error) {
            failed = true;
            CHECK(std::string(error.what()).find("no implementation plan") != std::string::npos);
        }
        CHECK(failed);
    }
    bool failed = false;
    try {
        planning::prepare(task, "", true, [](const planning::Request &) -> planning::Plan {
            throw std::runtime_error("planner failed");
        });
    } catch (const std::runtime_error & error) {
        failed = true;
        CHECK(std::string(error.what()) == "planner failed");
    }
    CHECK(failed);
    std::puts("Planning tests passed without model inference");
}
