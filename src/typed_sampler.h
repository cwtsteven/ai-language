#pragma once

#include "typed_prefix.h"
#include "llama.h"
#include <cstddef>

// Temperature sampling conditioned on the entire token piece keeping the typed
// prefix viable. A zero temperature selects the highest-logit viable token.
llama_sampler * typed_sampler_init(const llama_vocab * vocab, const typed::Prefix & initial = {},
                                  bool debug = false, size_t debug_limit = 20,
                                  float temperature = 0.8f, uint32_t seed = 0);
