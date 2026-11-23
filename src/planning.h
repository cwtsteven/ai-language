#pragma once

#include "language_prompt.h"

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace planning {

struct Request {
    std::string system;
    std::string user;
};

struct Plan {
    std::string text;
    bool complete = false;
};

struct Prepared {
    std::string system;
    std::string user;
    std::optional<Plan> plan;
};

// The callback generates unrestricted text, independently of the typed sampler.
using UnconstrainedGenerator = std::function<Plan(const Request &)>;

// Re-evaluate the request plus recent output when the finite model context fills.
// The complete plan text is retained separately and is never truncated here.
template <typename Token>
std::vector<Token> replay_tokens(const std::vector<Token> & prompt, std::vector<Token> & history,
                                 size_t context_size) {
    if (context_size <= prompt.size() + 1)
        throw std::runtime_error("planning prompt leaves no room for generation");
    const size_t keep = (context_size - prompt.size()) / 2;
    if (history.size() > keep) history.erase(history.begin(), history.end() - keep);
    auto replay = prompt;
    replay.insert(replay.end(), history.begin(), history.end());
    return replay;
}

inline Prepared prepare(const std::string & task, const std::string & prefix, bool enabled,
                        const UnconstrainedGenerator & generate) {
    Prepared prepared{ language::system_prompt(), "Produce a program for this request: " + task, std::nullopt };
    if (!enabled) return prepared;

    Request request{ language::planning_prompt(), "Plan the implementation for this request: " + task };
    if (!prefix.empty()) request.user += "\n\nExisting code prefix to continue:\n" + prefix;
    auto plan = generate(request);
    if (!plan.complete) throw std::runtime_error("planning ended without a complete response");
    if (plan.text.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("planning produced no implementation plan");
    prepared.user += "\n\nImplementation plan:\n" + plan.text +
        "\n\nUse this plan as guidance. Correct any conflict with the original request or language rules. Output code only.";
    prepared.plan = std::move(plan);
    return prepared;
}

} // namespace planning
