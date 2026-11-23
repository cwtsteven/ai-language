#pragma once

#include "llama.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>

namespace sampling {

// Sample from exp(logit / temperature), conditioned on viability. Gumbel
// priorities let us check candidates lazily without changing that distribution.
class Selector {
public:
    Selector(float temperature, uint32_t seed);
    size_t choose(const llama_token_data_array & candidates,
                  const std::function<bool(size_t)> & viable);
    void reset();

private:
    float temperature_;
    uint32_t seed_;
    std::mt19937 random_;
};

// Unconstrained sampling, with the same temperature and seed semantics.
llama_sampler * sampler_init(float temperature, uint32_t seed);

} // namespace sampling
