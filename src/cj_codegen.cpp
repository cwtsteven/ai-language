#include "cj_codegen.h"
#include "typed_prefix.h"

#include <cctype>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace typed {
namespace {

struct Atom {
    enum class Kind { Literal, Name } kind;
    std::string text;
    bool function_ref = false;
};

struct Expression {
    std::optional<Atom> atom;
    std::string callee;
    std::vector<Expression> arguments;
    std::vector<Type> type_arguments;
    bool function_ref = false;
    bool callee_is_function = false;
    std::vector<Symbol> lambda_params;
    std::shared_ptr<Expression> lambda_body;
};

struct Statement {
    enum class Kind { Let, Var, Assign, If, While, Return, Break, Continue } kind;
    std::string name;
    Type type = Type::Int();
    Expression value;
    std::vector<Statement> body;
    std::vector<Statement> alternative;
};

struct Function {
    std::string name;
    std::vector<std::string> type_params;
    Type result;
    std::vector<Symbol> parameters;
    std::vector<Statement> body;
};

// Parsing is separate from emission. A name reads its current value; assignment
// updates a var and does not alias an earlier let binding.
class Parser {
public:
    explicit Parser(std::string_view source) {
        auto expects_atom = [](std::string_view previous) {
            return previous == "=" || previous == "(" || previous == "," || previous == "return" ||
                previous == "!" || previous == "[" || previous == "+" || previous == "-" ||
                previous == "*" || previous == "==" || previous == "<" || previous == "&&" ||
                previous == "||";
        };
        for (size_t i = 0; i < source.size();) {
            const unsigned char ch = source[i];
            if (std::isspace(ch)) { ++i; continue; }
            const size_t start = i++;
            const std::string_view previous = tokens_.empty() ? std::string_view{} : tokens_.back();
            if (ch == '"') {
                while (i < source.size() && source[i] != '"') {
                    const unsigned char inside = static_cast<unsigned char>(source[i]);
                    if (inside == '\\' || inside < 0x20 || inside > 0x7e)
                        throw std::logic_error("invalid string literal in checked source");
                    ++i;
                }
                if (i >= source.size()) throw std::logic_error("unterminated string literal in checked source");
                ++i;
            } else if (ch == '-' && expects_atom(previous) && i < source.size() &&
                       std::isdigit(static_cast<unsigned char>(source[i]))) {
                while (i < source.size() &&
                       std::isdigit(static_cast<unsigned char>(source[i]))) ++i;
            } else if (ch == '=' || ch == '&' || ch == '|') {
                if (i < source.size() && source[i] == static_cast<char>(ch)) ++i;
            } else if (std::isalnum(ch) || ch == '_') {
                while (i < source.size() &&
                       (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            }
            // Single-character operators and punctuation are already one byte.
            tokens_.emplace_back(source.substr(start, i - start));
        }
    }

    std::vector<Function> program() {
        std::vector<Function> functions;
        while (peek("func")) functions.push_back(function(false));
        functions.push_back(function(true));
        if (position_ != tokens_.size()) throw std::logic_error("unparsed source after main");
        return functions;
    }

private:
    struct Binding {
        std::string name;
        Type type;
    };

    std::vector<std::string> tokens_;
    size_t position_ = 0;
    std::vector<Binding> symbols_;
    std::vector<std::string> function_names_;

    bool peek(std::string_view token) const {
        return position_ < tokens_.size() && tokens_[position_] == token;
    }
    std::string take() {
        if (position_ >= tokens_.size()) throw std::logic_error("unexpected end of checked source");
        return tokens_[position_++];
    }
    void expect(std::string_view token) {
        if (take() != token) throw std::logic_error("unexpected token in checked source");
    }
    Type type() {
        if (peek("(")) {
            take();
            std::vector<Type> parameters;
            if (!peek(")")) {
                while (true) {
                    parameters.push_back(type());
                    if (peek(")")) break;
                    expect(",");
                }
            }
            expect(")");
            expect("-");
            expect(">");
            return Type::Fun(std::move(parameters), type());
        }
        const auto name = take();
        if (name == "Int64") return Type::Int();
        if (name == "Bool") return Type::Bool();
        if (name == "ListInt64") return Type::List();
        if (name == "String") return Type::String();
        return Type::Param(name);
    }
    Type lookup(const std::string & name) const {
        for (auto binding = symbols_.rbegin(); binding != symbols_.rend(); ++binding) {
            if (binding->name == name) return binding->type;
        }
        throw std::logic_error("unbound name in checked source");
    }
    Atom atom() {
        auto text = take();
        if (text == "true" || text == "false" || (!text.empty() && text.front() == '"'))
            return {Atom::Kind::Literal, std::move(text)};
        if (text.front() == '-' || std::isdigit(static_cast<unsigned char>(text.front()))) {
            const bool negative = text.front() == '-';
            const size_t digits = negative ? 1 : 0;
            const size_t first = text.find_first_not_of('0', digits);
            text = first == std::string::npos ? "0" : (negative ? "-" : "") + text.substr(first);
            return {Atom::Kind::Literal, std::move(text)};
        }
        Atom named{ Atom::Kind::Name, std::move(text), false };
        named.function_ref = known_function(named.text);
        return named;
    }
    bool known_function(const std::string & name) const {
        for (const auto & function : function_names_) if (function == name) return true;
        return false;
    }
    static int infix_precedence(std::string_view op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "<") return 3;
        if (op == "+" || op == "-") return 4;
        if (op == "*") return 5;
        return 0;
    }
    Expression expression() { return parse_expr(0); }
    Expression parse_prefix() {
        Expression result;
        if (peek("fun")) {
            take();
            expect("(");
            while (!peek(")")) {
                Symbol parameter;
                parameter.name = take();
                expect(":");
                parameter.type = type();
                result.lambda_params.push_back(std::move(parameter));
                if (peek(")")) break;
                expect(",");
            }
            expect(")");
            expect(":");
            type();
            expect("=");
            expect(">");
            const size_t scope = symbols_.size();
            for (const auto & parameter : result.lambda_params)
                symbols_.push_back({ parameter.name, parameter.type });
            result.lambda_body = std::make_shared<Expression>(parse_expr(0));
            symbols_.resize(scope);
            return result;
        }
        if (peek("!")) {
            take();
            result.callee = "!";
            result.arguments.push_back(parse_expr(6));
            return result;
        }
        if (peek("(")) {
            take();
            result = parse_expr(0);
            expect(")");
            return result;
        }
        const bool generic_call = position_ + 1 < tokens_.size() && tokens_[position_ + 1] == "<" &&
            known_function(tokens_[position_]);
        const bool direct_call = position_ + 1 < tokens_.size() && tokens_[position_ + 1] == "(";
        if (generic_call || direct_call) {
            result.callee = take();
            result.callee_is_function = known_function(result.callee);
            if (peek("<")) {
                take();
                while (!peek(">")) {
                    result.type_arguments.push_back(type());
                    if (peek(">")) break;
                    expect(",");
                }
                expect(">");
            }
            expect("(");
            while (!peek(")")) {
                result.arguments.push_back(parse_expr(0));
                if (peek(")")) break;
                expect(",");
            }
            expect(")");
            return result;
        }
        Atom left = atom();
        if (left.kind == Atom::Kind::Name && known_function(left.text)) result.function_ref = true;
        result.atom = std::move(left);
        return result;
    }
    Expression parse_expr(int min_prec) {
        Expression left = parse_prefix();
        while (peek("[") && 7 >= min_prec) {
            const bool list = left.atom && left.atom->kind == Atom::Kind::Name &&
                lookup(left.atom->text) == Type::List();
            expect("[");
            Expression index = parse_expr(0);
            expect("]");
            Expression indexed;
            indexed.callee = list ? "[]" : "str[]";
            indexed.arguments = { std::move(left), std::move(index) };
            left = std::move(indexed);
        }
        while (true) {
            int precedence = 0;
            std::string op;
            for (auto candidate : { "||", "&&", "==", "<", "+", "-", "*" }) {
                if (!peek(candidate)) continue;
                precedence = infix_precedence(candidate);
                op = candidate;
                break;
            }
            if (op.empty() || precedence < min_prec) break;
            take();
            Expression right = parse_expr(precedence + 1);
            Expression combined;
            combined.callee = std::move(op);
            combined.arguments = { std::move(left), std::move(right) };
            left = std::move(combined);
        }
        return left;
    }
    std::vector<Statement> block() {
        expect("{");
        const size_t scope = symbols_.size();
        std::vector<Statement> body;
        while (!peek("}")) body.push_back(statement());
        expect("}");
        symbols_.resize(scope);
        return body;
    }
    Statement statement() {
        const auto keyword = take();
        Statement result;
        if (keyword == "let" || keyword == "var") {
            result.kind = keyword == "var" ? Statement::Kind::Var : Statement::Kind::Let;
            result.name = take();
            expect(":");
            result.type = type();
            expect("=");
            result.value = expression();
            symbols_.push_back({result.name, result.type});
        } else if (keyword == "if" || keyword == "while") {
            result.kind = keyword == "if" ? Statement::Kind::If : Statement::Kind::While;
            expect("(");
            result.value = expression();
            expect(")");
            result.body = block();
            if (keyword == "if") {
                expect("else");
                result.alternative = block();
            }
            return result;
        } else if (keyword == "return") {
            result.kind = Statement::Kind::Return;
            result.value = expression();
        } else if (keyword == "break") result.kind = Statement::Kind::Break;
        else if (keyword == "continue") result.kind = Statement::Kind::Continue;
        else {
            result.kind = Statement::Kind::Assign;
            result.name = keyword;
            result.type = lookup(keyword);
            expect("=");
            result.value = expression();
        }
        expect(";");
        return result;
    }
    Function function(bool main) {
        if (!main) expect("func");
        Function result;
        result.name = take();
        if (main && result.name != "main") throw std::logic_error("missing main in checked source");
        function_names_.push_back(result.name);
        if (peek("<")) {
            take();
            while (!peek(">")) {
                result.type_params.push_back(take());
                if (peek(">")) break;
                expect(",");
            }
            expect(">");
        }
        expect("(");
        while (!peek(")")) {
            const auto name = take();
            expect(":");
            result.parameters.push_back({name, type()});
            if (peek(")")) break;
            expect(",");
        }
        expect(")");
        expect(":");
        result.result = type();
        const size_t scope = symbols_.size();
        for (const auto & parameter : result.parameters) symbols_.push_back({parameter.name, parameter.type});
        result.body = block();
        symbols_.resize(scope);
        return result;
    }
};

class Emitter {
public:
    std::string program(const std::vector<Function> & functions) {
        for (const auto & function : functions) emit_function(function);
        std::string body = std::move(output_);
        output_.clear();
        depth_ = 0;
        if (needs_list_) line("import std.collection.ArrayList");
        if (needs_list_) {
            line("func typed_rt_list_push(typed_rt_items: ArrayList<Int64>, typed_rt_value: Int64): ArrayList<Int64> {");
            ++depth_;
            line("let typed_rt_copy = typed_rt_items.clone()");
            line("typed_rt_copy.add(typed_rt_value)");
            line("typed_rt_copy");
            --depth_;
            line("}");
        }
        if (needs_character_) {
            if (!output_.empty()) output_ += '\n';
            line("func typed_rt_str_at(typed_rt_text: String, typed_rt_index: Int64): String {");
            ++depth_;
            line("let typed_rt_chars = typed_rt_text.toRuneArray()");
            line("String([typed_rt_chars[typed_rt_index]])");
            --depth_;
            line("}");
        }
        if (!output_.empty()) output_ += '\n';
        output_ += body;
        return output_;
    }

private:
    std::string output_;
    size_t depth_ = 0;
    bool needs_list_ = false;
    bool needs_character_ = false;
    Type return_type_ = Type::Int();

    // Disjoint prefixes avoid collisions with Cangjie keywords, runtime classes,
    // and later function names that an earlier body used as a local variable.
    static std::string variable(const std::string & name) { return "typed_var_" + name; }
    static std::string callable(const std::string & name) { return "typed_fn_" + name; }
    std::string type(Type value) {
        switch (value.kind) {
            case Type::Kind::Int: return "Int64";
            case Type::Kind::Bool: return "Bool";
            case Type::Kind::List: needs_list_ = true; return "ArrayList<Int64>";
            case Type::Kind::String: return "String";
            case Type::Kind::Param: return value.param;
            case Type::Kind::Fun: {
                std::string text = "(";
                for (size_t i = 0; i < value.parameters.size(); ++i)
                    text += (i ? ", " : "") + type(value.parameters[i]);
                return text + ") -> " + type(*value.result);
            }
        }
        throw std::logic_error("unknown checked type");
    }
    // A list name is cloned when it is stored. listPush already returns a new list.
    std::string stored(Type destination, const Expression & value) {
        std::string text = expression(value);
        if (destination == Type::List() && value.atom && value.atom->kind == Atom::Kind::Name)
            text += ".clone()";
        return text;
    }
    void line(const std::string & text) { output_ += std::string(depth_ * 4, ' ') + text + '\n'; }
    static std::string atom(const Atom & value) {
        if (value.kind == Atom::Kind::Literal) return value.text;
        if (value.function_ref) return callable(value.text);
        return variable(value.text);
    }
    std::string expression(const Expression & value) {
        if (value.lambda_body) {
            std::string text = "{";
            for (size_t i = 0; i < value.lambda_params.size(); ++i) {
                if (i) text += ", ";
                text += variable(value.lambda_params[i].name) + ": " + type(value.lambda_params[i].type);
            }
            return text + " => " + expression(*value.lambda_body) + "}";
        }
        if (value.atom) {
            if (value.function_ref) return callable(value.atom->text);
            return atom(*value.atom);
        }
        const auto & name = value.callee;
        const auto & args = value.arguments;
        if (name == "+" || name == "-" || name == "*" || name == "==" || name == "<" ||
            name == "&&" || name == "||") {
            return "(" + expression(args.at(0)) + " " + name + " " + expression(args.at(1)) + ")";
        }
        if (name == "!") return "(!" + expression(args.at(0)) + ")";
        if (name == "emptyList") { needs_list_ = true; return "ArrayList<Int64>()"; }
        if (name == "listLen") { needs_list_ = true; return "(" + expression(args.at(0)) + ".size)"; }
        if (name == "[]") {
            needs_list_ = true;
            return "(" + expression(args.at(0)) + "[" + expression(args.at(1)) + "])";
        }
        if (name == "listPush") {
            needs_list_ = true;
            return "typed_rt_list_push(" + expression(args.at(0)) + ", " + expression(args.at(1)) + ")";
        }
        if (name == "strLen") return "(" + expression(args.at(0)) + ".size)";
        if (name == "str[]") {
            needs_character_ = true;
            return "typed_rt_str_at(" + expression(args.at(0)) + ", " + expression(args.at(1)) + ")";
        }
        std::string result = value.callee_is_function ? callable(name) : variable(name);
        if (!value.type_arguments.empty()) {
            result += "<";
            for (size_t i = 0; i < value.type_arguments.size(); ++i)
                result += (i ? ", " : "") + type(value.type_arguments[i]);
            result += ">";
        }
        result += "(";
        for (size_t i = 0; i < args.size(); ++i) result += (i ? ", " : "") + expression(args[i]);
        return result + ")";
    }
    // Used only to decide whether a fallthrough trap is needed in generated CJ.
    // It does not reject source or change the prefix checker's return rules.
    bool block(const std::vector<Statement> & statements) {
        bool falls_through = true;
        for (const auto & statement : statements) {
            bool continues = true;
            switch (statement.kind) {
                case Statement::Kind::Let:
                case Statement::Kind::Var: {
                    const auto declared_type = type(statement.type);
                    const auto rhs = stored(statement.type, statement.value);
                    const char * keyword = statement.kind == Statement::Kind::Var ? "var " : "let ";
                    line(std::string(keyword) + variable(statement.name) + ": " + declared_type + " = " + rhs);
                    break;
                }
                case Statement::Kind::Assign:
                    line(variable(statement.name) + " = " + stored(statement.type, statement.value));
                    break;
                case Statement::Kind::If: {
                    line("if (" + expression(statement.value) + ") {");
                    ++depth_;
                    const bool then_continues = block(statement.body);
                    --depth_;
                    line("} else {");
                    ++depth_;
                    const bool else_continues = block(statement.alternative);
                    --depth_;
                    line("}");
                    continues = then_continues || else_continues;
                    break;
                }
                case Statement::Kind::While:
                    line("while (" + expression(statement.value) + ") {");
                    ++depth_;
                    block(statement.body);
                    --depth_;
                    line("}");
                    break;
                case Statement::Kind::Return:
                    line("return " + stored(return_type_, statement.value));
                    continues = false;
                    break;
                case Statement::Kind::Break:
                case Statement::Kind::Continue:
                    line(statement.kind == Statement::Kind::Break ? "break" : "continue");
                    continues = false;
                    break;
            }
            falls_through &= continues;
        }
        return falls_through;
    }
    void emit_function(const Function & function) {
        return_type_ = function.result;
        if (!output_.empty()) output_ += '\n';
        std::string header = function.name == "main" ? "main" : "func " + callable(function.name);
        if (!function.type_params.empty()) {
            header += "<";
            for (size_t i = 0; i < function.type_params.size(); ++i)
                header += (i ? ", " : "") + function.type_params[i];
            header += ">";
        }
        header += "(";
        for (size_t i = 0; i < function.parameters.size(); ++i) {
            const auto & parameter = function.parameters[i];
            header += (i ? ", " : "") + variable(parameter.name) + ": " + type(parameter.type);
        }
        header += "): " + type(function.result) + " {";
        line(header);
        ++depth_;
        if (block(function.body)) line("throw Exception(\"function " + function.name + " exited without returning\")");
        --depth_;
        line("}");
    }
};

} // namespace

std::optional<std::string> generate_cangjie(std::string_view source) {
    Prefix checker;
    if (!checker.append(source) || !checker.can_end()) return std::nullopt;
    return Emitter{}.program(Parser(source).program());
}

} // namespace typed
