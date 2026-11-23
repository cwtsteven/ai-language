#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace typed {

// Translate a complete, well-typed source program to standalone Cangjie.
// Invalid or unfinished input produces no output.
std::optional<std::string> generate_cangjie(std::string_view source);

} // namespace typed
