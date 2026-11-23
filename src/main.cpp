#include "typed_prefix.h"
#include "pretty_printer.h"
#include "cj_codegen.h"
#include "typed_sampler.h"
#include "model_plugin.h"
#include "language_prompt.h"
#include "cli.h"
#include "planning.h"
#include "sampling.h"
#include "llama.h"

#include <cstdio>
#include <algorithm>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
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

planning::Plan run_planner(llama_model * model, const models::Plugin & plugin,
                           const planning::Request & request, int ubatch, float temperature, uint32_t seed) {
    const auto prompt = models::task_prompt(model, plugin, request.system, request.user);
    const auto * vocab = llama_model_get_vocab(model);
    if (prompt.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("planning prompt is too long");
    const int count = -llama_tokenize(vocab, prompt.data(), prompt.size(), nullptr, 0, true, true);
    if (count <= 0 || count > std::numeric_limits<int>::max() - 2048 - 8)
        throw std::runtime_error("failed to tokenize planning prompt within the context limit");
    std::vector<llama_token> tokens(static_cast<size_t>(count));
    if (llama_tokenize(vocab, prompt.data(), prompt.size(), tokens.data(), count, true, true) < 0)
        throw std::runtime_error("failed to tokenize planning prompt");

    auto params = llama_context_default_params();
    params.n_ctx = std::max(4096, count + 2048);
    params.n_batch = params.n_ctx;
    if (ubatch > 0) params.n_ubatch = ubatch;
    std::unique_ptr<llama_context, decltype(&llama_free)> context(llama_init_from_model(model, params), llama_free);
    if (!context) throw std::runtime_error("failed to create planning context");
    std::unique_ptr<llama_batch_ext, decltype(&llama_batch_ext_free)> batch(llama_batch_ext_init(context.get()), llama_batch_ext_free);
    std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)> sampler(
        sampling::sampler_init(temperature, seed), llama_sampler_free);
    if (!batch || !sampler) throw std::runtime_error("failed to allocate planning batch or sampler");
    batch_set(batch.get(), tokens.data(), count, 0);
    planning::Plan plan;
    std::vector<llama_token> history;
    int position = 0;
    int batch_count = count;
    while (true) {
        if (llama_process(context.get(), LLAMA_PROCESS_TYPE_DECODE, batch.get()))
            throw std::runtime_error("planning model decode failed");
        position += batch_count;
        const auto token = llama_sampler_sample(sampler.get(), context.get(), -1);
        if (llama_vocab_is_eog(vocab, token)) { plan.complete = true; break; }
        plan.text += token_piece(vocab, token);
        history.push_back(token);
        if (position >= static_cast<int>(llama_n_ctx(context.get()))) {
            const auto replay = planning::replay_tokens(tokens, history, llama_n_ctx(context.get()));
            llama_memory_clear(llama_get_memory(context.get()), true);
            position = 0;
            batch_count = static_cast<int>(replay.size());
            batch_set(batch.get(), replay.data(), batch_count, 0);
        } else {
            batch_set(batch.get(), &token, 1, position);
            batch_count = 1;
        }
    }
    // The planner context and sampler are freed before code generation begins.
    return plan;
}

} // namespace

int main(int argc, char ** argv) {
    cli::Options options;
    try {
        options = cli::parse(argc, argv);
    } catch (const std::exception & error) {
        std::fprintf(stderr, "%s\nUse '%s --help' for commands and 'generate --help' for generation options.\n",
                     error.what(), argv[0]);
        return 1;
    }
    if (options.help) {
        cli::print_help(stdout, argv[0], options.command);
        return 0;
    }
    if (options.command != cli::Command::Generate) {
        std::string & check = options.input;
        const bool from_stdin = check == "-";
        if (from_stdin) {
            check.clear();
            char buffer[4096];
            size_t size;
            while ((size = std::fread(buffer, 1, sizeof(buffer), stdin)) != 0)
                check.append(buffer, size);
            if (std::ferror(stdin)) { std::fputs("could not read stdin\n", stderr); return 2; }
        }
        const std::string_view source_path = from_stdin ? "<stdin>" : "<input>";
        const auto report_check_error = [&](const typed::Prefix & checker, bool viable) {
            if (!viable && checker.diagnostic())
                typed::print_diagnostic(stderr, source_path, check, *checker.diagnostic());
            else if (viable)
                typed::print_diagnostic(stderr, source_path, check, checker.end_diagnostic());
        };
        if (options.command == cli::Command::Format) {
            typed::Prefix checker;
            const bool viable = checker.append(check);
            if (!viable || !checker.can_end()) {
                report_check_error(checker, viable);
                return 2;
            }
            const auto formatted = typed::format_program(check);
            if (!formatted) {
                std::fputs("format requires a complete, well-typed program\n", stderr);
                return 2;
            }
            std::fwrite(formatted->data(), 1, formatted->size(), stdout);
            return 0;
        }
        typed::Prefix checker;
        const bool viable = checker.append(check);
        if (options.command == cli::Command::Complete) {
            if (!viable) {
                report_check_error(checker, false);
                return 2;
            }
            const auto suffix = checker.completion();
            if (!suffix) { std::fputs("could not complete prefix\n", stderr); return 2; }
            std::fwrite(check.data(), 1, check.size(), stdout);
            std::fwrite(suffix->data(), 1, suffix->size(), stdout);
            std::fputc('\n', stdout);
            return 0;
        }
        if (!viable) report_check_error(checker, false);
        std::printf("%s; complete=%s\n", viable ? "viable" : "invalid", viable && checker.can_end() ? "yes" : "no");
        return viable ? 0 : 2;
    }

    models::Plugin model_plugin;
    try {
        model_plugin = models::select_model(options.model_path);
    } catch (const std::exception & error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    const std::string & model_path = model_plugin.model_path;
    const std::string & task = options.input;
    const int n_predict = options.tokens;
    const int n_ubatch = options.ubatch ? options.ubatch : model_plugin.ubatch.value_or(0);
#ifdef __APPLE__
    const int platform_gpu_layers = 99;
#else
    const int platform_gpu_layers = 0;
#endif
    const int n_gpu_layers = options.gpu_layers.value_or(model_plugin.gpu_layers.value_or(platform_gpu_layers));
    const bool constrained = options.constrained;
    const bool debug = options.debug;
    const size_t debug_limit = options.debug_limit;

    const typed::Prefix initial;

    std::optional<planning::Prepared> supplied_plan;
    if (!options.plan_file.empty()) {
        try {
            std::ifstream file(options.plan_file, std::ios::binary);
            if (!file) throw std::runtime_error("could not read plan file: " + options.plan_file);
            const std::string text(std::istreambuf_iterator<char>{file}, {});
            if (file.bad()) throw std::runtime_error("could not read plan file: " + options.plan_file);
            supplied_plan = planning::with_plan(task, planning::Plan{ text, true });
        } catch (const std::exception & error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }

    const uint32_t seed = options.seed ? *options.seed : std::random_device{}();
    std::fprintf(stderr, "Sampling: temperature=%g seed=%u\n", options.temperature, seed);

    llama_log_set(quiet_log, nullptr);
    ggml_log_set(quiet_log, nullptr);
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = n_gpu_layers;
    llama_model * model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model) { std::fputs("failed to load model\n", stderr); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);

    std::string full_prompt;
    try {
        const auto prepared = supplied_plan ? std::move(*supplied_plan) :
            planning::prepare(task, options.plan, [&](const planning::Request & request) {
                if (debug) {
                    std::fputs("Planning implementation...\n", stderr);
                    std::fflush(stderr);
                }
                return run_planner(model, model_plugin, request, n_ubatch, options.temperature, seed);
            });
        if (options.plan_only) {
            std::fwrite(prepared.plan->text.data(), 1, prepared.plan->text.size(), stdout);
            const bool output_failed = std::ferror(stdout);
            llama_model_free(model);
            llama_backend_free();
            return output_failed ? 1 : 0;
        }
        if (debug && prepared.plan) {
            std::fprintf(stderr, "Implementation plan:\n%s\n\n", prepared.plan->text.c_str());
        }
        full_prompt = models::task_prompt(model, model_plugin, prepared.system, prepared.user);
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
        return 1;
    }

    llama_context_params context_params = llama_context_default_params();
    context_params.n_ctx = token_count + n_predict + 8;
    context_params.n_batch = token_count;
    if (n_ubatch > 0) context_params.n_ubatch = n_ubatch;
    llama_context * context = llama_init_from_model(model, context_params);
    if (!context) { std::fputs("failed to create context\n", stderr); return 1; }
    llama_batch_ext * batch = llama_batch_ext_init(context);
    batch_set(batch, prompt_tokens.data(), token_count, 0);

    llama_sampler * sampler = constrained
        ? typed_sampler_init(vocab, initial, debug, debug_limit, options.temperature, seed)
        : sampling::sampler_init(options.temperature, seed);
    typed::Prefix emitted = initial;
    std::string generated;
    int position = 0;
    int batch_count = token_count;
    int generated_tokens = 0;
    bool complete = false;
    bool failed = false;
    bool inference_failed = false;
    for (int i = 0; i < n_predict; ++i) {
        if (llama_process(context, LLAMA_PROCESS_TYPE_DECODE, batch)) {
            std::fputs("model decode failed\n", stderr);
            failed = true;
            inference_failed = true;
            break;
        }
        ++generated_tokens;
        position += batch_count;
        const llama_token token = llama_sampler_sample(sampler, context, -1);
        if (llama_vocab_is_eog(vocab, token)) { complete = true; break; }
        const std::string text = token_piece(vocab, token);
        if (constrained && !emitted.append(text)) {
            std::fputs("sampler emitted an invalid token\n", stderr);
            failed = true;
            inference_failed = true;
            break;
        }
        generated += text;
        batch_set(batch, &token, 1, position);
        batch_count = 1;
    }
    std::fprintf(stderr, "generated_tokens=%d\n", generated_tokens);
    if (constrained && options.force_postfix && !complete && !failed) {
        const auto suffix = emitted.completion();
        if (suffix && emitted.append(*suffix) && emitted.can_end()) {
            generated += *suffix;
            complete = true;
            std::fputs("model token budget exhausted; appended a typed postfix\n", stderr);
        }
    }
    bool translated = false;
    bool finished = false;
    if (!failed) {
        if (constrained) {
            finished = emitted.can_end();
        } else {
            typed::Prefix validator;
            const bool viable = validator.append(generated);
            finished = viable && validator.can_end();
            std::fprintf(stderr, "typed-check=%s\n", !viable ? "invalid" : finished ? "pass" : "incomplete");
            if (!viable && validator.diagnostic())
                typed::print_diagnostic(stderr, "<generated>", generated, *validator.diagnostic());
            else if (viable && !finished)
                typed::print_diagnostic(stderr, "<generated>", generated, validator.end_diagnostic());
        }
        if (finished) {
            const auto cangjie = typed::generate_cangjie(generated);
            if (!cangjie) {
                std::fputs("could not translate the typed program to Cangjie\n", stderr);
                failed = true;
            } else {
                std::fwrite(cangjie->data(), 1, cangjie->size(), stdout);
                translated = !std::ferror(stdout);
                failed = !translated;
            }
        }
    }
    if (!translated && !inference_failed && !finished) {
        if (constrained) std::fputs("unable to finish the generated prefix\n", stderr);
        else if (!complete) std::fputs("model stopped before an end token\n", stderr);
        else std::fputs("generated text is not a complete typed program, so no Cangjie was written\n", stderr);
    }

    llama_sampler_free(sampler);
    llama_batch_ext_free(batch);
    llama_free(context);
    llama_model_free(model);
    llama_backend_free();
    return inference_failed ? 1 : translated && !failed ? 0 : 2;
}
