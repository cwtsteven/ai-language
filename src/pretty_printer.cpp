#include "pretty_printer.h"
#include "typed_prefix.h"

#include <cctype>
#include <vector>

namespace typed {
namespace {

bool word_char(unsigned char ch) {
    return std::isalnum(ch) || ch == '_';
}

bool expects_atom(std::string_view previous) {
    return previous == "=" || previous == "(" || previous == "," || previous == "return" ||
        previous == "!" || previous == "[" || previous == "+" || previous == "-" ||
        previous == "*" || previous == "==" || previous == "<" || previous == "&&" ||
        previous == "||";
}

bool is_infix(std::string_view token) {
    return token == "+" || token == "-" || token == "*" || token == "==" ||
        token == "<" || token == "&&" || token == "||";
}

} // namespace

std::optional<std::string> format_program(std::string_view source) {
    Prefix checker;
    if (!checker.append(source) || !checker.can_end()) return std::nullopt;

    std::vector<std::string_view> tokens;
    for (size_t i = 0; i < source.size();) {
        if (std::isspace(static_cast<unsigned char>(source[i]))) { ++i; continue; }
        const size_t start = i++;
        const std::string_view previous = tokens.empty() ? std::string_view{} : tokens.back();
        if (source[start] == '"') {
            while (i < source.size() && source[i] != '"') ++i;
            if (i < source.size()) ++i;
        } else if (source[start] == '-' && expects_atom(previous) &&
                   i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) {
            while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) ++i;
        } else if (source[start] == '=' || source[start] == '&' || source[start] == '|') {
            if (i < source.size() && source[i] == source[start]) ++i;
        } else if (word_char(source[start])) {
            while (i < source.size() && word_char(source[i])) ++i;
        }
        tokens.push_back(source.substr(start, i - start));
    }

    std::string output;
    std::string line;
    size_t depth = 0;
    const auto space = [&]() {
        if (!line.empty() && line.back() != ' ') line += ' ';
    };
    const auto flush = [&]() {
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (!line.empty()) {
            output.append(depth * 4, ' ');
            output += line + '\n';
            line.clear();
        }
    };
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto token = tokens[i];
        const auto previous = i ? tokens[i - 1] : std::string_view{};
        if (token == "{") {
            space();
            line += '{';
            flush();
            ++depth;
        } else if (token == "}") {
            flush();
            --depth;
            line = "}";
            if (i + 1 == tokens.size() || tokens[i + 1] != "else") flush();
        } else if (token == ";") {
            line += ';';
            flush();
        } else if (token == "(") {
            if (previous == "if" || previous == "while") space();
            line += '(';
        } else if (token == ")") {
            line += ')';
        } else if (token == "[" || token == "]") {
            line += token;
        } else if (token == ":" || token == ",") {
            line += token;
            line += ' ';
        } else if (token == "=") {
            space();
            line += token;
            line += ' ';
        } else if (is_infix(token)) {
            space();
            line += token;
            line += ' ';
        } else if (token == "!") {
            space();
            line += token;
        } else {
            if (previous != "(" && previous != "!" && previous != "[") space();
            line += token;
        }
    }
    flush();
    return output;
}

} // namespace typed
