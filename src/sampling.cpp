#include "sampling.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace sampling {

Selector::Selector(float temperature, uint32_t seed)
    : temperature_(temperature), seed_(seed), random_(seed) {
    if (!std::isfinite(temperature) || temperature < 0)
        throw std::invalid_argument("temperature must be finite and nonnegative");
}

size_t Selector::choose(const llama_token_data_array & candidates,
                        const std::function<bool(size_t)> & viable) {
    struct Ranked { size_t index; double score; };
    std::vector<Ranked> order;
    order.reserve(candidates.size);
    for (size_t i = 0; i < candidates.size; ++i) {
        const double logit = candidates.data[i].logit;
        if (!std::isfinite(logit)) continue;
        double score = logit;
        if (temperature_ > 0) {
            // Strictly inside (0, 1), so both logarithms are finite.
            const double uniform = (static_cast<double>(random_()) + 0.5) /
                                   (static_cast<double>(std::mt19937::max()) + 1.0);
            score -= temperature_ * std::log(-std::log(uniform));
        }
        order.push_back({i, score});
    }
    const auto lower = [](const Ranked & a, const Ranked & b) {
        return a.score == b.score ? a.index > b.index : a.score < b.score;
    };
    std::make_heap(order.begin(), order.end(), lower);
    while (!order.empty()) {
        std::pop_heap(order.begin(), order.end(), lower);
        const size_t index = order.back().index;
        order.pop_back();
        if (viable(index)) return index;
    }
    return candidates.size;
}

void Selector::reset() { random_.seed(seed_); }

namespace {
const char * name(const llama_sampler *) { return "temperature-sampling"; }
void apply(llama_sampler * sampler, llama_token_data_array * candidates) {
    auto * selector = static_cast<Selector *>(sampler->ctx);
    const size_t winner = selector->choose(*candidates, [](size_t) { return true; });
    if (winner == candidates->size) {
        std::fputs("sampler: no finite-logit candidate\n", stderr);
        std::abort();
    }
    candidates->selected = static_cast<int64_t>(winner);
}
void reset(llama_sampler * sampler) { static_cast<Selector *>(sampler->ctx)->reset(); }
void free_context(llama_sampler * sampler) { delete static_cast<Selector *>(sampler->ctx); }
llama_sampler * clone(const llama_sampler * sampler);
llama_sampler_i interface = {
    name, nullptr, apply, reset, clone, free_context,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
llama_sampler * clone(const llama_sampler * sampler) {
    return llama_sampler_init(&interface, new Selector(*static_cast<const Selector *>(sampler->ctx)));
}
} // namespace

llama_sampler * sampler_init(float temperature, uint32_t seed) {
    return llama_sampler_init(&interface, new Selector(temperature, seed));
}

} // namespace sampling
