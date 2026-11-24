#include "llama.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace {

void quiet_log(ggml_log_level level, const char * message, void *) {
    if (level == GGML_LOG_LEVEL_ERROR) std::fputs(message, stderr);
}

int count_tokens(const llama_vocab * vocab, const std::string & text) {
    if (text.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) return -1;
    const int counted = llama_tokenize(vocab, text.data(), static_cast<int32_t>(text.size()), nullptr, 0, false, false);
    if (counted == std::numeric_limits<int32_t>::min()) return -1;
    return counted < 0 ? -counted : counted;
}

} // namespace

int main(int argc, char ** argv) {
    if (argc < 3) {
        std::fputs("usage: count-tokens MODEL FILE...\n", stderr);
        return 2;
    }
    llama_log_set(quiet_log, nullptr);
    llama_backend_init();
    llama_model_params params = llama_model_default_params();
    params.vocab_only = true;
    params.n_gpu_layers = 0;
    llama_model * model = llama_model_load_from_file(argv[1], params);
    if (!model) {
        std::fputs("failed to load model\n", stderr);
        llama_backend_free();
        return 1;
    }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    int status = 0;
    for (int i = 2; i < argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        if (!file) {
            std::fprintf(stderr, "could not read %s\n", argv[i]);
            status = 1;
            break;
        }
        const std::string text((std::istreambuf_iterator<char>(file)), {});
        if (file.bad()) {
            std::fprintf(stderr, "could not read %s\n", argv[i]);
            status = 1;
            break;
        }
        const int tokens = count_tokens(vocab, text);
        if (tokens <= 0) {
            std::fprintf(stderr, "failed to tokenize %s\n", argv[i]);
            status = 1;
            break;
        }
        std::printf("%d\n", tokens);
    }
    llama_model_free(model);
    llama_backend_free();
    return status;
}
