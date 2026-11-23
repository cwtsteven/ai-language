#include "model_plugin.h"
#include "language_prompt.h"
#include "typed_prefix.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

template <typename Action>
static void fails(Action action, const std::string & reason) {
    try {
        action();
    } catch (const std::runtime_error & error) {
        CHECK(std::string(error.what()).find(reason) != std::string::npos);
        return;
    }
    CHECK(false);
}

int main() {
    using models::format_prompt;
    fails([] { models::select_model(""); }, "requires --model");
    CHECK(models::select_model("weights/my-model.gguf").model_path == "weights/my-model.gguf");
    models::Plugin plugin;
    const std::string chatml = "<|im_start|>system\nrules<|im_end|>\n"
                               "<|im_start|>user\nrequest<|im_end|>\n"
                               "<|im_start|>assistant\n";
    CHECK(format_prompt(plugin, "chatml", "other-model", "rules", "request") == chatml);
    CHECK(format_prompt(plugin, "chatml", "qwen35", "rules", "request") ==
          chatml + "<think>\n\n</think>\n\n");
    CHECK(format_prompt(plugin, "gpt-oss", "gpt-oss", "rules", "request") ==
          "<|start|>system<|message|>rules<|end|>"
          "<|start|>user<|message|>request<|end|>"
          "<|start|>assistant<|channel|>final<|message|>");
    plugin.assistant_prefix = "";
    CHECK(format_prompt(plugin, "chatml", "qwen35", "rules", "request") == chatml);
    plugin.assistant_prefix = "code:";
    CHECK(format_prompt(plugin, "chatml", "other-model", "rules", "request") == chatml + "code:");
    plugin = {};
    plugin.chat_template = "chatml";
    CHECK(format_prompt(plugin, nullptr, "other-model", "rules", "request") == chatml);
    plugin.chat_template = "unsupported-template";
    fails([&] { format_prompt(plugin, nullptr, "other-model", "rules", "request"); }, "unsupported");
    plugin = {};
    fails([&] { format_prompt(plugin, nullptr, "other-model", "rules", "request"); }, "no chat template");
    plugin.prompt_template = "S:{{system}}\nU:{{task}}\nA:";
    // Task contents are inserted once, even when they contain placeholder text.
    CHECK(format_prompt(plugin, nullptr, "qwen35", "rules", "literal {{system}}") ==
          "S:rules\nU:literal {{system}}\nA:");
    plugin.assistant_prefix = "start";
    CHECK(format_prompt(plugin, nullptr, "other-model", "rules", "request") ==
          "S:rules\nU:request\nA:start");

    // Both built-in chat formatting and a custom plugin must carry the full
    // grammar, including the restriction to atomic loop conditions.
    const auto & rules = language::system_prompt();
    typed::Prefix loop_example;
    CHECK(loop_example.append(language::break_example));
    CHECK(loop_example.can_end());
    const auto assert_grammar = [&](const std::string & prompt) {
        CHECK(prompt.find(language::grammar) != std::string::npos);
        CHECK(prompt.find(language::break_example) != std::string::npos);
        CHECK(prompt.find("\"while\" \"(\" bool_atom") != std::string::npos);
        CHECK(prompt.find("\"let\" identifier") != std::string::npos);
        CHECK(prompt.find("\"break\" \";\"") != std::string::npos);
        CHECK(prompt.find("\"continue\" \";\"") != std::string::npos);
    };
    assert_grammar(format_prompt(plugin, nullptr, "other-model", rules, "sum 2 and 3"));
    plugin = {};
    assert_grammar(format_prompt(plugin, "chatml", "qwen35", rules, "sum 2 and 3"));
    assert_grammar(format_prompt(plugin, "gpt-oss", "gpt-oss", rules, "sum 2 and 3"));

    const auto directory = std::filesystem::temp_directory_path() /
        ("ai-language-plugin-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(std::filesystem::create_directory(directory));
    const auto path = directory / "custom.json";
    const auto load = [&](const std::string & json) {
        { std::ofstream out(path); out << json; CHECK(out.good()); }
        return models::load_plugin(path.string());
    };
    plugin = load(R"({"model":"weights/model.gguf","gpu_layers":0,"ubatch":64,
                      "chat_template":"chatml","assistant_prefix":""})");
    CHECK(plugin.name == "custom");
    CHECK(plugin.model_path == (directory / "weights/model.gguf").string());
    CHECK(plugin.gpu_layers && *plugin.gpu_layers == 0);
    CHECK(plugin.ubatch && *plugin.ubatch == 64);
    CHECK(plugin.chat_template && *plugin.chat_template == "chatml");
    CHECK(plugin.assistant_prefix && plugin.assistant_prefix->empty());
    const auto selected = models::select_model(path.string());
    CHECK(selected.model_path == plugin.model_path);
    CHECK(selected.gpu_layers == plugin.gpu_layers);
    CHECK(selected.ubatch == plugin.ubatch);
    const auto uppercase = directory / "uppercase.JSON";
    std::filesystem::copy_file(path, uppercase);
    CHECK(models::select_model(uppercase.string()).model_path == plugin.model_path);
    // Prompt adapters can be loaded alone, but CLI selection requires weights.
    plugin = load(R"({"name":"mine","prompt_template":"{{system}}\n{{task}}"})");
    CHECK(plugin.name == "mine");
    CHECK(plugin.model_path.empty());
    fails([&] { models::select_model(path.string()); }, "needs a model path");
    plugin = load(R"({"model":"/absolute/weights.gguf"})");
    CHECK(plugin.model_path == "/absolute/weights.gguf");
    fails([&] { load("{"); }, "model plugin");
    fails([&] { load("[]"); }, "JSON object");
    fails([&] { load(R"({"unknown":1})"); }, "unknown field");
    fails([&] { load(R"({"model":false})"); }, "model must be a string");
    fails([&] { load(R"({"model":""})"); }, "must not be empty");
    fails([&] { load(R"({"model":"bad\u0000path"})"); }, "NUL bytes");
    fails([&] { load(R"({"ubatch":0})"); }, "ubatch must be an integer > 0");
    fails([&] { load(R"({"ubatch":1.5})"); }, "ubatch must be an integer");
    fails([&] { load(R"({"gpu_layers":-1})"); }, "gpu_layers must be an integer");
    fails([&] { load(R"({"gpu_layers":18446744073709551615})"); }, "gpu_layers must be an integer");
    fails([&] { load(R"({"chat_template":"chatml","prompt_template":"{{system}}{{task}}"})"); }, "not both");
    fails([&] { load(R"({"prompt_template":"{{task}}"})"); }, "must contain");
    fails([&] { models::load_plugin((directory / "missing.json").string()); }, "cannot open");
    std::filesystem::remove_all(directory);
    std::puts("model plugin tests passed");
}
