#include "model_plugin.h"
#include "sampling.h"
#include "planning.h"
#include "llama.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void quiet_log(ggml_log_level level, const char * message, void *) {
    if (level == GGML_LOG_LEVEL_ERROR) std::fputs(message, stderr);
}

std::string token_piece(const llama_vocab * vocab, llama_token token) {
    std::string result(64, '\0');
    int n = llama_token_to_piece(vocab, token, result.data(), static_cast<int>(result.size()), 0, false);
    if (n < 0) {
        result.resize(static_cast<size_t>(-n));
        n = llama_token_to_piece(vocab, token, result.data(), static_cast<int>(result.size()), 0, false);
    }
    if (n < 0) return {};
    result.resize(static_cast<size_t>(n));
    return result;
}

void batch_set(llama_batch_ext * batch, const llama_token * tokens, int count, llama_pos pos) {
    llama_batch_ext_clear(batch);
    for (int i = 0; i < count; ++i) {
        int index = llama_batch_ext_add_token(batch, 0, tokens[i]);
        llama_pos p = pos + i;
        llama_batch_ext_set_pos(batch, index, &p);
    }
    llama_batch_ext_set_output_logits(batch, count - 1, true);
}

std::string read_file(const std::string & path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("could not read " + path);
    const std::string text(std::istreambuf_iterator<char>{file}, {});
    if (file.bad()) throw std::runtime_error("could not read " + path);
    return text;
}

struct Options {
    std::string model;
    std::string system_file;
    std::string user_file;
    int tokens = -1;
    float temperature = 0.8f;
    std::optional<uint32_t> seed;
    int ubatch = 0;
    int threads = 4;
    std::optional<int> gpu_layers;
    bool help = false;
};

void print_help(const char * executable) {
    std::fprintf(stdout,
        "usage: %s --model FILE --system-file FILE --user-file FILE [OPTIONS]\n\n"
        "Sample unconstrained text with the same chat wrapper as typed-cangjie.\n"
        "This command does not load the typed-language specification or checker.\n\n"
        "  --model FILE         Required GGUF weights or JSON model plugin.\n"
        "  --system-file FILE   System message, including the language specification.\n"
        "  --user-file FILE     User message.\n"
        "  --n N                Maximum generated tokens. Omit it to sample until the end token.\n"
        "  --temperature T      Sampling randomness (default 0.8; 0 = greedy).\n"
        "  --seed N             Sampling seed (default random, printed to stderr).\n"
        "  --threads N          CPU threads for prompt processing and generation (default 4).\n"
        "  --ngl N              GPU layers (default 99 on macOS, 0 elsewhere).\n"
        "  --ubatch N           Physical batch size.\n"
        "  -h, --help           Show this help.\n",
        executable);
}

std::string require_value(int & index, int argc, char ** argv) {
    if (index + 1 >= argc) throw std::runtime_error(std::string(argv[index]) + " requires a value");
    return argv[++index];
}

Options parse(int argc, char ** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { options.help = true; continue; }
        if (arg == "--model") options.model = require_value(i, argc, argv);
        else if (arg == "--system-file") options.system_file = require_value(i, argc, argv);
        else if (arg == "--user-file") options.user_file = require_value(i, argc, argv);
        else if (arg == "--n") {
            const std::string value = require_value(i, argc, argv);
            try {
                size_t used = 0;
                const int parsed = std::stoi(value, &used);
                if (used != value.size() || parsed <= 0) throw std::invalid_argument("n");
                options.tokens = parsed;
            } catch (const std::exception &) {
                throw std::runtime_error("--n requires a positive integer");
            }
        } else if (arg == "--temperature") {
            const std::string value = require_value(i, argc, argv);
            try {
                size_t used = 0;
                options.temperature = std::stof(value, &used);
                if (used != value.size() || !(options.temperature >= 0)) throw std::invalid_argument("temperature");
            } catch (const std::exception &) {
                throw std::runtime_error("--temperature requires a finite nonnegative number");
            }
        } else if (arg == "--seed") {
            const std::string value = require_value(i, argc, argv);
            try {
                size_t used = 0;
                const unsigned long parsed = std::stoul(value, &used);
                if (used != value.size() || parsed > 4294967295ul) throw std::invalid_argument("seed");
                options.seed = static_cast<uint32_t>(parsed);
            } catch (const std::exception &) {
                throw std::runtime_error("--seed requires an integer from 0 to 4294967295");
            }
        } else if (arg == "--ngl") {
            const std::string value = require_value(i, argc, argv);
            try {
                size_t used = 0;
                const int parsed = std::stoi(value, &used);
                if (used != value.size() || parsed < 0) throw std::invalid_argument("ngl");
                options.gpu_layers = parsed;
            } catch (const std::exception &) {
                throw std::runtime_error("--ngl requires a nonnegative integer");
            }
        } else if (arg == "--threads") {
            const std::string value = require_value(i, argc, argv);
            try {
                size_t used = 0;
                const int parsed = std::stoi(value, &used);
                if (used != value.size() || parsed <= 0) throw std::invalid_argument("threads");
                options.threads = parsed;
            } catch (const std::exception &) {
                throw std::runtime_error("--threads requires a positive integer");
            }
        } else if (arg == "--ubatch") {
            const std::string value = require_value(i, argc, argv);
            try {
                size_t used = 0;
                const int parsed = std::stoi(value, &used);
                if (used != value.size() || parsed <= 0) throw std::invalid_argument("ubatch");
                options.ubatch = parsed;
            } catch (const std::exception &) {
                throw std::runtime_error("--ubatch requires a positive integer");
            }
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }
    if (!options.help) {
        if (options.model.empty() || options.system_file.empty() || options.user_file.empty())
            throw std::runtime_error("--model, --system-file, and --user-file are required");
    }
    return options;
}

} // namespace

int main(int argc, char ** argv) {
    Options options;
    try {
        options = parse(argc, argv);
    } catch (const std::exception & error) {
        std::fprintf(stderr, "%s\nUse '%s --help' for options.\n", error.what(), argv[0]);
        return 1;
    }
    if (options.help) {
        print_help(argv[0]);
        return 0;
    }

    std::string system;
    std::string user;
    try {
        system = read_file(options.system_file);
        user = read_file(options.user_file);
    } catch (const std::exception & error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }

    models::Plugin model_plugin;
    try {
        model_plugin = models::select_model(options.model);
    } catch (const std::exception & error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    const int n_ubatch = options.ubatch ? options.ubatch : model_plugin.ubatch.value_or(0);
#ifdef __APPLE__
    const int platform_gpu_layers = 99;
#else
    const int platform_gpu_layers = 0;
#endif
    const int n_gpu_layers = options.gpu_layers.value_or(model_plugin.gpu_layers.value_or(platform_gpu_layers));
    const uint32_t seed = options.seed ? *options.seed : std::random_device{}();
    std::fprintf(stderr, "Sampling: temperature=%g seed=%u threads=%d\n", options.temperature, seed, options.threads);

    llama_log_set(quiet_log, nullptr);
    ggml_log_set(quiet_log, nullptr);
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = n_gpu_layers;
    llama_model * model = llama_model_load_from_file(model_plugin.model_path.c_str(), model_params);
    if (!model) { std::fputs("failed to load model\n", stderr); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);

    std::string full_prompt;
    try {
        full_prompt = models::task_prompt(model, model_plugin, system, user);
    } catch (const std::exception & error) {
        std::fprintf(stderr, "%s\n", error.what());
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }
    const int token_count = -llama_tokenize(vocab, full_prompt.data(), full_prompt.size(), nullptr, 0, true, true);
    if (token_count <= 0) { std::fputs("failed to tokenize prompt\n", stderr); return 1; }
    std::vector<llama_token> prompt_tokens(static_cast<size_t>(token_count));
    if (llama_tokenize(vocab, full_prompt.data(), full_prompt.size(), prompt_tokens.data(), token_count, true, true) < 0) {
        std::fputs("failed to tokenize prompt\n", stderr);
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }

    const bool until_end = options.tokens < 0;
    llama_context_params context_params = llama_context_default_params();
    context_params.n_threads = options.threads;
    context_params.n_threads_batch = options.threads;
    if (until_end) {
        if (token_count > std::numeric_limits<int>::max() - 2048 - 8) {
            std::fputs("failed to tokenize prompt within the context limit\n", stderr);
            llama_model_free(model);
            llama_backend_free();
            return 1;
        }
        context_params.n_ctx = std::max(4096, token_count + 2048);
        context_params.n_batch = context_params.n_ctx;
    } else {
        context_params.n_ctx = token_count + options.tokens + 8;
        context_params.n_batch = token_count;
    }
    if (n_ubatch > 0) context_params.n_ubatch = n_ubatch;
    llama_context * context = llama_init_from_model(model, context_params);
    if (!context) { std::fputs("failed to create context\n", stderr); return 1; }
    llama_batch_ext * batch = llama_batch_ext_init(context);
    batch_set(batch, prompt_tokens.data(), token_count, 0);
    llama_sampler * sampler = sampling::sampler_init(options.temperature, seed);

    std::string generated;
    std::vector<llama_token> history;
    int position = 0;
    int batch_count = token_count;
    int generated_tokens = 0;
    bool failed = false;
    const int limit = until_end ? std::numeric_limits<int>::max() : options.tokens;
    for (int i = 0; i < limit; ++i) {
        if (llama_process(context, LLAMA_PROCESS_TYPE_DECODE, batch)) {
            std::fputs("model decode failed\n", stderr);
            failed = true;
            break;
        }
        ++generated_tokens;
        position += batch_count;
        const llama_token token = llama_sampler_sample(sampler, context, -1);
        if (llama_vocab_is_eog(vocab, token)) break;
        const std::string text = token_piece(vocab, token);
        generated += text;
        history.push_back(token);
        if (!until_end) {
            std::fwrite(text.data(), 1, text.size(), stdout);
            std::fflush(stdout);
        }
        if (until_end && position >= static_cast<int>(llama_n_ctx(context))) {
            const auto replay = planning::replay_tokens(prompt_tokens, history, llama_n_ctx(context));
            llama_memory_clear(llama_get_memory(context), true);
            position = 0;
            batch_count = static_cast<int>(replay.size());
            batch_set(batch, replay.data(), batch_count, 0);
        } else {
            batch_set(batch, &token, 1, position);
            batch_count = 1;
        }
    }
    std::fprintf(stderr, "tokens: input=%d output=%d\n", token_count, generated_tokens);
    if (until_end && !failed)
        std::fwrite(generated.data(), 1, generated.size(), stdout);
    if (!until_end) std::fputc('\n', stdout);
    llama_sampler_free(sampler);
    llama_batch_ext_free(batch);
    llama_free(context);
    llama_model_free(model);
    llama_backend_free();
    return failed || std::ferror(stdout) ? 1 : 0;
}
