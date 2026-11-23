#include "typed_prefix.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace typed {
namespace {

struct Operation {
    const char * name;
    Type result;
    std::array<Type, 2> arguments;
    size_t arity;
};

constexpr Operation operations[] = {
    { "add",     Type::Int,     { Type::Int,     Type::Int }, 2 },
    { "sub",     Type::Int,     { Type::Int,     Type::Int }, 2 },
    { "mul",     Type::Int,     { Type::Int,     Type::Int }, 2 },
    { "eq",      Type::Bool,    { Type::Int,     Type::Int }, 2 },
    { "lt",      Type::Bool,    { Type::Int,     Type::Int }, 2 },
    { "not",     Type::Bool,    { Type::Bool,    Type::Bool }, 1 },
    { "and",     Type::Bool,    { Type::Bool,    Type::Bool }, 2 },
    { "or",      Type::Bool,    { Type::Bool,    Type::Bool }, 2 },
    { "refInt",  Type::RefInt,  { Type::Int,     Type::Int }, 1 },
    { "refBool", Type::RefBool, { Type::Bool,    Type::Bool }, 1 },
    { "readInt", Type::Int,     { Type::RefInt,  Type::Int }, 1 },
    { "readBool",Type::Bool,    { Type::RefBool, Type::Bool }, 1 },
};

struct BinaryOperation {
    const char * name;
    Type left;
    Type right;
    Type result;
};

constexpr BinaryOperation binary_operations[] = {
    { "+", Type::Int, Type::Int, Type::Int },
    { "-", Type::Int, Type::Int, Type::Int },
    { "*", Type::Int, Type::Int, Type::Int },
    { "<", Type::Int, Type::Int, Type::Bool },
    { "==", Type::Int, Type::Int, Type::Bool },
    { "==", Type::Bool, Type::Bool, Type::Bool },
    { "&&", Type::Bool, Type::Bool, Type::Bool },
    { "||", Type::Bool, Type::Bool, Type::Bool },
};

bool starts_with(std::string_view word, std::string_view prefix) {
    return word.size() >= prefix.size() && word.substr(0, prefix.size()) == prefix;
}

bool one_of(std::string_view partial, std::initializer_list<std::string_view> words) {
    for (auto word : words) {
        if (starts_with(word, partial)) return true;
    }
    return false;
}

bool identifier_prefix(std::string_view s) {
    if (s.empty() || !(s[0] >= 'a' && s[0] <= 'z')) return false;
    for (char ch : s.substr(1)) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

bool integer_prefix(std::string_view s) {
    if (s.empty()) return false;
    const bool negative = s[0] == '-';
    if (negative) s.remove_prefix(1);
    for (char ch : s) if (ch < '0' || ch > '9') return false;
    // Leading zeroes do not change the value, even before the literal ends.
    while (s.size() > 1 && s.front() == '0') s.remove_prefix(1);
    const std::string_view limit = negative ? "9223372036854775808" : "9223372036854775807";
    return s.size() < limit.size() || (s.size() == limit.size() && s <= limit);
}

bool integer_complete(std::string_view s) {
    return integer_prefix(s) && s != "-";
}

const Operation * operation(std::string_view name) {
    for (const auto & op : operations) if (op.name == name) return &op;
    return nullptr;
}

bool reserved(std::string_view name) {
    if (one_of(name, { "main", "Int64", "Bool", "RefInt64", "RefBool", "let", "var",
                       "if", "else", "while", "return", "write", "true", "false" })) {
        // one_of is a prefix test; only exact keywords are reserved.
        for (auto keyword : { "main", "Int64", "Bool", "RefInt64", "RefBool", "let", "var",
                              "if", "else", "while", "return", "write", "true", "false" }) {
            if (name == keyword) return true;
        }
    }
    return operation(name) != nullptr;
}

} // namespace

const Symbol * Prefix::find(std::string_view name) const {
    for (auto it = symbols_.rbegin(); it != symbols_.rend(); ++it) {
        if (it->name == name) return &*it;
    }
    return nullptr;
}

bool Prefix::name_available(std::string_view name) const {
    return identifier_prefix(name) && !reserved(name) && find(name) == nullptr;
}

bool Prefix::symbol_prefix(Type type, std::string_view partial) const {
    for (const auto & symbol : symbols_) {
        if (symbol.type == type && starts_with(symbol.name, partial)) return true;
    }
    return false;
}

bool Prefix::mutable_symbol_prefix(std::string_view partial) const {
    for (const auto & symbol : symbols_) {
        if (symbol.mutable_binding && starts_with(symbol.name, partial)) return true;
    }
    return false;
}

bool Prefix::has_atom(Type type) const {
    return type == Type::Int || type == Type::Bool || symbol_prefix(type, "");
}

bool Prefix::has_reference() const {
    return has_atom(Type::RefInt) || has_atom(Type::RefBool);
}

bool Prefix::atom_possible(Type type, std::string_view partial) const {
    if (type == Type::Int && integer_prefix(partial)) return true;
    if (type == Type::Bool && one_of(partial, { "true", "false" })) return true;
    return symbol_prefix(type, partial);
}

bool Prefix::atom_complete(Type type, std::string_view token) const {
    if (type == Type::Int && integer_complete(token)) return true;
    if (type == Type::Bool && (token == "true" || token == "false")) return true;
    const auto * symbol = find(token);
    return symbol && symbol->type == type;
}

bool Prefix::operation_possible(Type result, std::string_view partial) const {
    for (const auto & op : operations) {
        if (op.result != result || !starts_with(op.name, partial)) continue;
        bool productive = true;
        for (size_t i = 0; i < op.arity; ++i) productive &= has_atom(op.arguments[i]);
        if (productive) return true;
    }
    return false;
}

bool Prefix::begin_operation(std::string_view name, Type result) {
    const auto * op = operation(name);
    if (!op || op->result != result || !operation_possible(result, name)) return false;
    operation_ = std::string(name);
    argument_ = 0;
    phase_ = Phase::CallOpen;
    return true;
}

bool Prefix::rhs_operator_possible(std::string_view partial) const {
    for (const auto & op : binary_operations) {
        if (op.left == rhs_lhs_type_ && op.result == new_type_ &&
            starts_with(op.name, partial) && has_atom(op.right)) return true;
    }
    return false;
}

bool Prefix::begin_rhs_operator(std::string_view token) {
    for (const auto & op : binary_operations) {
        if (op.name == token && op.left == rhs_lhs_type_ &&
            op.result == new_type_ && has_atom(op.right)) {
            rhs_right_type_ = op.right;
            phase_ = Phase::RhsRight;
            return true;
        }
    }
    return false;
}

bool Prefix::rhs_atom_possible(std::string_view partial) const {
    if (atom_possible(new_type_, partial)) return true;
    // A different left type is viable only if an operator can produce the
    // expected result and its right operand has at least one inhabitant.
    for (const auto & op : binary_operations) {
        if (op.result == new_type_ && has_atom(op.right) &&
            atom_possible(op.left, partial)) return true;
    }
    return false;
}

bool Prefix::close_block() {
    if (blocks_.size() <= 1) return false;
    const Frame frame = blocks_.back();
    blocks_.pop_back();
    symbols_.resize(frame.symbol_count);
    phase_ = frame.kind == Block::IfThen ? Phase::Else : Phase::Stmt;
    return true;
}

bool Prefix::possible(std::string_view p) const {
    switch (phase_) {
        case Phase::Main:        return one_of(p, { "main" });
        case Phase::OpenParen:   return one_of(p, { "(" });
        case Phase::CloseParen:  return one_of(p, { ")" });
        case Phase::Colon:       return one_of(p, { ":" });
        case Phase::MainType:    return one_of(p, { "Int64" });
        case Phase::MainOpen:    return one_of(p, { "{" });
        case Phase::Stmt:
            return one_of(p, { "let", "var", "if", "while" }) ||
                (blocks_.size() == 1 && one_of(p, { "return" })) ||
                (blocks_.size() > 1 && one_of(p, { "}" })) ||
                mutable_symbol_prefix(p) ||
                (has_reference() && one_of(p, { "write" }));
        case Phase::LetName:     return identifier_prefix(p);
        case Phase::LetColon:    return one_of(p, { ":" });
        case Phase::LetType:     return one_of(p, { "Int64", "Bool", "RefInt64", "RefBool" });
        case Phase::LetEqual:    return one_of(p, { "=" });
        case Phase::Rhs:         return rhs_atom_possible(p) || operation_possible(new_type_, p);
        case Phase::RhsAfterAtom:
            return (rhs_lhs_type_ == new_type_ && one_of(p, { ";" })) ||
                rhs_operator_possible(p);
        case Phase::RhsRight:    return atom_possible(rhs_right_type_, p);
        case Phase::CallOpen:    return one_of(p, { "(" });
        case Phase::CallArg: {
            const auto * op = operation(operation_);
            return op && atom_possible(op->arguments[argument_], p);
        }
        case Phase::CallAfterArg: {
            const auto * op = operation(operation_);
            return op && one_of(p, argument_ + 1 < op->arity ?
                std::initializer_list<std::string_view>{ "," } :
                std::initializer_list<std::string_view>{ ")" });
        }
        case Phase::LetSemi:        return one_of(p, { ";" });
        case Phase::WriteOpen:      return one_of(p, { "(" });
        case Phase::WriteRef:       return symbol_prefix(Type::RefInt, p) || symbol_prefix(Type::RefBool, p);
        case Phase::WriteComma:     return one_of(p, { "," });
        case Phase::WriteValue:     return atom_possible(value_type_, p);
        case Phase::WriteClose:     return one_of(p, { ")" });
        case Phase::WriteSemi:      return one_of(p, { ";" });
        case Phase::IfOpenParen:    return one_of(p, { "(" });
        case Phase::IfCondition:    return atom_possible(Type::Bool, p);
        case Phase::IfCloseParen:   return one_of(p, { ")" });
        case Phase::IfOpen:         return one_of(p, { "{" });
        case Phase::Else:           return one_of(p, { "else" });
        case Phase::ElseOpen:       return one_of(p, { "{" });
        case Phase::WhileOpenParen: return one_of(p, { "(" });
        case Phase::WhileCondition: return atom_possible(Type::Bool, p);
        case Phase::WhileCloseParen:return one_of(p, { ")" });
        case Phase::WhileOpen:      return one_of(p, { "{" });
        case Phase::ReturnValue:    return atom_possible(Type::Int, p);
        case Phase::ReturnSemi:     return one_of(p, { ";" });
        case Phase::MainClose:      return one_of(p, { "}" });
        case Phase::Done:           return false;
    }
    return false;
}

bool Prefix::commit(std::string_view token) {
    if (!possible(token)) return false;
    switch (phase_) {
        case Phase::Main:        if (token != "main") return false; phase_ = Phase::OpenParen; return true;
        case Phase::OpenParen:   if (token != "(") return false; phase_ = Phase::CloseParen; return true;
        case Phase::CloseParen:  if (token != ")") return false; phase_ = Phase::Colon; return true;
        case Phase::Colon:       if (token != ":") return false; phase_ = Phase::MainType; return true;
        case Phase::MainType:    if (token != "Int64") return false; phase_ = Phase::MainOpen; return true;
        case Phase::MainOpen:
            if (token != "{") return false;
            blocks_.push_back({ Block::Main, symbols_.size() });
            phase_ = Phase::Stmt;
            return true;
        case Phase::Stmt:
            if (token == "let" || token == "var") {
                new_mutable_ = token == "var";
                assignment_ = false;
                phase_ = Phase::LetName;
                return true;
            }
            if (token == "if")     { phase_ = Phase::IfOpenParen; return true; }
            if (token == "write" && has_reference()) { phase_ = Phase::WriteOpen; return true; }
            if (token == "while") { phase_ = Phase::WhileOpenParen; return true; }
            if (token == "return" && blocks_.size() == 1) {
                phase_ = Phase::ReturnValue;
                return true;
            }
            if (token == "}" && blocks_.size() > 1) return close_block();
            if (const auto * symbol = find(token); symbol && symbol->mutable_binding) {
                new_name_ = std::string(token);
                new_type_ = symbol->type;
                assignment_ = true;
                phase_ = Phase::LetEqual;
                return true;
            }
            return false;
        case Phase::LetName:
            if (!name_available(token)) return false;
            new_name_ = std::string(token);
            phase_ = Phase::LetColon;
            return true;
        case Phase::LetColon: if (token != ":") return false; phase_ = Phase::LetType; return true;
        case Phase::LetType:
            if      (token == "Int64")    new_type_ = Type::Int;
            else if (token == "Bool")     new_type_ = Type::Bool;
            else if (token == "RefInt64") new_type_ = Type::RefInt;
            else if (token == "RefBool")  new_type_ = Type::RefBool;
            else return false;
            phase_ = Phase::LetEqual;
            return true;
        case Phase::LetEqual: if (token != "=") return false; phase_ = Phase::Rhs; return true;
        case Phase::Rhs:
            if (begin_operation(token, new_type_)) return true;
            if (atom_complete(new_type_, token)) rhs_lhs_type_ = new_type_;
            else {
                bool found = false;
                for (const auto & op : binary_operations) {
                    if (op.result == new_type_ && has_atom(op.right) && atom_complete(op.left, token)) {
                        rhs_lhs_type_ = op.left;
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            phase_ = Phase::RhsAfterAtom;
            return true;
        case Phase::RhsAfterAtom:
            if (token == ";" && rhs_lhs_type_ == new_type_) {
                if (!assignment_) symbols_.push_back({ new_name_, new_type_, new_mutable_ });
                phase_ = Phase::Stmt;
                return true;
            }
            return begin_rhs_operator(token);
        case Phase::RhsRight:
            if (!atom_complete(rhs_right_type_, token)) return false;
            phase_ = Phase::LetSemi;
            return true;
        case Phase::CallOpen: if (token != "(") return false; phase_ = Phase::CallArg; return true;
        case Phase::CallArg: {
            const auto * op = operation(operation_);
            if (!op || !atom_complete(op->arguments[argument_], token)) return false;
            phase_ = Phase::CallAfterArg;
            return true;
        }
        case Phase::CallAfterArg: {
            const auto * op = operation(operation_);
            if (!op) return false;
            if (argument_ + 1 < op->arity && token == ",") {
                ++argument_;
                phase_ = Phase::CallArg;
                return true;
            }
            if (argument_ + 1 == op->arity && token == ")") {
                phase_ = Phase::LetSemi;
                return true;
            }
            return false;
        }
        case Phase::LetSemi:
            if (token != ";") return false;
            if (!assignment_) symbols_.push_back({ new_name_, new_type_, new_mutable_ });
            phase_ = Phase::Stmt;
            return true;
        case Phase::WriteOpen: if (token != "(") return false; phase_ = Phase::WriteRef; return true;
        case Phase::WriteRef: {
            const auto * s = find(token);
            if (!s || (s->type != Type::RefInt && s->type != Type::RefBool)) return false;
            value_type_ = s->type == Type::RefInt ? Type::Int : Type::Bool;
            phase_ = Phase::WriteComma;
            return true;
        }
        case Phase::WriteComma: if (token != ",") return false; phase_ = Phase::WriteValue; return true;
        case Phase::WriteValue:
            if (!atom_complete(value_type_, token)) return false;
            phase_ = Phase::WriteClose;
            return true;
        case Phase::WriteClose: if (token != ")") return false; phase_ = Phase::WriteSemi; return true;
        case Phase::WriteSemi: if (token != ";") return false; phase_ = Phase::Stmt; return true;
        case Phase::IfOpenParen:
            if (token != "(") return false;
            phase_ = Phase::IfCondition;
            return true;
        case Phase::IfCondition:
            if (!atom_complete(Type::Bool, token)) return false;
            phase_ = Phase::IfCloseParen;
            return true;
        case Phase::IfCloseParen: if (token != ")") return false; phase_ = Phase::IfOpen; return true;
        case Phase::IfOpen:
            if (token != "{") return false;
            blocks_.push_back({ Block::IfThen, symbols_.size() });
            phase_ = Phase::Stmt;
            return true;
        case Phase::Else: if (token != "else") return false; phase_ = Phase::ElseOpen; return true;
        case Phase::ElseOpen:
            if (token != "{") return false;
            blocks_.push_back({ Block::IfElse, symbols_.size() });
            phase_ = Phase::Stmt;
            return true;
        case Phase::WhileOpenParen: if (token != "(") return false; phase_ = Phase::WhileCondition; return true;
        case Phase::WhileCondition:
            if (!atom_complete(Type::Bool, token)) return false;
            phase_ = Phase::WhileCloseParen;
            return true;
        case Phase::WhileCloseParen: if (token != ")") return false; phase_ = Phase::WhileOpen; return true;
        case Phase::WhileOpen:
            if (token != "{") return false;
            blocks_.push_back({ Block::While, symbols_.size() });
            phase_ = Phase::Stmt;
            return true;
        case Phase::ReturnValue:
            if (!atom_complete(Type::Int, token)) return false;
            phase_ = Phase::ReturnSemi;
            return true;
        case Phase::ReturnSemi: if (token != ";") return false; phase_ = Phase::MainClose; return true;
        case Phase::MainClose: if (token != "}") return false; phase_ = Phase::Done; return true;
        case Phase::Done: return false;
    }
    return false;
}

bool Prefix::step(unsigned char ch) {
    if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r') {
        if (pending_.empty()) return true;
        const std::string token = std::move(pending_);
        pending_.clear();
        return commit(token);
    }
    if (ch < 0x21 || ch > 0x7e) return false;
    if ((ch == '=' && pending_ == "=") ||
        (ch == '&' && pending_ == "&") ||
        (ch == '|' && pending_ == "|")) {
        const std::string token(2, static_cast<char>(ch));
        pending_.clear();
        return commit(token);
    }
    if (ch == '+' || ch == '-' || ch == '*' || ch == '<' ||
        ch == '=' || ch == '&' || ch == '|') {
        if (!pending_.empty()) {
            const std::string token = std::move(pending_);
            pending_.clear();
            if (!commit(token)) return false;
        }
        if ((ch == '=' || ch == '&' || ch == '|') && phase_ == Phase::RhsAfterAtom) {
            pending_.push_back(static_cast<char>(ch));
            return possible(pending_);
        }
        if (ch == '-' && (phase_ == Phase::Rhs || phase_ == Phase::RhsRight ||
                          phase_ == Phase::CallArg || phase_ == Phase::WriteValue ||
                          phase_ == Phase::ReturnValue)) {
            pending_ = "-";
            return possible(pending_);
        }
        return commit(std::string(1, static_cast<char>(ch)));
    }
    if (ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
        ch == ':' || ch == ';' || ch == ',') {
        if (!pending_.empty()) {
            const std::string token = std::move(pending_);
            pending_.clear();
            if (!commit(token)) return false;
        }
        return commit(std::string(1, static_cast<char>(ch)));
    }
    pending_.push_back(static_cast<char>(ch));
    return possible(pending_);
}

bool Prefix::append(std::string_view bytes) {
    Prefix next = *this;
    for (unsigned char ch : bytes) if (!next.step(ch)) return false;
    *this = std::move(next);
    return true;
}

bool Prefix::can_append(std::string_view bytes) const {
    Prefix next = *this;
    return next.append(bytes);
}

Prefix Prefix::state_before_rejection_for_debug(std::string_view bytes) const {
    Prefix state = *this;
    for (unsigned char ch : bytes) {
        Prefix before = state;
        if (!state.step(ch)) {
            // A punctuation byte may commit the preceding word successfully
            // and then fail in the new parser state.
            if (!before.pending_.empty() && state.pending_.empty() &&
                (state.phase_ != before.phase_ || state.blocks_.size() != before.blocks_.size() ||
                 state.symbols_.size() != before.symbols_.size())) return state;
            return before;
        }
    }
    return state;
}

bool Prefix::can_end() const {
    Prefix next = *this;
    if (!next.pending_.empty()) {
        const std::string token = std::move(next.pending_);
        next.pending_.clear();
        if (!next.commit(token)) return false;
    }
    return next.phase_ == Phase::Done;
}

std::string Prefix::completion_atom(Type type) const {
    if (type == Type::Int) {
        if (pending_.empty()) return "0";
        if (integer_prefix(pending_)) return pending_ == "-" ? "-0" : pending_;
    }
    if (type == Type::Bool) {
        for (auto literal : { "false", "true" }) {
            if (starts_with(literal, pending_)) return literal;
        }
    }
    for (const auto & symbol : symbols_) {
        if (symbol.type == type && starts_with(symbol.name, pending_)) return symbol.name;
    }
    return {};
}

std::string Prefix::completion_token() const {
    const auto fixed = [this](std::initializer_list<std::string_view> options) -> std::string {
        for (auto option : options) {
            if (starts_with(option, pending_)) return std::string(option);
        }
        return {};
    };
    switch (phase_) {
        case Phase::Main:        return fixed({ "main" });
        case Phase::OpenParen:   return fixed({ "(" });
        case Phase::CloseParen:  return fixed({ ")" });
        case Phase::Colon:       return fixed({ ":" });
        case Phase::MainType:    return fixed({ "Int64" });
        case Phase::MainOpen:    return fixed({ "{" });
        case Phase::Stmt:
            if (blocks_.size() == 1) {
                const auto token = fixed({ "return" });
                if (!token.empty()) return token;
            } else {
                const auto token = fixed({ "}" });
                if (!token.empty()) return token;
            }
            if (const auto token = fixed({ "let", "var", "if", "while" }); !token.empty()) return token;
            if (has_reference()) {
                if (const auto token = fixed({ "write" }); !token.empty()) return token;
            }
            for (const auto & symbol : symbols_) {
                if (symbol.mutable_binding && starts_with(symbol.name, pending_)) return symbol.name;
            }
            return {};
        case Phase::LetName: {
            std::string name = pending_.empty() ? "x" : pending_;
            while (!name_available(name)) name += "x";
            return name;
        }
        case Phase::LetColon:    return fixed({ ":" });
        case Phase::LetType:     return fixed({ "Int64", "Bool", "RefInt64", "RefBool" });
        case Phase::LetEqual:    return fixed({ "=" });
        case Phase::Rhs: {
            if (const auto atom = completion_atom(new_type_); !atom.empty()) return atom;
            for (const auto & op : binary_operations) {
                if (op.result == new_type_ && has_atom(op.right)) {
                    if (const auto atom = completion_atom(op.left); !atom.empty()) return atom;
                }
            }
            for (const auto & op : operations) {
                if (op.result == new_type_ && operation_possible(new_type_, op.name) &&
                    starts_with(op.name, pending_)) return op.name;
            }
            return {};
        }
        case Phase::RhsAfterAtom:
            if (rhs_lhs_type_ == new_type_) {
                if (const auto token = fixed({ ";" }); !token.empty()) return token;
            }
            for (const auto & op : binary_operations) {
                if (op.left == rhs_lhs_type_ && op.result == new_type_ &&
                    has_atom(op.right) && starts_with(op.name, pending_)) return op.name;
            }
            return {};
        case Phase::RhsRight: return completion_atom(rhs_right_type_);
        case Phase::CallOpen:    return fixed({ "(" });
        case Phase::CallArg: {
            const auto * op = operation(operation_);
            return op ? completion_atom(op->arguments[argument_]) : std::string{};
        }
        case Phase::CallAfterArg: {
            const auto * op = operation(operation_);
            return op ? fixed({ argument_ + 1 < op->arity ? "," : ")" }) : std::string{};
        }
        case Phase::LetSemi:        return fixed({ ";" });
        case Phase::WriteOpen:      return fixed({ "(" });
        case Phase::WriteRef:
            for (const auto & symbol : symbols_) {
                if ((symbol.type == Type::RefInt || symbol.type == Type::RefBool) &&
                    starts_with(symbol.name, pending_)) return symbol.name;
            }
            return {};
        case Phase::WriteComma:     return fixed({ "," });
        case Phase::WriteValue:     return completion_atom(value_type_);
        case Phase::WriteClose:     return fixed({ ")" });
        case Phase::WriteSemi:      return fixed({ ";" });
        case Phase::IfOpenParen:    return fixed({ "(" });
        case Phase::IfCondition:    return completion_atom(Type::Bool);
        case Phase::IfCloseParen:   return fixed({ ")" });
        case Phase::IfOpen:         return fixed({ "{" });
        case Phase::Else:           return fixed({ "else" });
        case Phase::ElseOpen:       return fixed({ "{" });
        case Phase::WhileOpenParen: return fixed({ "(" });
        case Phase::WhileCondition: return completion_atom(Type::Bool);
        case Phase::WhileCloseParen:return fixed({ ")" });
        case Phase::WhileOpen:      return fixed({ "{" });
        case Phase::ReturnValue:    return completion_atom(Type::Int);
        case Phase::ReturnSemi:     return fixed({ ";" });
        case Phase::MainClose:      return fixed({ "}" });
        case Phase::Done:           return {};
    }
    return {};
}

std::optional<std::string> Prefix::completion() const {
    Prefix state = *this;
    std::string suffix;
    // Each step finishes one lexical token. Existing nested blocks shrink,
    // while the selected next statement is always a return or a close brace.
    const size_t limit = 64 + state.blocks_.size() * 16;
    for (size_t i = 0; i < limit; ++i) {
        if (state.can_end()) return suffix;
        const auto token = state.completion_token();
        if (token.empty() || !starts_with(token, state.pending_)) return std::nullopt;
        const std::string piece = token.substr(state.pending_.size()) + " ";
        if (!state.append(piece)) return std::nullopt;
        suffix += piece;
    }
    return std::nullopt;
}

std::string Prefix::expected_for_debug() const {
    const auto type_name = [](Type type) -> const char * {
        switch (type) {
            case Type::Int: return "Int64";
            case Type::Bool: return "Bool";
            case Type::RefInt: return "RefInt64";
            case Type::RefBool: return "RefBool";
        }
        return "?";
    };
    switch (phase_) {
        case Phase::MainType: return "Int64 main return type";
        case Phase::Stmt: return "statement";
        case Phase::LetName: return "fresh variable name";
        case Phase::Rhs: return std::string(type_name(new_type_)) + " RHS";
        case Phase::RhsRight: return std::string(type_name(rhs_right_type_)) + " right operand";
        case Phase::RhsAfterAtom:
            return std::string("operator from ") + type_name(rhs_lhs_type_) + " to " +
                type_name(new_type_) + (rhs_lhs_type_ == new_type_ ? " or semicolon" : "");
        case Phase::CallArg: {
            const auto * op = operation(operation_);
            return op ? std::string(type_name(op->arguments[argument_])) + " argument" : "argument";
        }
        case Phase::WriteRef: return "reference variable";
        case Phase::WriteValue: return std::string(type_name(value_type_)) + " value";
        case Phase::IfCondition: return "Bool condition";
        case Phase::WhileCondition: return "Bool condition";
        case Phase::ReturnValue: return "Int64 return atom";
        case Phase::Done: return "end of program";
        default: return "grammar token";
    }
}

} // namespace typed
