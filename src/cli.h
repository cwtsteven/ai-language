#pragma once

#include <cstdio>
#include <cstdint>
#include <optional>
#include <string>

namespace cli {

enum class Command { Help, Task, Check, Complete, Format };

struct Options {
    Command command = Command::Help;
    bool help = false;
    std::string input;
    std::string model_path;
    std::string prefix;
    std::optional<std::string> prompt;
    int tokens = 256;
    float temperature = 0.8f;
    std::optional<uint32_t> seed;
    int ubatch = 0;
    std::optional<int> gpu_layers;
    bool raw = false;
    bool constrained = true;
    bool plan = true;
    bool debug = false;
    int debug_limit = 20;
};

Options parse(int argc, const char * const * argv);
void print_help(FILE * output, const char * executable, Command command);

} // namespace cli
