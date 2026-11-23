#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace typed {

// Format a complete, well-typed program. Reject invalid or unfinished input.
std::optional<std::string> format_program(std::string_view source);

} // namespace typed
