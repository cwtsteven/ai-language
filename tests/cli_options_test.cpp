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
    for (auto command : {"generate", "check", "complete", "format"}) {
        CHECK(parse({command, "--help"}).help);
        CHECK(parse({command, "-h"}).help);
    }
    CHECK(parse({"generate", "request", "--help"}).command == cli::Command::Generate);
    auto options = parse({"generate", "Compute Fibonacci(5)", "--model", "weights.gguf"});
    CHECK(options.command == cli::Command::Generate);
    CHECK(options.input == "Compute Fibonacci(5)");
    CHECK(options.model_path == "weights.gguf");
    CHECK(options.tokens == 256 && options.constrained && options.plan);
    CHECK(!options.force_postfix);
    CHECK(options.temperature == 0.8f && !options.seed);
    CHECK(!options.gpu_layers && options.ubatch == 0 && options.threads == 4);
    options = parse({"generate", "--model", "plugin.json", "--n", "512", "request"});
    CHECK(options.model_path == "plugin.json" && options.tokens == 512);
    CHECK(parse({"generate", "request", "--model", "x", "--n", "32"}).tokens == 32);
    CHECK(!parse({"generate", "request", "--model", "x", "--skip-plan"}).plan);
    CHECK(parse({"generate", "request", "--model", "x", "--force-postfix"}).force_postfix);
    CHECK(parse({"generate", "request", "--model", "x", "--plan-only"}).plan_only);
    CHECK(parse({"generate", "request", "--model", "x", "--plan-file", "saved plan.txt"}).plan_file == "saved plan.txt");
    options = parse({"generate", "request", "--model", "x", "--temperature", "0.6", "--seed", "42"});
    CHECK(options.temperature == 0.6f && options.seed && *options.seed == 42);
    CHECK(parse({"generate", "request", "--model", "x", "--temperature", "0"}).temperature == 0);
    CHECK(*parse({"generate", "request", "--model", "x", "--seed", "4294967295"}).seed == 4294967295u);
    CHECK(*parse({"generate", "request", "--model", "x", "--seed", "0"}).seed == 0);
    options = parse({"generate", "request", "--model", "x",
                     "--unconstrained", "--debug-limit", "0", "--ngl", "0", "--ubatch", "64"});
    CHECK(options.input == "request");
    CHECK(!options.constrained && options.debug && options.debug_limit == 0);
    CHECK(options.gpu_layers && *options.gpu_layers == 0 && options.ubatch == 64);
    CHECK(parse({"generate", "request", "--model", "x", "--threads", "24"}).threads == 24);
    CHECK(parse({"generate", "request", "--model", "x", "--debug"}).debug);
    CHECK(parse({"generate", "--model", "x", "--", "--help"}).input == "--help");
    CHECK(parse({"check", ""}).input.empty());
    CHECK(parse({"complete", ""}).command == cli::Command::Complete);
    CHECK(parse({"check", "-"}).input == "-");
    CHECK(parse({"format"}).input == "-");
    CHECK(parse({"format", "--", "--help"}).input == "--help");

    fails({"generate", "request", "--model", "x", "-n", "512"}, "unknown generate option");
    fails({"--task", "request"}, "unknown command");
    fails({"--check", "code"}, "unknown command");
    fails({"generate", "request"}, "requires --model");
    fails({"generate", "request", "--model", ""}, "requires --model");
    fails({"generate", "--model", "x"}, "requires TEXT");
    fails({"generate", "", "--model", "x"}, "requires TEXT");
    fails({"generate", "request", "--model", "x", "--prompt", "raw"}, "unknown generate option");
    fails({"generate", "request", "--model", "x", "--prefix", "main"}, "unknown generate option");
    fails({"generate", "request", "--model", "x", "--no-complete"}, "unknown generate option");
    fails({"generate", "request", "--model"}, "requires a value");
    fails({"generate", "request", "--model", "x", "--n"}, "requires a value");
    fails({"generate", "request", "--model", "x", "--n", "0"}, "positive integer");
    fails({"generate", "request", "--model", "x", "--n", "-1"}, "positive integer");
    fails({"generate", "request", "--model", "x", "--n", "12oops"}, "integer");
    fails({"generate", "request", "--model", "x", "--n", "2147483648"}, "integer");
    fails({"generate", "request", "--model", "x", "--ubatch", "0"}, "positive integer");
    fails({"generate", "request", "--model", "x", "--threads", "0"}, "positive integer");
    fails({"generate", "request", "--model", "x", "--ngl", "-1"}, "nonnegative integer");
    fails({"generate", "request", "--model", "x", "--debug-limit", "-1"}, "nonnegative integer");
    for (auto value : {"-0.1", "nan", "inf", "1e100", "1e-100", "0.8oops", "", " 0.8"})
        fails({"generate", "request", "--model", "x", "--temperature", value}, "finite nonnegative");
    for (auto value : {"-1", "4294967296", "42oops", "", "1.5"})
        fails({"generate", "request", "--model", "x", "--seed", value}, "integer from 0");
    fails({"generate", "request", "--model", "x", "--temperature"}, "requires a value");
    fails({"generate", "request", "--model", "x", "--seed"}, "requires a value");
    fails({"generate", "request", "--model", "x", "--plan-file"}, "requires a value");
    fails({"generate", "request", "--model", "x", "--plan-file", ""}, "must not be empty");
    fails({"generate", "request", "--model", "x", "--plan-only", "--skip-plan"}, "cannot combine");
    fails({"generate", "request", "--model", "x", "--plan-file", "p", "--skip-plan"}, "cannot combine");
    fails({"generate", "request", "--model", "x", "--plan-only", "--plan-file", "p"}, "cannot be combined");
    fails({"generate", "request", "--unknown"}, "unknown generate option");
    fails({"check"}, "requires CODE");
    fails({"complete"}, "requires CODE");
    fails({"check", "code", "--model", "x"}, "does not accept option");
    fails({"format", "code", "--format"}, "does not accept option");
    fails({"cj", "main(): Int64 { return 0; }"}, "unknown command");
    fails({"generate", "request", "--model", "x", "--format"}, "unknown generate option");
    fails({"check", "code", "complete", "code"}, "one quoted text");
    std::puts("CLI argument tests passed");
}
