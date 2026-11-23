#include "cli.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

static cli::Options parse(std::initializer_list<const char *> arguments) {
    std::vector<const char *> argv{ "typed-cangjie" };
    argv.insert(argv.end(), arguments);
    return cli::parse(static_cast<int>(argv.size()), argv.data());
}

static void fails(std::initializer_list<const char *> arguments, const std::string & reason) {
    try {
        parse(arguments);
    } catch (const std::runtime_error & error) {
        CHECK(std::string(error.what()).find(reason) != std::string::npos);
        return;
    }
    CHECK(false);
}

int main() {
    CHECK(parse({}).help);
    CHECK(parse({"--help"}).command == cli::Command::Help);
    for (auto command : {"task", "check", "complete", "format"}) {
        CHECK(parse({command, "--help"}).help);
        CHECK(parse({command, "-h"}).help);
    }
    CHECK(parse({"task", "request", "--help"}).command == cli::Command::Task);
    auto options = parse({"task", "Compute Fibonacci(5)", "--model", "weights.gguf"});
    CHECK(options.command == cli::Command::Task);
    CHECK(options.input == "Compute Fibonacci(5)");
    CHECK(options.model_path == "weights.gguf");
    CHECK(options.tokens == 256 && options.constrained && options.plan);
    CHECK(!options.force_postfix);
    CHECK(options.temperature == 0.8f && !options.seed);
    CHECK(!options.gpu_layers && options.ubatch == 0);
    options = parse({"task", "--model", "plugin.json", "--n", "512", "request"});
    CHECK(options.model_path == "plugin.json" && options.tokens == 512);
    CHECK(parse({"task", "request", "--model", "x", "--n", "32"}).tokens == 32);
    CHECK(!parse({"task", "request", "--model", "x", "--skip-plan"}).plan);
    CHECK(parse({"task", "request", "--model", "x", "--force-postfix"}).force_postfix);
    CHECK(parse({"task", "request", "--model", "x", "--plan-only"}).plan_only);
    CHECK(parse({"task", "request", "--model", "x", "--plan-file", "saved plan.txt"}).plan_file == "saved plan.txt");
    options = parse({"task", "request", "--model", "x", "--temperature", "0.6", "--seed", "42"});
    CHECK(options.temperature == 0.6f && options.seed && *options.seed == 42);
    CHECK(parse({"task", "request", "--model", "x", "--temperature", "0"}).temperature == 0);
    CHECK(*parse({"task", "request", "--model", "x", "--seed", "4294967295"}).seed == 4294967295u);
    CHECK(*parse({"task", "request", "--model", "x", "--seed", "0"}).seed == 0);
    options = parse({"task", "request", "--model", "x",
                     "--unconstrained", "--debug-limit", "0", "--ngl", "0", "--ubatch", "64"});
    CHECK(options.input == "request");
    CHECK(!options.constrained && options.debug && options.debug_limit == 0);
    CHECK(options.gpu_layers && *options.gpu_layers == 0 && options.ubatch == 64);
    CHECK(parse({"task", "request", "--model", "x", "--debug"}).debug);
    CHECK(parse({"task", "--model", "x", "--", "--help"}).input == "--help");
    CHECK(parse({"check", ""}).input.empty());
    CHECK(parse({"complete", ""}).command == cli::Command::Complete);
    CHECK(parse({"check", "-"}).input == "-");
    CHECK(parse({"format"}).input == "-");
    CHECK(parse({"format", "--", "--help"}).input == "--help");

    fails({"task", "request", "--model", "x", "-n", "512"}, "unknown task option");
    fails({"--task", "request"}, "unknown command");
    fails({"--check", "code"}, "unknown command");
    fails({"task", "request"}, "requires --model");
    fails({"task", "request", "--model", ""}, "requires --model");
    fails({"task", "--model", "x"}, "requires TEXT");
    fails({"task", "", "--model", "x"}, "requires TEXT");
    fails({"task", "request", "--model", "x", "--prompt", "raw"}, "unknown task option");
    fails({"task", "request", "--model", "x", "--prefix", "main"}, "unknown task option");
    fails({"task", "request", "--model", "x", "--no-complete"}, "unknown task option");
    fails({"task", "request", "--model"}, "requires a value");
    fails({"task", "request", "--model", "x", "--n"}, "requires a value");
    fails({"task", "request", "--model", "x", "--n", "0"}, "positive integer");
    fails({"task", "request", "--model", "x", "--n", "-1"}, "positive integer");
    fails({"task", "request", "--model", "x", "--n", "12oops"}, "integer");
    fails({"task", "request", "--model", "x", "--n", "2147483648"}, "integer");
    fails({"task", "request", "--model", "x", "--ubatch", "0"}, "positive integer");
    fails({"task", "request", "--model", "x", "--ngl", "-1"}, "nonnegative integer");
    fails({"task", "request", "--model", "x", "--debug-limit", "-1"}, "nonnegative integer");
    for (auto value : {"-0.1", "nan", "inf", "1e100", "1e-100", "0.8oops", "", " 0.8"})
        fails({"task", "request", "--model", "x", "--temperature", value}, "finite nonnegative");
    for (auto value : {"-1", "4294967296", "42oops", "", "1.5"})
        fails({"task", "request", "--model", "x", "--seed", value}, "integer from 0");
    fails({"task", "request", "--model", "x", "--temperature"}, "requires a value");
    fails({"task", "request", "--model", "x", "--seed"}, "requires a value");
    fails({"task", "request", "--model", "x", "--plan-file"}, "requires a value");
    fails({"task", "request", "--model", "x", "--plan-file", ""}, "must not be empty");
    fails({"task", "request", "--model", "x", "--plan-only", "--skip-plan"}, "cannot combine");
    fails({"task", "request", "--model", "x", "--plan-file", "p", "--skip-plan"}, "cannot combine");
    fails({"task", "request", "--model", "x", "--plan-only", "--plan-file", "p"}, "cannot be combined");
    fails({"task", "request", "--unknown"}, "unknown task option");
    fails({"check"}, "requires CODE");
    fails({"complete"}, "requires CODE");
    fails({"check", "code", "--model", "x"}, "does not accept option");
    fails({"format", "code", "--format"}, "does not accept option");
    fails({"cj", "main(): Int64 { return 0; }"}, "unknown command");
    fails({"task", "request", "--model", "x", "--format"}, "unknown task option");
    fails({"check", "code", "complete", "code"}, "one quoted text");
    std::puts("CLI argument tests passed");
}
