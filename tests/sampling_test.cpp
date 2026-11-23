#include "sampling.h"
#include "typed_prefix.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

static llama_token next(llama_sampler * sampler) {
    llama_token_data tokens[] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
    llama_token_data_array candidates{tokens, 4, -1, false};
    llama_sampler_apply(sampler, &candidates);
    CHECK(candidates.selected >= 0 && candidates.selected < 4);
    const auto token = candidates.data[candidates.selected].id;
    llama_sampler_accept(sampler, token);
    return token;
}

int main() {
    typed::Prefix prefix;
    CHECK(prefix.append("main(): Int64 { return "));
    const char * pieces[] = {"false; }", "0; }", "1; }", ""};
    llama_token_data tokens[] = {{0, 100, 0}, {1, 0, 0}, {2, std::log(3.0f), 0}, {3, 100, 0}};
    const llama_token_data_array candidates{tokens, 4, -1, false};
    const auto viable = [&](size_t i) {
        return pieces[i][0] && prefix.can_append(pieces[i]);
    };

    // A dominant ill-typed token and an empty piece cannot absorb probability
    // mass. Among valid returns, the expected probabilities are 1/4 and 3/4.
    sampling::Selector selector(1.0f, 42);
    int ones = 0;
    constexpr int trials = 20000;
    for (int i = 0; i < trials; ++i) {
        const auto winner = selector.choose(candidates, viable);
        CHECK(winner == 1 || winner == 2);
        ones += winner == 2;
        typed::Prefix emitted = prefix;
        CHECK(emitted.append(pieces[winner]) && emitted.can_end());
    }
    CHECK(std::abs(ones / double(trials) - 0.75) < 0.02);
    CHECK(!prefix.can_end() && prefix.can_append("0; }") && prefix.can_append("1; }"));

    sampling::Selector warmer(2.0f, 42);
    ones = 0;
    for (int i = 0; i < trials; ++i) ones += warmer.choose(candidates, viable) == 2;
    const double expected = std::sqrt(3.0) / (1 + std::sqrt(3.0));
    CHECK(std::abs(ones / double(trials) - expected) < 0.02);

    for (uint32_t seed : {0u, 42u, 4294967295u}) {
        sampling::Selector greedy(0, seed);
        for (int i = 0; i < 20; ++i) CHECK(greedy.choose(candidates, viable) == 2);
    }
    CHECK(selector.choose(candidates, [](size_t) { return false; }) == candidates.size);
    llama_token_data masked[] = {{0, -std::numeric_limits<float>::infinity(), 0}, {1, 0, 0}};
    const llama_token_data_array sparse{masked, 2, -1, true};
    CHECK(selector.choose(sparse, [](size_t) { return true; }) == 1);
    masked[1].logit = -std::numeric_limits<float>::infinity();
    CHECK(selector.choose(sparse, [](size_t) { return true; }) == sparse.size);
    for (float invalid : {-1.0f, std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected = false;
        try { sampling::Selector bad(invalid, 42); }
        catch (const std::invalid_argument &) { rejected = true; }
        CHECK(rejected);
    }

    // Exercise the llama sampler interface used by the planner and unconstrained
    // generation, including cloning the current stream and resetting its seed.
    auto * first = sampling::sampler_init(0.8f, 42);
    auto * same = sampling::sampler_init(0.8f, 42);
    auto * different = sampling::sampler_init(0.8f, 43);
    std::vector<llama_token> sequence;
    bool differs = false;
    for (int i = 0; i < 64; ++i) {
        const auto token = next(first);
        sequence.push_back(token);
        CHECK(token == next(same));
        differs |= token != next(different);
    }
    CHECK(differs);
    auto * cloned = llama_sampler_clone(first);
    for (int i = 0; i < 64; ++i) CHECK(next(first) == next(cloned));
    llama_sampler_reset(first);
    for (const auto token : sequence) CHECK(next(first) == token);
    llama_sampler_free(first);
    llama_sampler_free(same);
    llama_sampler_free(different);
    llama_sampler_free(cloned);
    std::puts("Sampling tests passed");
}
