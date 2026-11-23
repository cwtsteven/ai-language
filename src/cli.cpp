#include "cli.h"

#include <charconv>
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

} // namespace

Options parse(int argc, const char * const * argv) {
    Options options;
    if (argc < 2 || std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h") {
        options.help = true;
        return options;
    }
    const std::string command = argv[1];
    if (command == "task") options.command = Command::Task;
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
            if (options.command != Command::Task)
                throw std::runtime_error(command + " does not accept option " + arg);
            if (arg == "--raw") { options.raw = true; continue; }
            if (arg == "--unconstrained") { options.constrained = false; continue; }
            if (arg == "--debug") { options.debug = true; continue; }
            if (arg != "--model" && arg != "--n" && arg != "--prefix" &&
                arg != "--prompt" && arg != "--debug-limit" && arg != "--ngl" && arg != "--ubatch")
                throw std::runtime_error("unknown task option: " + arg);
            if (i + 1 >= argc) throw std::runtime_error(arg + " requires a value");
            const std::string value = argv[++i];
            if (arg == "--model") options.model_path = value;
            else if (arg == "--prefix") options.prefix = value;
            else if (arg == "--prompt") options.prompt = value;
            else if (arg == "--n") options.tokens = number(arg, value, 1);
            else if (arg == "--ubatch") options.ubatch = number(arg, value, 1);
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
    if (options.command == Command::Task) {
        if (options.model_path.empty()) throw std::runtime_error("task requires --model FILE");
        if (input_set && options.prompt) throw std::runtime_error("task accepts TEXT or --prompt, not both");
        if (!options.prompt && (!input_set || options.input.empty()))
            throw std::runtime_error("task requires TEXT or --prompt TEXT");
        if (options.prompt && options.prompt->empty()) throw std::runtime_error("--prompt must not be empty");
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
            "  task      Generate a program using a supplied local model.\n"
            "  check     Check code or a prefix without a model.\n"
            "  complete  Complete a valid prefix without a model.\n"
            "  format    Format existing code without a model.\n\n"
            "Use '%s COMMAND --help' for that command's arguments and options.\n",
            executable, executable);
    } else if (command == Command::Task) {
        std::fprintf(output,
            "usage: %s task TEXT --model FILE [OPTIONS]\n"
            "       %s task --prompt TEXT --model FILE [OPTIONS]\n\n"
            "Arguments:\n"
            "  TEXT              A quoted description of the program to generate.\n\n"
            "Options:\n"
            "  --model FILE      Required: GGUF weights or a JSON model plugin.\n"
            "  --n N             Maximum generated model tokens (default 256).\n"
            "  --prefix CODE     Seed generation with a valid program prefix.\n"
            "  --prompt TEXT     Use a prepared raw prompt instead of a task description.\n"
            "  --raw             Stream unformatted output.\n"
            "  --unconstrained   Disable token constraints and check the result.\n"
            "  --debug           Trace selected and rejected tokens to stderr.\n"
            "  --debug-limit N   Rejected candidates shown per step (default 20); enables tracing.\n"
            "  --ngl N           Override GPU layers (default: plugin, then 99 on macOS/0 elsewhere).\n"
            "  --ubatch N        Override physical batch size (default: plugin, then llama.cpp).\n"
            "  -h, --help        Show this command's help without loading a model.\n\n"
            "Task descriptions include the grammar prompt. Output is type-constrained and\n"
            "formatted by default. A token budget is not a word count; if it runs out,\n"
            "constrained generation constructs a typed suffix and reports this on stderr.\n"
            "The suffix may not fulfill the request. Use --unconstrained --raw to inspect\n"
            "invalid model output. Store memory settings in a model plugin for routine use.\n",
            executable, executable);
    } else {
        const char * name = command == Command::Check ? "check" : command == Command::Complete ? "complete" : "format";
        std::fprintf(output, "usage: %s %s %s\n\n", executable, name,
                     command == Command::Format ? "[CODE|-]" : "CODE|-");
        if (command == Command::Check) std::fputs(
            "Check prefix viability and whether the program is complete.\n"
            "A viable unfinished prefix exits 0; an invalid prefix exits 2.\n", output);
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
