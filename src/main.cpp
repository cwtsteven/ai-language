#include "typed_prefix.h"
#include "typed_sampler.h"
#include "llama.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void quiet_log(ggml_log_level level, const char * message, void *) {
    if (level == GGML_LOG_LEVEL_ERROR) std::fputs(message, stderr);
}

void usage(const char * executable) {
    std::fprintf(stderr,
        "usage: %s --check TEXT\n"
        "       %s --complete TEXT\n"
        "       %s --task REQUEST [--model MODEL.gguf] [--n 256] [--prefix CODE] [--debug] [--unconstrained]\n"
        "       %s --model MODEL.gguf [--prefix CODE] [--prompt TEXT] [--n 256] [--ngl N]\n",
        executable, executable, executable, executable);
}

std::string task_prompt(const llama_model * model, const std::string & task) {
    const char * tmpl = llama_model_chat_template(model, nullptr);
    if (!tmpl) return {};

    const std::string system =
        "Write exactly one program in the following small Cangjie-inspired typed language. Output code only: no Markdown or explanation. "
        "Put whitespace between every token, including punctuation. "
        "Start with: main ( ) : Int64 { . End with: return INT_ATOM ; } . "
        "The body may use let name : Type = rhs ;, cell := rhs ;, "
        "if ( bool_atom ) { statements } else { statements }, or "
        "while ( bool_atom ) { statements }. "
        "Types are Int64, Bool, RefInt64, RefBool. Atoms are integer literals, true, false, in-scope variables, or !cell to dereference a cell. Whitespace after ! is allowed. Keep := together. "
        "Right-hand sides may be atoms, one binary expression of atomic operands, or these calls with atomic arguments. "
        "Binary operators: Int64 + Int64, Int64 - Int64, Int64 * Int64 produce Int64; "
        "Int64 < Int64 and Int64 == Int64 produce Bool; Bool && Bool, Bool || Bool, and Bool == Bool produce Bool. "
        "add(Int64,Int64):Int64, sub(Int64,Int64):Int64, mul(Int64,Int64):Int64, "
        "eq(Int64,Int64):Bool, lt(Int64,Int64):Bool, not(Bool):Bool, and(Bool,Bool):Bool, or(Bool,Bool):Bool, "
        "refInt(Int64):RefInt64, refBool(Bool):RefBool. "
        "Every computed intermediate value must be bound by let or assigned to a cell. Do not nest operations. "
        "All let bindings are immutable. For mutation, allocate a RefInt64 or RefBool cell. "
        "Read it with !cell and update its contents with cell := value. Never use var, write(), readInt(), or readBool(). "
        "A while (!flag) loop rereads a RefBool cell on each iteration. "
        "Initialize its flag with refBool(true) to enter the loop; use flag := false to stop it. "
        "For a counter loop, compare the current counter with the limit inside the loop, "
        "then stop or update the counter. Return only after the loop, not inside it. "
        "Arithmetic example: main ( ) : Int64 { let x : Int64 = 2 + 3 ; return x ; } "
        "Mutable cell example: main ( ) : Int64 { let x : RefInt64 = refInt ( 5 ) ; "
        "x := ! x + 1 ; return ! x ; }";
    const std::string user = "Produce a program for this request: " + task;
    const llama_chat_message messages[] = {
        { "system", system.c_str() },
        { "user", user.c_str() },
    };
    const int32_t length = llama_chat_apply_template(tmpl, messages, 2, true, nullptr, 0);
    if (length <= 0) return {};
    std::string formatted(static_cast<size_t>(length), '\0');
    const int32_t written = llama_chat_apply_template(tmpl, messages, 2, true,
        formatted.data(), static_cast<int32_t>(formatted.size()));
    if (written != length) return {};
    char architecture[64] = {};
    if (llama_model_meta_val_str(model, "general.architecture", architecture, sizeof(architecture)) > 0 &&
        std::strcmp(architecture, "qwen35") == 0) {
        // The Qwen3.5 chat template enters thinking mode by default. Start the
        // final answer explicitly so the code sampler does not have to reject
        // the model's preferred <think> token as its first candidate.
        formatted += "<think>\n\n</think>\n\n";
    }
    return formatted;
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
    std::string check;
    std::string prefix;
    std::string task;
    std::string prompt = "Write only code in the typed Cangjie-inspired language. Start with main ( ) : Int64 { . ";
    int n_predict = 256;
#ifdef __APPLE__
    int n_gpu_layers = 99;
#else
    int n_gpu_layers = 0;
#endif
    bool checking = false;
    bool completing = false;
    bool constrained = true;
    bool debug = false;
    size_t debug_limit = 20;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--unconstrained") {
            constrained = false;
            continue;
        }
        if (arg == "--debug") {
            debug = true;
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
            else if (arg == "--prompt") prompt = value;
            else if (arg == "--n") n_predict = std::stoi(value);
            else if (arg == "--ngl") n_gpu_layers = std::stoi(value);
            else if (arg == "--debug-limit") { debug_limit = std::stoul(value); debug = true; }
            else { usage(argv[0]); return 1; }
        } catch (...) { usage(argv[0]); return 1; }
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
    if (model_path.empty() && !task.empty()) {
        model_path = "models/Qwen3.5-9B-Q4_K_M.gguf";
    }
    if (model_path.empty() || n_predict < 1) { usage(argv[0]); return 1; }

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
        full_prompt = task_prompt(model, task);
        if (full_prompt.empty()) {
            std::fputs("model has no supported chat template; use --prompt instead\n", stderr);
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
    llama_context * context = llama_init_from_model(model, context_params);
    if (!context) { std::fputs("failed to create context\n", stderr); return 1; }
    llama_batch_ext * batch = llama_batch_ext_init(context);
    batch_set(batch, prompt_tokens.data(), token_count, 0);

    llama_sampler * sampler = constrained ? typed_sampler_init(vocab, initial, debug, debug_limit) : llama_sampler_init_greedy();
    typed::Prefix emitted = initial;
    std::string generated = prefix;
    std::fwrite(prefix.data(), 1, prefix.size(), stdout);
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
        std::fwrite(text.data(), 1, text.size(), stdout);
        std::fflush(stdout);
        batch_set(batch, &token, 1, position);
        batch_count = 1;
    }
    if (constrained && !complete && !failed) {
        const auto suffix = emitted.completion();
        if (suffix && emitted.append(*suffix) && emitted.can_end()) {
            std::fwrite(suffix->data(), 1, suffix->size(), stdout);
            generated += *suffix;
            complete = true;
            std::fputs("model token budget exhausted; finished the program deterministically\n", stderr);
        }
    }
    std::fputc('\n', stdout);
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
    return complete ? 0 : 2;
}
