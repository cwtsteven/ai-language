#pragma once

#include <optional>
#include <string>
#include <string_view>

struct llama_model;

namespace models {

// A data-only plugin: no executable code is loaded from a model descriptor.
struct Plugin {
    std::string name;
    std::string model_path;
    std::optional<std::string> chat_template;
    std::optional<std::string> prompt_template;
    std::optional<std::string> assistant_prefix;
    std::optional<int> gpu_layers;
    std::optional<int> ubatch;
};

// Relative model paths are resolved against the descriptor's directory.
// Loading and formatting throw std::runtime_error with a user-facing reason.
Plugin load_plugin(const std::string & path);
std::string format_prompt(const Plugin & plugin, const char * embedded_template,
                         std::string_view architecture, const std::string & system,
                         const std::string & user);
std::string task_prompt(const llama_model * model, const Plugin & plugin,
                        const std::string & system, const std::string & user);

} // namespace models
