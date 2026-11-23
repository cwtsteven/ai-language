#include "model_plugin.h"
#include "llama.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace models {
namespace {

std::string render(const std::string & text, const std::string & system, const std::string & user) {
    std::string result;
    for (size_t i = 0; i < text.size();) {
        if (text.compare(i, 10, "{{system}}") == 0) {
            result += system;
            i += 10;
        } else if (text.compare(i, 8, "{{task}}") == 0) {
            result += user;
            i += 8;
        } else {
            result += text[i++];
        }
    }
    return result;
}

} // namespace

Plugin load_plugin(const std::string & path) {
    try {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("cannot open file");
        const auto json = nlohmann::json::parse(input);
        if (!json.is_object()) throw std::runtime_error("expected a JSON object");
        Plugin plugin;
        plugin.name = std::filesystem::path(path).stem().string();
        for (const auto & item : json.items()) {
            const auto & key = item.key();
            const auto & value = item.value();
            if (key == "name" || key == "model" || key == "chat_template" ||
                key == "prompt_template" || key == "assistant_prefix") {
                if (!value.is_string()) throw std::runtime_error(key + " must be a string");
                const auto text = value.get<std::string>();
                if (key != "assistant_prefix" && text.empty())
                    throw std::runtime_error(key + " must not be empty");
                if (text.find('\0') != std::string::npos)
                    throw std::runtime_error(key + " must not contain NUL bytes");
                if (key == "name") plugin.name = text;
                else if (key == "model") {
                    auto model = std::filesystem::path(text);
                    if (model.is_relative()) model = std::filesystem::absolute(path).parent_path() / model;
                    plugin.model_path = model.lexically_normal().string();
                } else if (key == "chat_template") plugin.chat_template = text;
                else if (key == "prompt_template") plugin.prompt_template = text;
                else plugin.assistant_prefix = text;
            } else if (key == "gpu_layers" || key == "ubatch") {
                if (!value.is_number_integer() || value < 0 ||
                    value > std::numeric_limits<int>::max() || (key == "ubatch" && value == 0))
                    throw std::runtime_error(key + " must be an integer " +
                                             (key == "ubatch" ? "> 0" : ">= 0"));
                if (key == "gpu_layers") plugin.gpu_layers = value.get<int>();
                else plugin.ubatch = value.get<int>();
            } else {
                throw std::runtime_error("unknown field: " + key);
            }
        }
        if (plugin.chat_template && plugin.prompt_template)
            throw std::runtime_error("choose chat_template or prompt_template, not both");
        if (plugin.prompt_template &&
            (plugin.prompt_template->find("{{system}}") == std::string::npos ||
             plugin.prompt_template->find("{{task}}") == std::string::npos))
            throw std::runtime_error("prompt_template must contain {{system}} and {{task}}");
        return plugin;
    } catch (const std::exception & error) {
        throw std::runtime_error("model plugin " + path + ": " + error.what());
    }
}

std::string format_prompt(const Plugin & plugin, const char * embedded_template,
                         std::string_view architecture, const std::string & system,
                         const std::string & user) {
    std::string formatted;
    if (plugin.prompt_template) {
        formatted = render(*plugin.prompt_template, system, user);
    } else {
        const char * tmpl = plugin.chat_template ? plugin.chat_template->c_str() : embedded_template;
        if (!tmpl) throw std::runtime_error(
            "model has no chat template; supply --model-plugin with a prompt_template or use --prompt");
        const llama_chat_message messages[] = {
            { "system", system.c_str() }, { "user", user.c_str() },
        };
        const int32_t length = llama_chat_apply_template(tmpl, messages, 2, true, nullptr, 0);
        if (length <= 0) throw std::runtime_error(
            "unsupported chat template; supply --model-plugin with a prompt_template or use --prompt");
        formatted.resize(static_cast<size_t>(length));
        if (llama_chat_apply_template(tmpl, messages, 2, true, formatted.data(), length) != length)
            throw std::runtime_error("could not format the model's chat template");
    }
    if (plugin.assistant_prefix) {
        formatted += *plugin.assistant_prefix;
    } else if (!plugin.prompt_template) {
        // These adapters enter the code answer before applying type constraints.
        if (architecture == "qwen35") formatted += "<think>\n\n</think>\n\n";
        else if (architecture == "gpt-oss") formatted += "<|channel|>final<|message|>";
    }
    return formatted;
}

std::string task_prompt(const llama_model * model, const Plugin & plugin,
                        const std::string & system, const std::string & user) {
    char architecture[128] = {};
    llama_model_meta_val_str(model, "general.architecture", architecture, sizeof(architecture));
    return format_prompt(plugin, llama_model_chat_template(model, nullptr), architecture, system, user);
}

} // namespace models
