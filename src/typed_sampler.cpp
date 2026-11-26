#include "typed_sampler.h"
#include "sampling.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace {

struct Context {
    const llama_vocab * vocab;
    typed::Prefix initial;
    typed::Prefix state;
    bool debug;
    size_t debug_limit;
    size_t step;
    sampling::Selector selector;
};

std::string printable(std::string_view bytes) {
    std::string result;
    const size_t shown = std::min<size_t>(bytes.size(), 80);
    for (size_t i = 0; i < shown; ++i) {
        const unsigned char ch = static_cast<unsigned char>(bytes[i]);
        if (ch == '\n') result += "\\n";
        else if (ch == '\r') result += "\\r";
        else if (ch == '\t') result += "\\t";
        else if (ch == '"') result += "\\\"";
        else if (ch == '\\') result += "\\\\";
        else if (ch >= 0x20 && ch <= 0x7e) result.push_back(static_cast<char>(ch));
        else {
            char escaped[5];
            std::snprintf(escaped, sizeof(escaped), "\\x%02x", ch);
            result += escaped;
        }
    }
    if (bytes.size() > shown) result += "...";
    return result;
}

std::string piece(const llama_vocab * vocab, llama_token token) {
    std::string result(64, '\0');
    int n = llama_token_to_piece(vocab, token, result.data(), static_cast<int>(result.size()), 0, false);
    if (n < 0) {
        result.resize(static_cast<size_t>(-n));
        n = llama_token_to_piece(vocab, token, result.data(), static_cast<int>(result.size()), 0, false);
    }
    if (n <= 0) return {};
    result.resize(static_cast<size_t>(n));
    return result;
}

const char * name(const llama_sampler *) { return "typed-temperature"; }

void accept(llama_sampler * sampler, llama_token token) {
    auto * ctx = static_cast<Context *>(sampler->ctx);
    if (llama_vocab_is_eog(ctx->vocab, token)) {
        if (!ctx->state.can_end()) {
            std::fputs("typed sampler: premature end token\n", stderr);
            std::abort();
        }
        ++ctx->step;
        return;
    }
    if (!ctx->state.append(piece(ctx->vocab, token))) {
        std::fputs("typed sampler: accepted token violated prefix invariant\n", stderr);
        std::abort();
    }
    ++ctx->step;
}

void apply(llama_sampler * sampler, llama_token_data_array * candidates) {
    auto * ctx = static_cast<Context *>(sampler->ctx);
    // Once the program is complete, end immediately. Otherwise the model
    // may prefer legal trailing whitespace forever.
    if (ctx->state.can_end()) {
        size_t end = candidates->size;
        for (size_t i = 0; i < candidates->size; ++i) {
            if (llama_vocab_is_eog(ctx->vocab, candidates->data[i].id) &&
                (end == candidates->size || candidates->data[i].logit > candidates->data[end].logit)) {
                end = i;
            }
        }
        if (end != candidates->size) {
            candidates->selected = static_cast<int32_t>(end);
            if (ctx->debug) {
                std::fprintf(stderr, "[typed step %zu] SELECT <eog> (program complete)\n", ctx->step + 1);
            }
            return;
        }
        std::fputs("typed sampler: complete program but no end token in candidates\n", stderr);
        std::abort();
    }
    size_t rejected = 0;
    size_t winner = ctx->selector.choose(*candidates, [&](size_t i) {
        const llama_token token = candidates->data[i].id;
        const bool eog = llama_vocab_is_eog(ctx->vocab, token);
        const std::string text = eog ? std::string{} : piece(ctx->vocab, token);
        if (!eog && !text.empty() && ctx->state.can_append(text)) return true;
        if (ctx->debug && rejected < ctx->debug_limit) {
            const typed::Prefix failure_state = eog || text.empty() ? ctx->state :
                ctx->state.state_before_rejection_for_debug(text);
            const std::string shown = eog ? "<eog>" : printable(text);
            const std::string partial = printable(failure_state.partial_for_debug());
            std::fprintf(stderr,
                "[typed step %zu] REJECT id=%d logit=%.3f piece=\"%s\" reason=%s expected=\"%s\" partial=\"%s\"\n",
                ctx->step + 1, token, candidates->data[i].logit, shown.c_str(),
                eog ? "premature end" : text.empty() ? "empty piece" : "no typed completion",
                failure_state.expected_for_debug().c_str(), partial.c_str());
        }
        ++rejected;
        return false;
    });

    if (winner == candidates->size) {
        // Ranking skips non-finite logits. Retry every candidate so a Metal/NaN
        // glitch does not look like an empty typed vocabulary.
        for (size_t i = 0; i < candidates->size; ++i) {
            const llama_token token = candidates->data[i].id;
            if (llama_vocab_is_eog(ctx->vocab, token)) continue;
            const std::string text = piece(ctx->vocab, token);
            if (!text.empty() && ctx->state.can_append(text)) {
                winner = i;
                break;
            }
        }
    }
    if (winner == candidates->size) {
        // Soft-fail: exit 2 so the harness records an unfinished attempt and continues.
        std::fputs("typed sampler: no viable candidate in vocabulary\n", stderr);
        std::_Exit(2);
    }
    if (ctx->debug) {
        if (rejected > ctx->debug_limit) {
            std::fprintf(stderr, "[typed step %zu] ... %zu more rejected candidates\n",
                ctx->step + 1, rejected - ctx->debug_limit);
        }
        std::fprintf(stderr, "[typed step %zu] SELECT id=%d logit=%.3f piece=\"%s\" rejected=%zu\n",
            ctx->step + 1, candidates->data[winner].id, candidates->data[winner].logit,
            printable(piece(ctx->vocab, candidates->data[winner].id)).c_str(), rejected);
    }
    for (size_t i = 0; i < candidates->size; ++i) {
        if (i != winner) candidates->data[i].logit = -std::numeric_limits<float>::infinity();
    }
    candidates->selected = static_cast<int32_t>(winner);
    candidates->sorted = false;
}

void reset(llama_sampler * sampler) {
    auto * ctx = static_cast<Context *>(sampler->ctx);
    ctx->state = ctx->initial;
    ctx->step = 0;
    ctx->selector.reset();
}

llama_sampler * clone(const llama_sampler * sampler);

void free_context(llama_sampler * sampler) { delete static_cast<Context *>(sampler->ctx); }

llama_sampler_i interface = {
    name, accept, apply, reset, clone, free_context,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

llama_sampler * clone(const llama_sampler * sampler) {
    return llama_sampler_init(&interface, new Context(*static_cast<const Context *>(sampler->ctx)));
}

} // namespace

llama_sampler * typed_sampler_init(const llama_vocab * vocab, const typed::Prefix & initial,
                                  bool debug, size_t debug_limit, float temperature, uint32_t seed) {
    return llama_sampler_init(&interface, new Context{
        vocab, initial, initial, debug, debug_limit, 0, sampling::Selector(temperature, seed) });
}
