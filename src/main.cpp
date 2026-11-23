#include "typed_prefix.h"
#include "pretty_printer.h"
#include "typed_sampler.h"
#include "model_plugin.h"
#include "language_prompt.h"
#include "llama.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace {

void quiet_log(ggml_log_level level, const char * message, void *) {
    if (level == GGML_LOG_LEVEL_ERROR) std::fputs(message, stderr);
}

void usage(const char * executable, FILE * output = stderr, bool advanced = false) {
    std::fprintf(output,
        "usage: %s --model FILE --task TEXT\n"
        "       %s --check CODE\n\n"
        "  --task TEXT   Describe the program to generate.\n"
        "  --model FILE  GGUF weights or a JSON model plugin; required for generation.\n"
        "  --check CODE  Check code without loading a model.\n\n"
        "Generation includes the grammar prompt, type constraints, and formatted output.\n"
        "Use --help-advanced for development options and standalone utilities.\n",
        executable, executable);
    if (advanced) std::fputs(
        "\nAdvanced options:\n"
        "  --n N             Generated token budget (default 256).\n"
        "  --prefix CODE     Seed generation with a valid program prefix.\n"
        "  --prompt TEXT     Supply a prepared raw prompt instead of --task.\n"
        "  --raw             Stream unformatted output.\n"
        "  --unconstrained   Disable token constraints and check the result.\n"
        "  --debug           Trace selected and rejected tokens to stderr.\n"
        "  --debug-limit N   Rejected candidates shown per step (default 20).\n"
        "  --ngl N           Override GPU layers from the model plugin.\n"
        "  --ubatch N        Override physical batch size from the model plugin.\n"
        "  --complete CODE   Construct a valid suffix without a model.\n"
        "  --format [CODE|-] Format existing code; omit CODE to read stdin.\n\n"
        "Store GPU layers and batch size in a model plugin for everyday use.\n"
        "Use --unconstrained --raw to inspect invalid model output.\n", output);
}

std::string task_prompt(const llama_model * model, const models::Plugin & plugin, const std::string & task) {
    const std::string & system = language::system_prompt();
    const std::string user = "Produce a program for this request: " + task;
    return models::task_prompt(model, plugin, system, user);
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

} // namespace

int main(int argc, char ** argv) {
    std::string model_path;
    models::Plugin model_plugin;
    std::string check;
    std::string prefix;
    std::string task;
    std::string prompt = "Write only code in the typed Cangjie-inspired language. Start with main ( ) : Int64 { . ";
    int n_predict = 256;
    int n_ubatch = 0;
#ifdef __APPLE__
    int n_gpu_layers = 99;
#else
    int n_gpu_layers = 0;
#endif
    bool gpu_layers_set = false;
    bool checking = false;
    bool completing = false;
    bool formatting = true;
    bool format_requested = false;
    bool format_text = false;
    bool prompt_set = false;
    bool constrained = true;
    bool debug = false;
    size_t debug_limit = 20;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "--help-advanced") {
            usage(argv[0], stdout, arg == "--help-advanced");
            return 0;
        }
        if (arg == "--raw") {
            formatting = false;
            continue;
        }
        if (arg == "--unconstrained") {
            constrained = false;
            continue;
        }
        if (arg == "--debug") {
            debug = true;
            continue;
        }
        if (arg == "--format") {
            format_requested = true;
            if (i + 1 < argc && std::strncmp(argv[i + 1], "--", 2) != 0) {
                check = argv[++i];
                format_text = true;
            }
            continue;
        }
        if (i + 1 >= argc) { usage(argv[0]); return 1; }
        const std::string value = argv[++i];
        try {
            if (arg == "--model") model_path = value;
            else if (arg == "--check") { check = value; checking = true; }
            else if (arg == "--complete") { check = value; completing = true; }
            else if (arg == "--task") task = value;
            else if (arg == "--prefix") prefix = value;
            else if (arg == "--prompt") { prompt = value; prompt_set = true; }
            else if (arg == "--n") n_predict = std::stoi(value);
            else if (arg == "--ubatch") {
                n_ubatch = std::stoi(value);
                if (n_ubatch < 1) { usage(argv[0]); return 1; }
            }
            else if (arg == "--ngl") { n_gpu_layers = std::stoi(value); gpu_layers_set = true; }
            else if (arg == "--debug-limit") { debug_limit = std::stoul(value); debug = true; }
            else { usage(argv[0]); return 1; }
        } catch (...) { usage(argv[0]); return 1; }
    }

    if ((checking && completing) || (format_requested && (!formatting || checking || completing ||
        (format_text && (!task.empty() || !model_path.empty() || prompt_set))))) {
        usage(argv[0]);
        return 1;
    }
    if (format_requested && (format_text || (task.empty() && model_path.empty() && !prompt_set))) {
        if (!format_text) check = "-";
        if (check == "-") {
            check.clear();
            char buffer[4096];
            size_t size;
            while ((size = std::fread(buffer, 1, sizeof(buffer), stdin)) != 0)
                check.append(buffer, size);
            if (std::ferror(stdin)) { std::fputs("could not read stdin\n", stderr); return 2; }
        }
        const auto formatted = typed::format_program(check);
        if (!formatted) {
            std::fputs("format requires a complete, well-typed program\n", stderr);
            return 2;
        }
        std::fwrite(formatted->data(), 1, formatted->size(), stdout);
        return 0;
    }

    if (checking || completing) {
        typed::Prefix checker;
        const bool viable = checker.append(check);
        if (completing) {
            if (!viable) { std::fputs("invalid prefix\n", stderr); return 2; }
            const auto suffix = checker.completion();
            if (!suffix) { std::fputs("could not complete prefix\n", stderr); return 2; }
            std::fwrite(check.data(), 1, check.size(), stdout);
            std::fwrite(suffix->data(), 1, suffix->size(), stdout);
            std::fputc('\n', stdout);
            return 0;
        }
        std::printf("%s; complete=%s\n", viable ? "viable" : "invalid", viable && checker.can_end() ? "yes" : "no");
        return viable ? 0 : 2;
    }
    if (model_path.empty()) {
        std::fputs("generation requires --model FILE (GGUF weights or a JSON model plugin)\n", stderr);
        return 1;
    }
    if (n_predict < 1) {
        usage(argv[0]);
        return 1;
    }
    try {
        model_plugin = models::select_model(model_path);
    } catch (const std::exception & error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    model_path = model_plugin.model_path;
    if (!gpu_layers_set && model_plugin.gpu_layers) n_gpu_layers = *model_plugin.gpu_layers;
    if (!n_ubatch && model_plugin.ubatch) n_ubatch = *model_plugin.ubatch;

    typed::Prefix initial;
    if (!initial.append(prefix)) {
        std::fputs("invalid --prefix\n", stderr);
        return 1;
    }

    llama_log_set(quiet_log, nullptr);
    ggml_log_set(quiet_log, nullptr);
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = n_gpu_layers;
    llama_model * model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model) { std::fputs("failed to load model\n", stderr); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);

    std::string full_prompt;
    if (!task.empty()) {
        try {
            full_prompt = task_prompt(model, model_plugin, task);
        } catch (const std::exception & error) {
            std::fprintf(stderr, "%s\n", error.what());
            llama_model_free(model);
            llama_backend_free();
            return 1;
        }
    } else {
        full_prompt = prompt;
    }
    full_prompt += prefix;
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

    llama_sampler * sampler = constrained ? typed_sampler_init(vocab, initial, debug, debug_limit) : llama_sampler_init_greedy();
    typed::Prefix emitted = initial;
    std::string generated = prefix;
    if (!formatting) std::fwrite(prefix.data(), 1, prefix.size(), stdout);
    int position = 0;
    int batch_count = token_count;
    bool complete = false;
    bool failed = false;
    for (int i = 0; i < n_predict; ++i) {
        if (llama_process(context, LLAMA_PROCESS_TYPE_DECODE, batch)) {
            std::fputs("model decode failed\n", stderr);
            failed = true;
            break;
        }
        position += batch_count;
        const llama_token token = llama_sampler_sample(sampler, context, -1);
        if (llama_vocab_is_eog(vocab, token)) { complete = true; break; }
        const std::string text = token_piece(vocab, token);
        if (constrained && !emitted.append(text)) {
            std::fputs("sampler emitted an invalid token\n", stderr);
            failed = true;
            break;
        }
        generated += text;
        if (!formatting) {
            std::fwrite(text.data(), 1, text.size(), stdout);
            std::fflush(stdout);
        }
        batch_set(batch, &token, 1, position);
        batch_count = 1;
    }
    if (constrained && !complete && !failed) {
        const auto suffix = emitted.completion();
        if (suffix && emitted.append(*suffix) && emitted.can_end()) {
            if (!formatting) std::fwrite(suffix->data(), 1, suffix->size(), stdout);
            generated += *suffix;
            complete = true;
            std::fputs("model token budget exhausted; finished the program deterministically\n", stderr);
        }
    }
    if (!formatting) std::fputc('\n', stdout);
    if (formatting && !failed) {
        const auto formatted = typed::format_program(generated);
        if (formatted) {
            std::fwrite(formatted->data(), 1, formatted->size(), stdout);
        } else {
            std::fputs("cannot format generated output: program is invalid or incomplete\n", stderr);
            failed = true;
        }
    }
    if (!constrained) {
        typed::Prefix validator;
        const bool viable = validator.append(generated);
        std::fprintf(stderr, "typed-check=%s\n", !viable ? "invalid" : validator.can_end() ? "pass" : "incomplete");
    }
    if (!complete) std::fputs(constrained ? "unable to finish the generated prefix\n" :
        "model stopped before an end token\n", stderr);

    llama_sampler_free(sampler);
    llama_batch_ext_free(batch);
    llama_free(context);
    llama_model_free(model);
    llama_backend_free();
    return complete && !failed ? 0 : 2;
}
