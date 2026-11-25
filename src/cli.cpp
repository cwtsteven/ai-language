#include "cli.h"

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace cli {
namespace {

int number(const std::string & option, const std::string & value, int minimum) {
    int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < minimum)
        throw std::runtime_error(option + " requires " + (minimum ? "a positive" : "a nonnegative") +
                                 " integer within the 32-bit signed range");
    return result;
}

float temperature(const std::string & value) {
    try {
        size_t consumed = 0;
        const float result = std::stof(value, &consumed);
        if (consumed == value.size() && value.find_first_of(" \t\r\n") == std::string::npos &&
            std::isfinite(result) && result >= 0) return result;
    } catch (const std::exception &) {}
    throw std::runtime_error("--temperature requires a finite nonnegative number (0 selects greedy)");
}

uint32_t seed(const std::string & value) {
    uint32_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::runtime_error("--seed requires an integer from 0 to 4294967295");
    return result;
}

} // namespace

Options parse(int argc, const char * const * argv) {
    Options options;
    if (argc < 2 || std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h") {
        options.help = true;
        return options;
    }
    const std::string command = argv[1];
    if (command == "generate") options.command = Command::Generate;
    else if (command == "check") options.command = Command::Check;
    else if (command == "complete") options.command = Command::Complete;
    else if (command == "format") options.command = Command::Format;
    else throw std::runtime_error("unknown command: " + command);

    bool input_set = false;
    bool literal = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (!literal && (arg == "--help" || arg == "-h")) {
            options.help = true;
            return options;
        }
        if (!literal && arg == "--") { literal = true; continue; }
        if (!literal && arg.size() > 1 && arg.front() == '-') {
            if (options.command != Command::Generate)
                throw std::runtime_error(command + " does not accept option " + arg);
            if (arg == "--unconstrained") { options.constrained = false; continue; }
            if (arg == "--skip-plan") { options.plan = false; continue; }
            if (arg == "--plan-only") { options.plan_only = true; continue; }
            if (arg == "--force-postfix") { options.force_postfix = true; continue; }
            if (arg == "--debug") { options.debug = true; continue; }
            if (arg != "--model" && arg != "--n" && arg != "--debug-limit" && arg != "--ngl" &&
                arg != "--ubatch" && arg != "--threads" && arg != "--temperature" && arg != "--seed" && arg != "--plan-file")
                throw std::runtime_error("unknown generate option: " + arg);
            if (i + 1 >= argc) throw std::runtime_error(arg + " requires a value");
            const std::string value = argv[++i];
            if (arg == "--model") options.model_path = value;
            else if (arg == "--plan-file") {
                if (value.empty()) throw std::runtime_error("--plan-file must not be empty");
                options.plan_file = value;
            }
            else if (arg == "--n") options.tokens = number(arg, value, 1);
            else if (arg == "--temperature") options.temperature = temperature(value);
            else if (arg == "--seed") options.seed = seed(value);
            else if (arg == "--ubatch") options.ubatch = number(arg, value, 1);
            else if (arg == "--threads") options.threads = number(arg, value, 1);
            else if (arg == "--ngl") options.gpu_layers = number(arg, value, 0);
            else if (arg == "--debug-limit") {
                options.debug_limit = number(arg, value, 0);
                options.debug = true;
            }
        } else {
            if (input_set) throw std::runtime_error(command + " accepts one quoted text argument");
            options.input = arg;
            input_set = true;
        }
    }
    if (options.command == Command::Generate) {
        if (options.model_path.empty()) throw std::runtime_error("generate requires --model FILE");
        if (options.plan_only && !options.plan_file.empty())
            throw std::runtime_error("--plan-only and --plan-file cannot be combined");
        if ((options.plan_only || !options.plan_file.empty()) && !options.plan)
            throw std::runtime_error("--plan-only/--plan-file cannot combine with --skip-plan");
        if (!input_set || options.input.empty())
            throw std::runtime_error("generate requires TEXT");
    } else if (!input_set) {
        if (options.command == Command::Format) options.input = "-";
        else throw std::runtime_error(command + " requires CODE or '-' to read stdin");
    }
    return options;
}

void print_help(FILE * output, const char * executable, Command command) {
    if (command == Command::Help) {
        std::fprintf(output,
            "usage: %s COMMAND [OPTIONS]\n\n"
            "Commands:\n"
            "  generate  Generate a Cangjie program using a supplied local model.\n"
            "  check     Check typed code or a prefix without a model.\n"
            "  complete  Complete a valid typed prefix without a model.\n"
            "  format    Format existing typed code without a model.\n\n"
            "Use '%s COMMAND --help' for that command's arguments and options.\n",
            executable, executable);
    } else if (command == Command::Generate) {
        std::fprintf(output,
            "usage: %s generate TEXT --model FILE [OPTIONS]\n\n"
            "Arguments:\n"
            "  TEXT              A quoted description of the program to generate.\n\n"
            "Options:\n"
            "  --model FILE      Required: GGUF weights or a JSON model plugin.\n"
            "  --n N             Maximum generated code tokens (default 256).\n"
            "  --temperature T   Sampling randomness for planning and code generation (default 0.8; 0 = greedy).\n"
            "  --seed N          Reuse a sampling seed (0..4294967295; default random, printed to stderr).\n"
            "  --skip-plan       Skip the default unconstrained implementation-planning pass.\n"
            "  --plan-only       Write only the implementation plan to stdout; do not generate code.\n"
            "  --plan-file FILE  Generate code using this saved plan; do not run the planner.\n"
            "  --force-postfix   If the token budget ends an open program, append a typed postfix before translation.\n"
            "  --unconstrained   Disable token constraints and translate the result when it is a complete typed program.\n"
            "  --debug           Show the plan, token traces, and typed program on stderr.\n"
            "  --debug-limit N   Rejected candidates shown per code token (default 20); enables debug mode.\n"
            "  --threads N       CPU threads for prompt processing and generation (default 4).\n"
            "  --ngl N           GPU layers to offload (default 99 on macOS, 0 elsewhere).\n"
            "  --ubatch N        Physical prompt batch size (default 512).\n"
            "  -h, --help        Show this command's help without loading a model.\n\n"
            "The description first produces a plan (shown only in debug mode), without an output token cap,\n"
            "then include it with the language specification for code generation. Sampling is type-constrained.\n"
            "A complete typed program is translated to Cangjie and written to stdout. An unfinished program\n"
            "writes the typed prefix on stdout (exit 2), prints it under 'Typed program:' on stderr, and writes no Cangjie.\n"
            "A token budget is not a word count. If it runs out before the program is complete, nothing is translated.\n"
            "--force-postfix appends a typed postfix that closes the program, then translates it, and reports that on stderr.\n"
            "That postfix is constructed, so it may not fulfill the request. Store memory settings in a model plugin for routine use.\n",
            executable);
    } else {
        const char * name = command == Command::Check ? "check" : command == Command::Complete ? "complete" : "format";
        std::fprintf(output, "usage: %s %s %s\n\n", executable, name,
                     command == Command::Format ? "[CODE|-]" : "CODE|-");
        if (command == Command::Check) std::fputs(
            "Check prefix viability and whether the program is complete.\n"
            "A viable unfinished prefix exits 0; an invalid prefix exits 2 with a\n"
            "compiler-style error on stderr (location, message, and source caret).\n", output);
        else if (command == Command::Complete) std::fputs(
            "Print a valid prefix plus a deterministically constructed suffix.\n"
            "Invalid prefixes exit 2. Completion does not prove algorithm correctness.\n", output);
        else std::fputs(
            "Pretty-print a complete, well-typed program. Omit CODE to read stdin.\n"
            "Invalid or unfinished input exits 2 without printing formatted code.\n", output);
        std::fputs(
            "\nArguments:\n"
            "  CODE        Quoted program text (or prefix, for check/complete).\n"
            "  -           Read code from stdin. No model is needed.\n\n"
            "Options:\n"
            "  -h, --help  Show this command's help.\n"
            "  --          Treat the remaining argument as literal text.\n", output);
    }
}

} // namespace cli
