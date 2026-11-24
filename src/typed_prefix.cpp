#include "typed_prefix.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>

namespace typed {
namespace {

struct Operation {
    const char * name;
    Type result;
    std::array<Type, 2> arguments;
    size_t arity;
};

const Operation operations[] = {
    { "emptyList", Type::List(),  { Type::Int(),     Type::Int() }, 0 },
    { "listLen",   Type::Int(),   { Type::List(),    Type::Int() }, 1 },
    { "listPush",  Type::List(),  { Type::List(),    Type::Int() }, 2 },
    { "strLen",    Type::Int(),   { Type::String(),  Type::Int() }, 1 },
};

struct InfixOp {
    const char * text;
    Type left;
    Type right;
    Type result;
};

const InfixOp infix_ops[] = {
    { "+",  Type::Int(),    Type::Int(),    Type::Int() },
    { "-",  Type::Int(),    Type::Int(),    Type::Int() },
    { "*",  Type::Int(),    Type::Int(),    Type::Int() },
    { "==", Type::Int(),    Type::Int(),    Type::Bool() },
    { "<",  Type::Int(),    Type::Int(),    Type::Bool() },
    { "+",  Type::String(), Type::String(), Type::String() },
    { "==", Type::String(), Type::String(), Type::Bool() },
    { "&&", Type::Bool(),   Type::Bool(),   Type::Bool() },
    { "||", Type::Bool(),   Type::Bool(),   Type::Bool() },
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

bool type_parameter_prefix(std::string_view s) {
    if (s.empty() || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z'))) return false;
    for (size_t i = 1; i < s.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(s[i]);
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
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
    const bool negative = s.front() == '-';
    if (negative) s.remove_prefix(1);
    if (s.empty()) return negative;
    for (char ch : s) if (ch < '0' || ch > '9') return false;
    while (s.size() > 1 && s.front() == '0') s.remove_prefix(1);
    if (negative) {
        return s.size() < 19 || (s.size() == 19 && s <= "9223372036854775808");
    }
    return s.size() < 19 || (s.size() == 19 && s <= "9223372036854775807");
}

bool integer_complete(std::string_view s) {
    return integer_prefix(s) && s != "-";
}

bool string_literal_prefix(std::string_view s) {
    if (s.empty() || s.front() != '"') return false;
    for (size_t i = 1; i < s.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch == '"') return i + 1 == s.size();
        if (ch != ' ' && (ch < 0x21 || ch > 0x7e || ch == '\\')) return false;
    }
    return true;
}

bool string_literal_complete(std::string_view s) {
    return s.size() >= 2 && string_literal_prefix(s) && s.back() == '"';
}

const Operation * operation(std::string_view name) {
    for (const auto & op : operations) if (op.name == name) return &op;
    return nullptr;
}

bool reserved(std::string_view name) {
    for (auto keyword : { "main", "func", "fun", "Int64", "Bool", "ListInt64", "String", "let", "var",
                          "if", "else", "while", "break", "continue", "return", "assign", "write",
                          "readInt", "readBool", "refInt", "refBool", "true", "false" }) {
        if (name == keyword) return true;
    }
    return operation(name) != nullptr;
}

std::optional<Type> parse_base(std::string_view token) {
    if (token == "Int64") return Type::Int();
    if (token == "Bool") return Type::Bool();
    if (token == "ListInt64") return Type::List();
    if (token == "String") return Type::String();
    return std::nullopt;
}

bool word_char(unsigned char ch) {
    return std::isalnum(ch) || ch == '_';
}

bool is_single_punct(unsigned char ch) {
    return ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
           ch == ':' || ch == ';' || ch == ',' || ch == '+' || ch == '*' ||
           ch == '<' || ch == '>' || ch == '!' || ch == '[' || ch == ']';
}

std::string_view first_lexeme(std::string_view text) {
    if (text.empty()) return {};
    const unsigned char ch = static_cast<unsigned char>(text.front());
    if (ch < 0x21 || ch > 0x7e) return text.substr(0, 1);
    if (ch == '-') {
        size_t i = 1;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
        return text.substr(0, i);
    }
    if (ch == '=' || ch == '&' || ch == '|') {
        if (text.size() >= 2 && text[1] == ch) return text.substr(0, 2);
        return text.substr(0, 1);
    }
    if (is_single_punct(ch)) return text.substr(0, 1);
    if (word_char(ch)) {
        size_t i = 1;
        while (i < text.size() && word_char(static_cast<unsigned char>(text[i]))) ++i;
        return text.substr(0, i);
    }
    return text.substr(0, 1);
}

std::string display_lexeme(std::string_view lexeme) {
    if (lexeme.empty()) return "<end of input>";
    const unsigned char ch = static_cast<unsigned char>(lexeme.front());
    if (ch < 0x21 || ch > 0x7e) {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "\\x%02x", ch);
        return buffer;
    }
    return std::string(lexeme);
}

void advance_position(unsigned char ch, size_t & offset, size_t & line, size_t & column) {
    ++offset;
    if (ch == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }
}

bool looks_like_integer(std::string_view token) {
    if (token.empty()) return false;
    size_t i = token[0] == '-' ? 1 : 0;
    if (i >= token.size()) return false;
    for (; i < token.size(); ++i) {
        if (token[i] < '0' || token[i] > '9') return false;
    }
    return true;
}

const InfixOp * find_infix(std::string_view text, Type left, Type needed) {
    for (const auto & op : infix_ops) {
        if (op.text == text && op.left == left && op.result == needed) return &op;
    }
    return nullptr;
}

bool infix_prefix(Type left, Type needed, std::string_view partial) {
    for (const auto & op : infix_ops) {
        if (op.left == left && op.result == needed && starts_with(op.text, partial)) return true;
    }
    return false;
}

} // namespace

Type Type::Int() { return Type{Kind::Int, {}, {}, nullptr}; }
Type Type::Bool() { return Type{Kind::Bool, {}, {}, nullptr}; }
Type Type::List() { return Type{Kind::List, {}, {}, nullptr}; }
Type Type::String() { return Type{Kind::String, {}, {}, nullptr}; }
Type Type::Param(std::string name) { return Type{Kind::Param, std::move(name), {}, nullptr}; }
Type Type::Fun(std::vector<Type> parameters, Type result) {
    return Type{Kind::Fun, {}, std::move(parameters), std::make_shared<Type>(std::move(result))};
}

bool Type::operator==(const Type & other) const {
    if (kind != other.kind || param != other.param || parameters != other.parameters) return false;
    if (static_cast<bool>(result) != static_cast<bool>(other.result)) return false;
    return !result || *result == *other.result;
}

std::string type_name(const Type & type) {
    switch (type.kind) {
        case Type::Kind::Int: return "Int64";
        case Type::Kind::Bool: return "Bool";
        case Type::Kind::List: return "ListInt64";
        case Type::Kind::String: return "String";
        case Type::Kind::Param: return type.param;
        case Type::Kind::Fun: {
            std::string text = "(";
            for (size_t i = 0; i < type.parameters.size(); ++i) {
                if (i) text += ", ";
                text += type_name(type.parameters[i]);
            }
            text += ") -> ";
            return text + (type.result ? type_name(*type.result) : "?");
        }
    }
    return "?";
}

const Symbol * Prefix::find(std::string_view name) const {
    for (auto it = symbols_.rbegin(); it != symbols_.rend(); ++it) {
        if (it->name == name) return &*it;
    }
    return nullptr;
}

bool Prefix::name_available(std::string_view name) const {
    return identifier_prefix(name) && !reserved(name) && find(name) == nullptr &&
        find_function(name) == nullptr && name != declaring_.name;
}

const Prefix::Function * Prefix::find_function(std::string_view name) const {
    for (const auto & function : functions_) if (function.name == name) return &function;
    return nullptr;
}

bool Prefix::mutable_prefix(std::string_view partial) const {
    for (const auto & symbol : symbols_) {
        if (symbol.mutable_binding && starts_with(symbol.name, partial)) return true;
    }
    return false;
}

bool Prefix::symbol_prefix(Type type, std::string_view partial) const {
    for (const auto & symbol : symbols_) {
        if (symbol.type == type && starts_with(symbol.name, partial)) return true;
    }
    return false;
}

bool Prefix::has_atom(Type type) const {
    if (type.kind == Type::Kind::Int || type.kind == Type::Kind::Bool ||
        type.kind == Type::Kind::String) return true;
    if (type.kind == Type::Kind::Fun) return type.result && has_atom(*type.result);
    return symbol_prefix(type, "");
}

bool Prefix::atom_possible(Type type, std::string_view partial) const {
    if (type.kind == Type::Kind::Int && integer_prefix(partial)) return true;
    if (type.kind == Type::Kind::Bool && one_of(partial, { "true", "false" })) return true;
    if (type.kind == Type::Kind::String && string_literal_prefix(partial)) return true;
    if (type.kind == Type::Kind::Fun && has_atom(type) && one_of(partial, { "fun" })) return true;
    if (type.kind == Type::Kind::Fun) {
        for (const auto & function : functions_) {
            if (!function.type_params.empty() || function_type(function) != type) continue;
            if (starts_with(function.name, partial)) return true;
        }
    }
    return symbol_prefix(type, partial);
}

bool Prefix::atom_complete(Type type, std::string_view token) const {
    if (type.kind == Type::Kind::Int && integer_complete(token)) return true;
    if (type.kind == Type::Kind::Bool && (token == "true" || token == "false")) return true;
    if (type.kind == Type::Kind::String && string_literal_complete(token)) return true;
    if (type.kind == Type::Kind::Fun) {
        if (const auto * function = find_function(token);
            function && function->type_params.empty() && function_type(*function) == type) return true;
    }
    const auto * symbol = find(token);
    return symbol && symbol->type == type;
}

bool Prefix::left_can_reach(Type left, Type needed) const {
    if (left == needed) return true;
    if (index_possible(left, needed)) return true;
    return infix_possible(left, needed, "");
}

bool Prefix::index_possible(Type left, Type needed) const {
    if (left == Type::List() && needed == Type::Int()) return true;
    if (left == Type::String() && needed == Type::String()) return true;
    return false;
}

bool Prefix::infix_possible(Type left, Type needed, std::string_view partial) const {
    return infix_prefix(left, needed, partial);
}

bool Prefix::operation_possible(Type result, std::string_view partial) const {
    for (const auto & op : operations) {
        if (op.result != result || !starts_with(op.name, partial)) continue;
        bool productive = true;
        for (size_t i = 0; i < op.arity; ++i) productive &= has_atom(op.arguments[i]);
        if (productive) return true;
    }
    for (const auto & function : functions_) {
        if (!starts_with(function.name, partial)) continue;
        if (function.type_params.empty()) {
            if (function.result == result &&
                std::all_of(function.arguments.begin(), function.arguments.end(),
                            [this](const Type & type) { return has_atom(type); })) return true;
            continue;
        }
        if (instantiate_possible(function, result)) return true;
    }
    return false;
}

bool Prefix::begin_operation(std::string_view name, Type result) {
    if (!operation_possible(result, name)) return false;
    if (const auto * op = operation(name); op && op->result == result) {
        call_arguments_.assign(op->arguments.begin(), op->arguments.begin() + op->arity);
    } else if (const auto * function = find_function(name)) {
        call_name_ = function->name;
        if (!function->type_params.empty()) {
            call_type_args_.clear();
            type_dest_ = TypeDest::CallArg;
            phase_ = Phase::CallTypeOpen;
            return true;
        }
        if (function->result != result) return false;
        call_arguments_ = function->arguments;
    } else return false;
    argument_ = 0;
    phase_ = Phase::CallOpen;
    return true;
}

bool Prefix::begin_expression(Type needed, ExprFollow follow) {
    expr_needed_ = needed;
    expr_follow_ = follow;
    phase_ = Phase::ExprStart;
    return true;
}

bool Prefix::finish_expression() {
    switch (expr_follow_) {
        case ExprFollow::LetSemi: phase_ = Phase::LetSemi; return true;
        case ExprFollow::ReturnSemi: phase_ = Phase::ReturnSemi; return true;
        case ExprFollow::IfClose: phase_ = Phase::IfCloseParen; return true;
        case ExprFollow::WhileClose: phase_ = Phase::WhileCloseParen; return true;
        case ExprFollow::LambdaEnd: {
            symbols_.resize(lambda_base_);
            if (lambdas_.empty()) return false;
            const LambdaFrame frame = std::move(lambdas_.back());
            lambdas_.pop_back();
            new_name_ = frame.name;
            expr_follow_ = frame.follow;
            expr_needed_ = frame.expr_needed;
            saved_needed_ = frame.signature;
            lambda_base_ = frame.base;
            lambda_param_ = frame.param;
            return finish_expression();
        }
    }
    return false;
}

Type Prefix::function_type(const Function & function) const {
    return Type::Fun(function.arguments, function.result);
}

bool Prefix::type_param_named(std::string_view name) const {
    return std::find(type_params_.begin(), type_params_.end(), name) != type_params_.end();
}

std::optional<Type> Prefix::base_or_param(std::string_view token) const {
    if (const auto base = parse_base(token)) return base;
    if (type_param_named(token)) return Type::Param(std::string(token));
    return std::nullopt;
}

Type Prefix::substitute(const Type & type, const std::vector<Type> & args) const {
    if (type.kind == Type::Kind::Param) {
        const auto * function = find_function(call_name_);
        if (!function) return type;
        for (size_t i = 0; i < function->type_params.size() && i < args.size(); ++i) {
            if (function->type_params[i] == type.param) return args[i];
        }
        return type;
    }
    if (type.kind != Type::Kind::Fun) return type;
    std::vector<Type> parameters;
    for (const auto & parameter : type.parameters) parameters.push_back(substitute(parameter, args));
    return Type::Fun(std::move(parameters), substitute(*type.result, args));
}

bool Prefix::instantiate_possible(const Function & function, const Type & needed) const {
    std::vector<Type> choices{ Type::Int(), Type::Bool(), Type::List(), Type::String() };
    for (const auto & name : type_params_) choices.push_back(Type::Param(name));
    const size_t slots = function.type_params.size();
    if (slots == 0) return false;
    std::vector<size_t> index(slots, 0);
    const auto apply = [&](const auto & self, const Type & type, const std::vector<Type> & args) -> Type {
        if (type.kind == Type::Kind::Param) {
            for (size_t i = 0; i < slots; ++i)
                if (function.type_params[i] == type.param) return args[i];
            return type;
        }
        if (type.kind != Type::Kind::Fun) return type;
        std::vector<Type> parameters;
        for (const auto & parameter : type.parameters) parameters.push_back(self(self, parameter, args));
        return Type::Fun(std::move(parameters), self(self, *type.result, args));
    };
    while (true) {
        std::vector<Type> args;
        for (size_t slot : index) args.push_back(choices[slot]);
        bool productive = apply(apply, function.result, args) == needed;
        for (const auto & argument : function.arguments)
            productive = productive && has_atom(apply(apply, argument, args));
        if (productive) return true;
        size_t cursor = 0;
        while (cursor < slots) {
            if (++index[cursor] < choices.size()) break;
            index[cursor++] = 0;
        }
        if (cursor == slots) return false;
    }
}

bool Prefix::begin_type(TypeDest dest) {
    type_dest_ = dest;
    type_stack_.clear();
    type_at_result_ = false;
    phase_ = Phase::TypeAtom;
    return true;
}

bool Prefix::finish_type(const Type & type) {
    switch (type_dest_) {
        case TypeDest::Let:
            new_type_ = type;
            phase_ = Phase::LetEqual;
            return true;
        case TypeDest::Param:
            symbols_.push_back({ new_name_, type });
            declaring_.arguments.push_back(type);
            phase_ = Phase::AfterParam;
            return true;
        case TypeDest::Return:
            declaring_.result = type;
            return_type_ = type;
            functions_.push_back(declaring_);
            phase_ = Phase::FunctionOpen;
            return true;
        case TypeDest::LambdaParam:
            if (lambda_param_ >= saved_needed_.parameters.size() ||
                type != saved_needed_.parameters[lambda_param_]) return false;
            symbols_.push_back({ new_name_, type });
            phase_ = Phase::LambdaAfterParam;
            return true;
        case TypeDest::LambdaResult:
            if (!saved_needed_.result || type != *saved_needed_.result) return false;
            lambda_result_ = type;
            phase_ = Phase::LambdaEq;
            return true;
        case TypeDest::CallArg:
            call_type_args_.push_back(type);
            phase_ = Phase::CallTypeNext;
            return true;
    }
    return false;
}

bool Prefix::deliver_type(Type type) {
    if (!type_stack_.empty() && type_stack_.back().result) {
        auto build = std::move(type_stack_.back());
        type_stack_.pop_back();
        return deliver_type(Type::Fun(std::move(build.parameters), std::move(type)));
    }
    if (!type_stack_.empty()) {
        type_stack_.back().parameters.push_back(std::move(type));
        phase_ = Phase::TypeAfter;
        return true;
    }
    return finish_type(std::move(type));
}

bool Prefix::symbol_callable(const Symbol & symbol, Type needed) const {
    if (symbol.type.kind != Type::Kind::Fun || !symbol.type.result || *symbol.type.result != needed)
        return false;
    return std::all_of(symbol.type.parameters.begin(), symbol.type.parameters.end(),
                        [this](const Type & type) { return has_atom(type); });
}

bool Prefix::value_start_possible(Type needed, std::string_view partial) const {
    if (needed == Type::Bool() && one_of(partial, { "!" })) return true;
    if (needed.kind == Type::Kind::Fun && has_atom(needed) && one_of(partial, { "fun" })) return true;
    if (atom_possible(needed, partial)) return true;
    if (operation_possible(needed, partial)) return true;
    for (const auto & symbol : symbols_) {
        if (!starts_with(symbol.name, partial)) continue;
        if (left_can_reach(symbol.type, needed) || symbol_callable(symbol, needed)) return true;
    }
    if (needed != Type::Int() && integer_prefix(partial) && left_can_reach(Type::Int(), needed))
        return true;
    if (needed != Type::Bool() && one_of(partial, { "true", "false" }) &&
        left_can_reach(Type::Bool(), needed)) return true;
    if (needed != Type::String() && string_literal_prefix(partial) &&
        left_can_reach(Type::String(), needed)) return true;
    return false;
}

bool Prefix::close_block() {
    if (blocks_.empty()) return false;
    const Frame frame = blocks_.back();
    blocks_.pop_back();
    symbols_.resize(frame.symbol_count);
    if (frame.kind == Block::Main || frame.kind == Block::Function) {
        if (frame.kind == Block::Function) type_params_.clear();
        declaring_ = {};
        phase_ = frame.kind == Block::Main ? Phase::Done : Phase::TopLevel;
    } else phase_ = frame.kind == Block::IfThen ? Phase::Else : Phase::Stmt;
    return true;
}

bool Prefix::in_loop() const {
    return std::any_of(blocks_.begin(), blocks_.end(), [](const Frame & frame) {
        return frame.kind == Block::While;
    });
}

bool Prefix::possible(std::string_view p) const {
    switch (phase_) {
        case Phase::TopLevel:    return one_of(p, { "main", "func" });
        case Phase::FunctionName:
        case Phase::ParamName:
        case Phase::LambdaParamName: return identifier_prefix(p);
        case Phase::TypeParamName: return type_parameter_prefix(p);
        case Phase::FunctionGenericOrParen: return one_of(p, { "(", "<" });
        case Phase::TypeParamNext: return one_of(p, { ",", ">" });
        case Phase::FunctionParen:return one_of(p, { "(" });
        case Phase::ParamStart:  return one_of(p, { ")" }) || identifier_prefix(p);
        case Phase::ParamColon:
        case Phase::FunctionColon:return one_of(p, { ":" });
        case Phase::AfterParam:  return one_of(p, { ",", ")" });
        case Phase::FunctionOpen:return one_of(p, { "{" });
        case Phase::OpenParen:   return one_of(p, { "(" });
        case Phase::CloseParen:  return one_of(p, { ")" });
        case Phase::Colon:       return one_of(p, { ":" });
        case Phase::MainType:    return one_of(p, { "Int64" });
        case Phase::MainOpen:    return one_of(p, { "{" });
        case Phase::Stmt:
            return one_of(p, { "let", "var", "if", "while" }) ||
                mutable_prefix(p) ||
                (in_loop() && one_of(p, { "break", "continue" })) ||
                (has_atom(return_type_) && one_of(p, { "return" })) ||
                (!blocks_.empty() && one_of(p, { "}" }));
        case Phase::LetName:     return identifier_prefix(p);
        case Phase::LetColon:    return one_of(p, { ":" });
        case Phase::LetType:
        case Phase::ParamType:
        case Phase::FunctionType:
        case Phase::TypeAtom: {
            if (one_of(p, { "(", "Int64", "Bool", "ListInt64", "String" })) return true;
            for (const auto & name : type_params_) if (starts_with(name, p)) return true;
            return false;
        }
        case Phase::TypeAfter: return one_of(p, { ",", ")" });
        case Phase::TypeArrow: return one_of(p, { "-" });
        case Phase::TypeArrowGt: return one_of(p, { ">" });
        case Phase::LetEqual:
        case Phase::AssignEqual: return one_of(p, { "=" });
        case Phase::ExprStart:   return value_start_possible(expr_needed_, p);
        case Phase::ExprAfterLeft: {
            if (left_type_ == expr_needed_ &&
                one_of(p, expr_follow_ == ExprFollow::LetSemi || expr_follow_ == ExprFollow::ReturnSemi
                        ? std::initializer_list<std::string_view>{ ";" }
                        : std::initializer_list<std::string_view>{ ")" })) return true;
            if (index_possible(left_type_, expr_needed_) && one_of(p, { "[" })) return true;
            return infix_possible(left_type_, expr_needed_, p);
        }
        case Phase::InfixRight:  return atom_possible(infix_right_type_, p);
        case Phase::IndexArg:    return atom_possible(Type::Int(), p);
        case Phase::IndexClose:  return one_of(p, { "]" });
        case Phase::UnaryArg:    return atom_possible(Type::Bool(), p);
        case Phase::IfAtom:
        case Phase::WhileAtom:  return atom_possible(Type::Bool(), p);
        case Phase::CallTypeOpen: return one_of(p, { "<" });
        case Phase::CallTypeArg: return static_cast<bool>(base_or_param(p)) ||
            one_of(p, { "Int64", "Bool", "ListInt64", "String" }) ||
            [&] {
                for (const auto & name : type_params_) if (starts_with(name, p)) return true;
                return false;
            }();
        case Phase::CallTypeNext: return one_of(p, { ",", ">" });
        case Phase::LambdaParen: return one_of(p, { "(" });
        case Phase::LambdaParamStart:
            return (lambda_param_ >= saved_needed_.parameters.size() && one_of(p, { ")" })) ||
                (lambda_param_ < saved_needed_.parameters.size() && identifier_prefix(p));
        case Phase::LambdaParamColon:
        case Phase::LambdaColon: return one_of(p, { ":" });
        case Phase::LambdaAfterParam:
            return one_of(p, lambda_param_ + 1 < saved_needed_.parameters.size()
                ? std::initializer_list<std::string_view>{ "," }
                : std::initializer_list<std::string_view>{ ")" });
        case Phase::LambdaEq: return one_of(p, { "=" });
        case Phase::LambdaGt: return one_of(p, { ">" });
        case Phase::LambdaParamType:
        case Phase::LambdaResult: return false;
        case Phase::CallOpen:    return one_of(p, { "(" });
        case Phase::CallArg:
            return argument_ < call_arguments_.size() && atom_possible(call_arguments_[argument_], p);
        case Phase::CallAfterArg: {
            return one_of(p, argument_ + 1 < call_arguments_.size() ?
                std::initializer_list<std::string_view>{ "," } :
                std::initializer_list<std::string_view>{ ")" });
        }
        case Phase::CallClose:      return one_of(p, { ")" });
        case Phase::LetSemi:        return one_of(p, { ";" });
        case Phase::LoopControlSemi:return one_of(p, { ";" });
        case Phase::IfOpenParen:    return one_of(p, { "(" });
        case Phase::IfCloseParen:   return one_of(p, { ")" });
        case Phase::IfOpen:         return one_of(p, { "{" });
        case Phase::Else:           return one_of(p, { "else" });
        case Phase::ElseOpen:       return one_of(p, { "{" });
        case Phase::WhileOpenParen: return one_of(p, { "(" });
        case Phase::WhileCloseParen:return one_of(p, { ")" });
        case Phase::WhileOpen:      return one_of(p, { "{" });
        case Phase::ReturnSemi:     return one_of(p, { ";" });
        case Phase::Done:           return false;
    }
    return false;
}

bool Prefix::commit(std::string_view token) {
    if (!possible(token)) return false;
    switch (phase_) {
        case Phase::TopLevel:
            declaring_ = {};
            if (token == "main") {
                return_type_ = Type::Int();
                phase_ = Phase::OpenParen;
                return true;
            }
            if (token == "func") { phase_ = Phase::FunctionName; return true; }
            return false;
        case Phase::FunctionName:
            if (!name_available(token)) return false;
            declaring_.name = std::string(token);
            declaring_.type_params.clear();
            phase_ = Phase::FunctionGenericOrParen;
            return true;
        case Phase::FunctionGenericOrParen:
            if (token == "<") { phase_ = Phase::TypeParamName; return true; }
            if (token == "(") { phase_ = Phase::ParamStart; return true; }
            return false;
        case Phase::TypeParamName:
            if (!type_parameter_prefix(token)) return false;
            if (parse_base(token) || type_param_named(token)) return false;
            declaring_.type_params.push_back(std::string(token));
            type_params_.push_back(declaring_.type_params.back());
            phase_ = Phase::TypeParamNext;
            return true;
        case Phase::TypeParamNext:
            if (token == ",") { phase_ = Phase::TypeParamName; return true; }
            if (token == ">") { phase_ = Phase::FunctionParen; return true; }
            return false;
        case Phase::FunctionParen:
            if (token != "(") return false;
            phase_ = Phase::ParamStart;
            return true;
        case Phase::ParamStart:
            if (token == ")") { phase_ = Phase::FunctionColon; return true; }
            [[fallthrough]];
        case Phase::ParamName:
            if (!name_available(token)) return false;
            new_name_ = std::string(token);
            phase_ = Phase::ParamColon;
            return true;
        case Phase::ParamColon:
            if (token != ":") return false;
            return begin_type(TypeDest::Param);
        case Phase::ParamType:
        case Phase::LambdaParamName:
        case Phase::LambdaParamType:
        case Phase::LambdaResult:
            return false;
        case Phase::AfterParam:
            if (token == ",") { phase_ = Phase::ParamName; return true; }
            if (token == ")") { phase_ = Phase::FunctionColon; return true; }
            return false;
        case Phase::FunctionColon:
            if (token != ":") return false;
            return begin_type(TypeDest::Return);
        case Phase::FunctionType:
            return false;
        case Phase::FunctionOpen:
            if (token != "{") return false;
            blocks_.push_back({ Block::Function, 0 });
            phase_ = Phase::Stmt;
            return true;
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
            after_return_ = false;
            if (token == "let" || token == "var") {
                assignment_ = false;
                mutable_binding_ = token == "var";
                phase_ = Phase::LetName;
                return true;
            }
            if (token == "if")     { phase_ = Phase::IfOpenParen; return true; }
            if (token == "while") { phase_ = Phase::WhileOpenParen; return true; }
            if (const auto * symbol = find(token); symbol && symbol->mutable_binding) {
                new_name_ = symbol->name;
                new_type_ = symbol->type;
                assignment_ = true;
                phase_ = Phase::AssignEqual;
                return true;
            }
            if ((token == "break" || token == "continue") && in_loop()) {
                phase_ = Phase::LoopControlSemi;
                return true;
            }
            if (token == "return") {
                return begin_expression(return_type_, ExprFollow::ReturnSemi);
            }
            if (token == "}") return close_block();
            return false;
        case Phase::LetName:
            if (!name_available(token)) return false;
            new_name_ = std::string(token);
            phase_ = Phase::LetColon;
            return true;
        case Phase::LetColon: if (token != ":") return false; return begin_type(TypeDest::Let);
        case Phase::LetType: return false;
        case Phase::LetEqual:
            if (token != "=") return false;
            return begin_expression(new_type_, ExprFollow::LetSemi);
        case Phase::AssignEqual:
            if (token != "=") return false;
            return begin_expression(new_type_, ExprFollow::LetSemi);
        case Phase::ExprStart: {
            if (expr_needed_ == Type::Bool() && token == "!") {
                phase_ = Phase::UnaryArg;
                return true;
            }
            if (token == "fun" && expr_needed_.kind == Type::Kind::Fun && has_atom(expr_needed_)) {
                lambdas_.push_back(
                    { expr_follow_, expr_needed_, saved_needed_, lambda_base_, lambda_param_, new_name_ });
                saved_needed_ = expr_needed_;
                saved_follow_ = expr_follow_;
                lambda_base_ = symbols_.size();
                lambda_param_ = 0;
                phase_ = Phase::LambdaParen;
                return true;
            }
            if (begin_operation(token, expr_needed_)) return true;
            if (const auto * symbol = find(token); symbol && symbol_callable(*symbol, expr_needed_)) {
                call_arguments_ = symbol->type.parameters;
                argument_ = 0;
                phase_ = Phase::CallOpen;
                return true;
            }
            Type left = Type::Int();
            bool found = false;
            if (integer_complete(token)) { left = Type::Int(); found = true; }
            else if (token == "true" || token == "false") { left = Type::Bool(); found = true; }
            else if (string_literal_complete(token)) { left = Type::String(); found = true; }
            else if (const auto * function = find_function(token);
                     function && function->type_params.empty() &&
                     function_type(*function) == expr_needed_) {
                left = expr_needed_;
                found = true;
            }
            else if (const auto * symbol = find(token)) { left = symbol->type; found = true; }
            if (!found || !left_can_reach(left, expr_needed_)) return false;
            left_type_ = left;
            if (left == expr_needed_ && !infix_possible(left, expr_needed_, "") &&
                !index_possible(left, expr_needed_)) {
                return finish_expression();
            }
            phase_ = Phase::ExprAfterLeft;
            return true;
        }
        case Phase::ExprAfterLeft: {
            if (token == ";" || token == ")") {
                if (left_type_ != expr_needed_) return false;
                if (token == ";") {
                    if (expr_follow_ != ExprFollow::LetSemi &&
                        expr_follow_ != ExprFollow::ReturnSemi) return false;
                    // Commit the follow punctuation through finish + LetSemi/ReturnSemi.
                    if (!finish_expression()) return false;
                    return commit(token);
                }
                if (expr_follow_ != ExprFollow::IfClose &&
                    expr_follow_ != ExprFollow::WhileClose) return false;
                if (!finish_expression()) return false;
                return commit(token);
            }
            if (token == "[") {
                if (!index_possible(left_type_, expr_needed_)) return false;
                phase_ = Phase::IndexArg;
                return true;
            }
            const auto * op = find_infix(token, left_type_, expr_needed_);
            if (!op) return false;
            infix_right_type_ = op->right;
            phase_ = Phase::InfixRight;
            return true;
        }
        case Phase::InfixRight:
            if (!atom_complete(infix_right_type_, token)) return false;
            return finish_expression();
        case Phase::IndexArg:
            if (!atom_complete(Type::Int(), token)) return false;
            phase_ = Phase::IndexClose;
            return true;
        case Phase::IndexClose:
            if (token != "]") return false;
            return finish_expression();
        case Phase::UnaryArg:
            if (!atom_complete(Type::Bool(), token)) return false;
            return finish_expression();
        case Phase::CallOpen:
            if (token != "(") return false;
            phase_ = call_arguments_.empty() ? Phase::CallClose : Phase::CallArg;
            return true;
        case Phase::CallArg: {
            if (argument_ >= call_arguments_.size() || !atom_complete(call_arguments_[argument_], token)) return false;
            phase_ = Phase::CallAfterArg;
            return true;
        }
        case Phase::CallAfterArg: {
            if (argument_ + 1 < call_arguments_.size() && token == ",") {
                ++argument_;
                phase_ = Phase::CallArg;
                return true;
            }
            if (argument_ + 1 == call_arguments_.size() && token == ")") {
                return finish_expression();
            }
            return false;
        }
        case Phase::CallClose:
            if (token != ")") return false;
            return finish_expression();
        case Phase::LetSemi:
            if (token != ";") return false;
            if (!assignment_) symbols_.push_back({ new_name_, new_type_, mutable_binding_ });
            phase_ = Phase::Stmt;
            return true;
        case Phase::LoopControlSemi:
            if (token != ";") return false;
            phase_ = Phase::Stmt;
            return true;
        case Phase::IfOpenParen:
            if (token != "(") return false;
            phase_ = Phase::IfAtom;
            return true;
        case Phase::IfAtom:
            if (!atom_complete(Type::Bool(), token)) return false;
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
        case Phase::WhileOpenParen:
            if (token != "(") return false;
            phase_ = Phase::WhileAtom;
            return true;
        case Phase::WhileAtom:
            if (!atom_complete(Type::Bool(), token)) return false;
            phase_ = Phase::WhileCloseParen;
            return true;
        case Phase::WhileCloseParen: if (token != ")") return false; phase_ = Phase::WhileOpen; return true;
        case Phase::WhileOpen:
            if (token != "{") return false;
            blocks_.push_back({ Block::While, symbols_.size() });
            phase_ = Phase::Stmt;
            return true;
        case Phase::ReturnSemi:
            if (token != ";") return false;
            after_return_ = true;
            phase_ = Phase::Stmt;
            return true;
        case Phase::TypeAtom: {
            if (token == "(") {
                type_stack_.push_back({});
                return true;
            }
            const auto parsed = base_or_param(token);
            if (!parsed) return false;
            return deliver_type(*parsed);
        }
        case Phase::TypeAfter:
            if (token == ",") { phase_ = Phase::TypeAtom; return true; }
            if (token == ")") { phase_ = Phase::TypeArrow; return true; }
            return false;
        case Phase::TypeArrow:
            if (token != "-") return false;
            phase_ = Phase::TypeArrowGt;
            return true;
        case Phase::TypeArrowGt:
            if (token != ">" || type_stack_.empty()) return false;
            type_stack_.back().result = true;
            phase_ = Phase::TypeAtom;
            return true;
        case Phase::CallTypeOpen:
            if (token != "<") return false;
            call_type_args_.clear();
            phase_ = Phase::CallTypeArg;
            return true;
        case Phase::CallTypeArg: {
            const auto parsed = base_or_param(token);
            if (!parsed || (parsed->kind != Type::Kind::Param &&
                            parsed->kind != Type::Kind::Int && parsed->kind != Type::Kind::Bool &&
                            parsed->kind != Type::Kind::List && parsed->kind != Type::Kind::String))
                return false;
            return finish_type(*parsed);
        }
        case Phase::CallTypeNext: {
            const auto * function = find_function(call_name_);
            if (!function) return false;
            if (token == "," && call_type_args_.size() < function->type_params.size()) {
                phase_ = Phase::CallTypeArg;
                return true;
            }
            if (token != ">" || call_type_args_.size() != function->type_params.size()) return false;
            call_arguments_.clear();
            for (const auto & argument : function->arguments)
                call_arguments_.push_back(substitute(argument, call_type_args_));
            if (substitute(function->result, call_type_args_) != expr_needed_) return false;
            argument_ = 0;
            phase_ = Phase::CallOpen;
            return true;
        }
        case Phase::LambdaParen:
            if (token != "(") return false;
            phase_ = Phase::LambdaParamStart;
            return true;
        case Phase::LambdaParamStart:
            if (token == ")" && lambda_param_ >= saved_needed_.parameters.size()) {
                phase_ = Phase::LambdaColon;
                return true;
            }
            if (lambda_param_ >= saved_needed_.parameters.size() || !name_available(token)) return false;
            new_name_ = std::string(token);
            phase_ = Phase::LambdaParamColon;
            return true;
        case Phase::LambdaParamColon:
            if (token != ":") return false;
            return begin_type(TypeDest::LambdaParam);
        case Phase::LambdaAfterParam:
            if (token == "," && lambda_param_ + 1 < saved_needed_.parameters.size()) {
                ++lambda_param_;
                phase_ = Phase::LambdaParamStart;
                return true;
            }
            if (token == ")" && lambda_param_ + 1 == saved_needed_.parameters.size()) {
                phase_ = Phase::LambdaColon;
                return true;
            }
            return false;
        case Phase::LambdaColon:
            if (token != ":") return false;
            return begin_type(TypeDest::LambdaResult);
        case Phase::LambdaEq:
            if (token != "=") return false;
            phase_ = Phase::LambdaGt;
            return true;
        case Phase::LambdaGt:
            if (token != ">") return false;
            return begin_expression(lambda_result_, ExprFollow::LambdaEnd);
        case Phase::Done: return false;
    }
    return false;
}

bool Prefix::string_literal_context() const {
    return expects_atom() && expected_atom_type() == Type::String();
}

bool Prefix::atom_start_phase() const {
    return phase_ == Phase::ExprStart || phase_ == Phase::InfixRight ||
           phase_ == Phase::IndexArg || phase_ == Phase::UnaryArg ||
           phase_ == Phase::CallArg;
}

bool Prefix::step(unsigned char ch) {
    if (quoting_) {
        if (ch == '"') {
            pending_.push_back('"');
            quoting_ = false;
            const std::string token = std::move(pending_);
            pending_.clear();
            return commit(token);
        }
        if (ch == ' ' || (ch >= 0x21 && ch <= 0x7e && ch != '\\')) {
            pending_.push_back(static_cast<char>(ch));
            return true;
        }
        return false;
    }
    if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r') {
        if (pending_.empty()) return true;
        const std::string token = std::move(pending_);
        pending_.clear();
        return commit(token);
    }
    if (ch < 0x21 || ch > 0x7e) return false;

    // Multi-character operators: == && ||, and '-' as either unary minus or infix.
    if (ch == '=' || ch == '&' || ch == '|' || ch == '-') {
        // Finish a pending atom/word before starting an operator (e.g. `2==`).
        if (!pending_.empty() && pending_ != "=" && pending_ != "&" && pending_ != "|" &&
            pending_ != "-") {
            const std::string token = std::move(pending_);
            pending_.clear();
            if (!commit(token)) return false;
        }
        if (ch == '-' && atom_start_phase()) {
            pending_ = "-";
            return possible(pending_);
        }
        if (phase_ == Phase::LetEqual || phase_ == Phase::AssignEqual ||
            phase_ == Phase::LambdaEq) {
            return commit("=");
        }
        if (phase_ == Phase::ExprAfterLeft) {
            if (pending_.empty()) {
                pending_.push_back(static_cast<char>(ch));
                return possible(pending_);
            }
            if ((pending_ == "=" && ch == '=') ||
                (pending_ == "&" && ch == '&') ||
                (pending_ == "|" && ch == '|')) {
                pending_.push_back(static_cast<char>(ch));
                const std::string token = std::move(pending_);
                pending_.clear();
                return commit(token);
            }
            if (pending_ == "-") {
                // Infix '-' then another '-' starts a negative right operand (`x--1`).
                const std::string token = std::move(pending_);
                pending_.clear();
                if (!commit(token)) return false;
                return step(ch);
            }
            return false;
        }
        if (ch == '-') {
            pending_ = "-";
            return possible(pending_);
        }
        return commit(std::string(1, static_cast<char>(ch)));
    }

    if (is_single_punct(ch)) {
        if (!pending_.empty()) {
            const std::string token = std::move(pending_);
            pending_.clear();
            if (!commit(token)) return false;
        }
        return commit(std::string(1, static_cast<char>(ch)));
    }
    if (ch == '"') {
        if (!pending_.empty() || !string_literal_context()) return false;
        quoting_ = true;
        pending_ = "\"";
        return true;
    }
    // Continue a signed integer only where '-' started an atom, not infix minus.
    if (pending_ == "-" && std::isdigit(ch) && atom_start_phase()) {
        pending_.push_back(static_cast<char>(ch));
        return possible(pending_);
    }
    // Infix '-' followed by a digit: commit subtraction, then start the operand.
    if (pending_ == "-" && std::isdigit(ch) && phase_ == Phase::ExprAfterLeft) {
        const std::string token = std::move(pending_);
        pending_.clear();
        if (!commit(token)) return false;
        return step(ch);
    }
    // Flush a pending operator fragment before starting a word.
    if (!pending_.empty() && (pending_ == "=" || pending_ == "&" || pending_ == "|" ||
                              pending_ == "-" || pending_ == "+" || pending_ == "*" ||
                              pending_ == "<" || pending_ == "!" || pending_ == "[")) {
        const std::string token = std::move(pending_);
        pending_.clear();
        if (!commit(token)) return false;
    }
    pending_.push_back(static_cast<char>(ch));
    return possible(pending_);
}

bool Prefix::expects_atom() const {
    return phase_ == Phase::ExprStart || phase_ == Phase::InfixRight ||
           phase_ == Phase::IndexArg || phase_ == Phase::UnaryArg ||
           phase_ == Phase::CallArg;
}

Type Prefix::expected_atom_type() const {
    switch (phase_) {
        case Phase::ExprStart: return expr_needed_;
        case Phase::InfixRight: return infix_right_type_;
        case Phase::IndexArg: return Type::Int();
        case Phase::UnaryArg: return Type::Bool();
        case Phase::CallArg:
            return argument_ < call_arguments_.size() ? call_arguments_[argument_] : Type::Int();
        default: return Type::Int();
    }
}

std::string Prefix::diagnose_message(std::string_view lexeme) const {
    if (lexeme.size() == 1) {
        const unsigned char ch = static_cast<unsigned char>(lexeme.front());
        if (ch < 0x21 || ch > 0x7e)
            return "unexpected character '" + display_lexeme(lexeme) + "'";
    }
    if (phase_ == Phase::Done)
        return "unexpected token after the end of the program";

    if (phase_ == Phase::Stmt && (lexeme == "break" || lexeme == "continue") && !in_loop())
        return "'" + std::string(lexeme) + "' is only allowed inside a loop";
    if (phase_ == Phase::Stmt) {
        if (const auto * symbol = find(lexeme)) {
            if (!symbol->mutable_binding)
                return "cannot assign to immutable '" + symbol->name + "'";
        }
    }

    if (phase_ == Phase::LetName || phase_ == Phase::FunctionName ||
        phase_ == Phase::ParamName || phase_ == Phase::ParamStart) {
        std::string_view name = lexeme;
        while (!name.empty() && !word_char(static_cast<unsigned char>(name.back())))
            name.remove_suffix(1);
        if (identifier_prefix(name) && !name_available(name)) {
            if (reserved(name))
                return "'" + std::string(name) + "' is a reserved name";
            return "name '" + std::string(name) + "' is already defined";
        }
    }

    if (expects_atom()) {
        if (looks_like_integer(lexeme) && !integer_prefix(lexeme))
            return "Int64 literal is out of range";
        std::optional<Type> found;
        if (lexeme == "true" || lexeme == "false") {
            found = Type::Bool();
        } else if (integer_complete(lexeme)) {
            found = Type::Int();
        } else if (string_literal_complete(lexeme)) {
            found = Type::String();
        } else if (const auto * symbol = find(lexeme)) {
            found = symbol->type;
        }
        if (found) {
            const Type expected = expected_atom_type();
            if (phase_ == Phase::ExprStart) {
                if (!left_can_reach(*found, expr_needed_)) {
                    return std::string("expected ") + type_name(expr_needed_) + ", found " +
                           type_name(*found) + " '" + std::string(lexeme) + "'";
                }
            } else if (*found != expected) {
                return std::string("expected ") + type_name(expected) + ", found " +
                       type_name(*found) + " '" + std::string(lexeme) + "'";
            }
        }
        if (phase_ == Phase::ExprStart) {
            if (const auto * op = operation(lexeme); op && op->result != expr_needed_) {
                return std::string("call '") + op->name + "' has type " + type_name(op->result) +
                       " but expected " + type_name(expr_needed_);
            }
            if (const auto * function = find_function(lexeme);
                function && function->result != expr_needed_) {
                return "call '" + function->name + "' has type " + type_name(function->result) +
                       " but expected " + type_name(expr_needed_);
            }
        }
        if (phase_ == Phase::CallArg && lexeme == ")" && argument_ < call_arguments_.size())
            return std::string("expected ") + type_name(call_arguments_[argument_]) +
                   " argument, found ')'";
    }

    if (phase_ == Phase::CallAfterArg) {
        if (argument_ + 1 < call_arguments_.size() && lexeme == ")")
            return std::string("expected ") + type_name(call_arguments_[argument_ + 1]) +
                   " argument, found ')'";
        if (argument_ + 1 >= call_arguments_.size() && lexeme == ",")
            return "expected ')', found ','";
    }

    return "expected " + expected_for_debug() + ", found '" + display_lexeme(lexeme) + "'";
}

bool Prefix::append(std::string_view bytes) {
    Prefix next = *this;
    next.diagnostic_.reset();
    size_t offset = offset_;
    size_t line = line_;
    size_t column = column_;
    for (size_t i = 0; i < bytes.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(bytes[i]);
        Prefix before = next;
        if (!next.step(ch)) {
            const bool committed_pending = !before.pending_.empty() && next.pending_.empty() &&
                (next.phase_ != before.phase_ || next.blocks_.size() != before.blocks_.size() ||
                 next.symbols_.size() != before.symbols_.size());
            const Prefix & failure = committed_pending ? next : before;

            size_t token_offset = offset;
            size_t token_line = line;
            size_t token_column = column;
            std::string lexeme_storage;
            std::string_view lexeme;
            if (!committed_pending && !before.pending_.empty()) {
                token_offset = before.token_start_offset_;
                token_line = before.token_start_line_;
                token_column = before.token_start_column_;
                lexeme_storage = before.pending_;
                lexeme_storage.push_back(static_cast<char>(ch));
                const std::string_view rest = bytes.substr(i + 1);
                const std::string_view more = first_lexeme(rest);
                if (!more.empty() && word_char(static_cast<unsigned char>(lexeme_storage.back())) &&
                    word_char(static_cast<unsigned char>(more.front()))) {
                    lexeme_storage.append(more);
                }
                lexeme = lexeme_storage;
            } else {
                const std::string_view rest = bytes.substr(i);
                const std::string_view first = first_lexeme(rest);
                lexeme = first.empty() ? rest.substr(0, 1) : first;
                lexeme_storage = std::string(lexeme);
                lexeme = lexeme_storage;
            }

            diagnostic_ = Diagnostic{
                token_line, token_column, token_offset,
                std::max<size_t>(1, lexeme.size()),
                failure.diagnose_message(lexeme),
            };
            return false;
        }
        if (before.pending_.empty() && !next.pending_.empty()) {
            next.token_start_offset_ = offset;
            next.token_start_line_ = line;
            next.token_start_column_ = column;
        }
        advance_position(ch, offset, line, column);
        next.offset_ = offset;
        next.line_ = line;
        next.column_ = column;
    }
    *this = std::move(next);
    return true;
}

bool Prefix::can_append(std::string_view bytes) const {
    Prefix next = *this;
    return next.append(bytes);
}

Prefix Prefix::state_before_rejection_for_debug(std::string_view bytes) const {
    Prefix next = *this;
    next.diagnostic_.reset();
    for (size_t i = 0; i < bytes.size(); ++i) {
        Prefix before = next;
        if (!next.step(static_cast<unsigned char>(bytes[i]))) return before;
    }
    return next;
}

Diagnostic Prefix::end_diagnostic() const {
    return Diagnostic{ line_, column_, offset_, 1,
        "unexpected end of input; expected " + expected_for_debug() };
}

void print_diagnostic(FILE * out, std::string_view path, std::string_view source,
                      const Diagnostic & diagnostic) {
    std::fprintf(out, "%.*s:%zu:%zu: error: %s\n",
                 static_cast<int>(path.size()), path.data(),
                 diagnostic.line, diagnostic.column, diagnostic.message.c_str());
    size_t line_start = 0;
    size_t current_line = 1;
    while (current_line < diagnostic.line && line_start < source.size()) {
        const size_t next = source.find('\n', line_start);
        if (next == std::string_view::npos) {
            line_start = source.size();
            break;
        }
        line_start = next + 1;
        ++current_line;
    }
    size_t line_end = line_start;
    while (line_end < source.size() && source[line_end] != '\n') ++line_end;
    const std::string_view line = source.substr(line_start, line_end - line_start);
    std::fprintf(out, "%.*s\n", static_cast<int>(line.size()), line.data());

    const size_t caret_column = diagnostic.column == 0 ? 1 : diagnostic.column;
    for (size_t i = 1; i < caret_column; ++i) std::fputc(' ', out);
    std::fputc('^', out);
    const size_t mark = diagnostic.length == 0 ? 0 : diagnostic.length - 1;
    for (size_t i = 0; i < mark; ++i) std::fputc('~', out);
    std::fputc('\n', out);
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
    if (type == Type::Int()) {
        if (pending_.empty()) return "0";
        if (integer_prefix(pending_)) return pending_ == "-" ? "-0" : pending_;
    }
    if (type == Type::Bool()) {
        for (auto literal : { "false", "true" }) {
            if (starts_with(literal, pending_)) return literal;
        }
    }
    if (type == Type::String()) {
        if (pending_.empty()) return "\"\"";
        if (string_literal_prefix(pending_) && !string_literal_complete(pending_))
            return std::string(pending_) + "\"";
        if (string_literal_complete(pending_)) return std::string(pending_);
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
    const auto fresh_name = [this]() {
        std::string name = pending_.empty() ? "x" : pending_;
        if (!identifier_prefix(name)) return std::string{};
        for (size_t guard = 0; !name_available(name); ++guard) {
            if (guard == 64) return std::string{};
            name += "x";
        }
        return name;
    };
    const auto fresh_type_param = [this]() {
        std::string name = pending_.empty() ? "T" : pending_;
        if (!type_parameter_prefix(name)) return std::string{};
        const auto taken = [&](const std::string & candidate) {
            return parse_base(candidate).has_value() ||
                std::find(declaring_.type_params.begin(), declaring_.type_params.end(), candidate) !=
                    declaring_.type_params.end();
        };
        for (size_t guard = 0; taken(name); ++guard) {
            if (guard == 64) return std::string{};
            name += "T";
        }
        return name;
    };
    switch (phase_) {
        case Phase::TopLevel:    return fixed({ "main", "func" });
        case Phase::FunctionName:
        case Phase::ParamName:   return fresh_name();
        case Phase::FunctionParen:return fixed({ "(" });
        case Phase::ParamStart:
            if (const auto token = fixed({ ")" }); !token.empty()) return token;
            return fresh_name();
        case Phase::ParamColon:
        case Phase::FunctionColon:return fixed({ ":" });
        case Phase::ParamType:
        case Phase::FunctionType:return fixed({ "Int64", "Bool", "ListInt64", "String" });
        case Phase::AfterParam:  return fixed({ ")", "," });
        case Phase::FunctionOpen:return fixed({ "{" });
        case Phase::OpenParen:   return fixed({ "(" });
        case Phase::CloseParen:  return fixed({ ")" });
        case Phase::Colon:       return fixed({ ":" });
        case Phase::MainType:    return fixed({ "Int64" });
        case Phase::MainOpen:    return fixed({ "{" });
        case Phase::Stmt:
            if (after_return_) {
                if (const auto token = fixed({ "}" }); !token.empty()) return token;
            }
            if (blocks_.size() == 1 && has_atom(return_type_)) {
                const auto token = fixed({ "return" });
                if (!token.empty()) return token;
            }
            if (!blocks_.empty()) {
                const auto token = fixed({ "}" });
                if (!token.empty()) return token;
            }
            if (has_atom(return_type_)) {
                if (const auto token = fixed({ "return" }); !token.empty()) return token;
            }
            if (const auto token = fixed({ "let", "var", "if", "while" }); !token.empty()) return token;
            for (const auto & symbol : symbols_) {
                if (symbol.mutable_binding && starts_with(symbol.name, pending_)) return symbol.name;
            }
            if (in_loop()) {
                if (const auto token = fixed({ "break", "continue" }); !token.empty()) return token;
            }
            return {};
        case Phase::LetName:     return fresh_name();
        case Phase::LetColon:    return fixed({ ":" });
        case Phase::LetType:
            return fixed({ "Int64", "Bool", "ListInt64", "String" });
        case Phase::LetEqual:
        case Phase::AssignEqual: return fixed({ "=" });
        case Phase::ExprStart: {
            if (expr_needed_.kind == Type::Kind::Fun && has_atom(expr_needed_) &&
                starts_with("fun", pending_)) return "fun";
            for (const auto & function : functions_) {
                if (function.type_params.empty() && function_type(function) == expr_needed_ &&
                    starts_with(function.name, pending_)) return function.name;
            }
            if (const auto atom = completion_atom(expr_needed_); !atom.empty()) return atom;
            if (expr_needed_ == Type::Bool() && starts_with("!", pending_)) return "!";
            for (const auto & op : operations) {
                if (op.result == expr_needed_ && operation_possible(expr_needed_, op.name) &&
                    starts_with(op.name, pending_)) return op.name;
            }
            for (const auto & function : functions_) {
                if (!starts_with(function.name, pending_)) continue;
                if (function.type_params.empty() && function.result != expr_needed_) continue;
                if (operation_possible(expr_needed_, function.name)) return function.name;
            }
            for (const auto & symbol : symbols_) {
                if (!starts_with(symbol.name, pending_)) continue;
                if (left_can_reach(symbol.type, expr_needed_) || symbol_callable(symbol, expr_needed_))
                    return symbol.name;
            }
            if (left_can_reach(Type::Int(), expr_needed_)) {
                if (const auto atom = completion_atom(Type::Int()); !atom.empty()) return atom;
            }
            return {};
        }
        case Phase::ExprAfterLeft: {
            if (left_type_ == expr_needed_) {
                if (expr_follow_ == ExprFollow::LetSemi || expr_follow_ == ExprFollow::ReturnSemi) {
                    if (const auto token = fixed({ ";" }); !token.empty()) return token;
                } else if (expr_follow_ != ExprFollow::LambdaEnd) {
                    if (const auto token = fixed({ ")" }); !token.empty()) return token;
                }
            }
            if (index_possible(left_type_, expr_needed_)) {
                if (const auto token = fixed({ "[" }); !token.empty()) return token;
            }
            for (const auto & op : infix_ops) {
                if (op.left == left_type_ && op.result == expr_needed_ &&
                    starts_with(op.text, pending_)) return op.text;
            }
            return {};
        }
        case Phase::InfixRight:  return completion_atom(infix_right_type_);
        case Phase::IndexArg:    return completion_atom(Type::Int());
        case Phase::IndexClose:  return fixed({ "]" });
        case Phase::UnaryArg:    return completion_atom(Type::Bool());
        case Phase::CallOpen:    return fixed({ "(" });
        case Phase::CallArg:
            return argument_ < call_arguments_.size() ? completion_atom(call_arguments_[argument_]) : std::string{};
        case Phase::CallAfterArg:return fixed({ argument_ + 1 < call_arguments_.size() ? "," : ")" });
        case Phase::CallClose:  return fixed({ ")" });
        case Phase::LetSemi:        return fixed({ ";" });
        case Phase::LoopControlSemi:return fixed({ ";" });
        case Phase::IfOpenParen:    return fixed({ "(" });
        case Phase::IfCloseParen:   return fixed({ ")" });
        case Phase::IfOpen:         return fixed({ "{" });
        case Phase::Else:           return fixed({ "else" });
        case Phase::ElseOpen:       return fixed({ "{" });
        case Phase::WhileOpenParen: return fixed({ "(" });
        case Phase::WhileCloseParen:return fixed({ ")" });
        case Phase::WhileOpen:      return fixed({ "{" });
        case Phase::ReturnSemi:     return fixed({ ";" });
        case Phase::FunctionGenericOrParen: return fixed({ "(", "<" });
        case Phase::TypeParamName:  return fresh_type_param();
        case Phase::TypeParamNext:  return fixed({ ">", "," });
        case Phase::IfAtom:
        case Phase::WhileAtom:      return completion_atom(Type::Bool());
        case Phase::TypeAtom: {
            if (const auto token = fixed({ "Int64", "Bool", "ListInt64", "String", "(" }); !token.empty())
                return token;
            for (const auto & name : type_params_)
                if (starts_with(name, pending_)) return name;
            return {};
        }
        case Phase::TypeAfter:      return fixed({ ")", "," });
        case Phase::TypeArrow:      return fixed({ "-" });
        case Phase::TypeArrowGt:    return fixed({ ">" });
        case Phase::CallTypeOpen:   return fixed({ "<" });
        case Phase::CallTypeArg: {
            if (const auto token = fixed({ "Int64", "Bool", "ListInt64", "String" }); !token.empty())
                return token;
            for (const auto & name : type_params_)
                if (starts_with(name, pending_)) return name;
            return {};
        }
        case Phase::CallTypeNext:   return fixed({ ">", "," });
        case Phase::LambdaParen:    return fixed({ "(" });
        case Phase::LambdaParamStart: return fresh_name();
        case Phase::LambdaParamColon:
        case Phase::LambdaColon:    return fixed({ ":" });
        case Phase::LambdaAfterParam: return fixed({ ")", "," });
        case Phase::LambdaEq:       return fixed({ "=" });
        case Phase::LambdaGt:       return fixed({ ">" });
        case Phase::LambdaParamName: return fresh_name();
        case Phase::LambdaParamType:
        case Phase::LambdaResult:   return fixed({ "Int64" });
        case Phase::Done:           return {};
    }
    return {};
}

std::optional<std::string> Prefix::completion() const {
    Prefix state = *this;
    std::string suffix;
    const size_t limit = 96 + state.blocks_.size() * 16 + state.call_arguments_.size() * 4;
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
    switch (phase_) {
        case Phase::TopLevel: return "func declaration or main";
        case Phase::FunctionName: return "fresh function name";
        case Phase::ParamStart:
        case Phase::ParamName: return "fresh parameter name or parameter list end";
        case Phase::ParamType:
        case Phase::FunctionType: return "Int64 or Bool type";
        case Phase::MainType: return "Int64 main return type";
        case Phase::Stmt: return "statement";
        case Phase::LetName: return "fresh variable name";
        case Phase::ExprStart: return std::string(type_name(expr_needed_)) + " expression";
        case Phase::ExprAfterLeft: return "operator, index, or expression end";
        case Phase::InfixRight: return std::string(type_name(infix_right_type_)) + " operand";
        case Phase::IndexArg: return "Int64 index";
        case Phase::IndexClose: return "']'";
        case Phase::UnaryArg: return "Bool operand";
        case Phase::CallArg:
            return argument_ < call_arguments_.size() ?
                std::string(type_name(call_arguments_[argument_])) + " argument" : "argument";
        case Phase::AssignEqual: return "'='";
        case Phase::LoopControlSemi:return "semicolon after break/continue";
        case Phase::ReturnSemi: return "semicolon after return";
        case Phase::Done: return "end of program";
        default: return "grammar token";
    }
}

} // namespace typed
