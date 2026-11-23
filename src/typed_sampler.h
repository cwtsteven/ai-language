#pragma once

#include "typed_prefix.h"
#include "llama.h"
#include <cstddef>

// Greedy constrained sampler: choose the highest-logit candidate whose entire
// token piece keeps the typed language prefix viable.
llama_sampler * typed_sampler_init(const llama_vocab * vocab, const typed::Prefix & initial = {},
                                  bool debug = false, size_t debug_limit = 20);
