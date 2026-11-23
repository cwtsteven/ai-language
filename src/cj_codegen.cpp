#include "cj_codegen.h"
#include "typed_prefix.h"

#include <cctype>
#include <stdexcept>
#include <utility>
#include <vector>

namespace typed {
namespace {

struct Atom {
    enum class Kind { Literal, Name, Dereference } kind;
    std::string text;
};

struct Expression {
    std::optional<Atom> atom;
    std::string callee;
    std::vector<Atom> arguments;
};

struct Statement {
    enum class Kind { Let, Assign, If, While, Return, Break, Continue } kind;
    std::string name;
    Type type = Type::Int;
    Expression value;
    std::vector<Statement> body;
    std::vector<Statement> alternative;
};

struct Function {
    std::string name;
    Type result;
    std::vector<Symbol> parameters;
    std::vector<Statement> body;
};

// Parsing is deliberately separate from emission: ! reads a cell, it is not
// Cangjie's Boolean negation, and aliases must retain object identity.
class Parser {
public:
    explicit Parser(std::string_view source) {
        for (size_t i = 0; i < source.size();) {
            const unsigned char ch = source[i];
            if (std::isspace(ch)) { ++i; continue; }
            const size_t start = i++;
            if (std::isalnum(ch) || ch == '_' || ch == '-') {
                while (i < source.size() &&
                       (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            }
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
    std::vector<std::string> tokens_;
    size_t position_ = 0;

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
        const auto name = take();
        if (name == "Int64") return Type::Int;
        if (name == "Bool") return Type::Bool;
        if (name == "RefInt64") return Type::RefInt;
        if (name == "RefBool") return Type::RefBool;
        throw std::logic_error("unknown type in checked source");
    }
    Atom atom() {
        if (peek("!")) {
            take();
            return {Atom::Kind::Dereference, take()};
        }
        auto text = take();
        if (text == "true" || text == "false") return {Atom::Kind::Literal, std::move(text)};
        if (text.front() == '-' || std::isdigit(static_cast<unsigned char>(text.front()))) {
            const bool negative = text.front() == '-';
            const size_t digits = negative ? 1 : 0;
            const size_t first = text.find_first_not_of('0', digits);
            text = first == std::string::npos ? "0" : (negative ? "-" : "") + text.substr(first);
            return {Atom::Kind::Literal, std::move(text)};
        }
        return {Atom::Kind::Name, std::move(text)};
    }
    Expression expression() {
        Expression result;
        if (position_ + 1 < tokens_.size() && tokens_[position_ + 1] == "(") {
            result.callee = take();
            expect("(");
            while (!peek(")")) {
                result.arguments.push_back(atom());
                if (peek(")")) break;
                expect(",");
            }
            expect(")");
        } else result.atom = atom();
        return result;
    }
    std::vector<Statement> block() {
        expect("{");
        std::vector<Statement> body;
        while (!peek("}")) body.push_back(statement());
        expect("}");
        return body;
    }
    Statement statement() {
        const auto keyword = take();
        Statement result;
        if (keyword == "let") {
            result.kind = Statement::Kind::Let;
            result.name = take();
            expect(":");
            result.type = type();
            expect("=");
            result.value = expression();
        } else if (keyword == "assign") {
            result.kind = Statement::Kind::Assign;
            expect("(");
            result.name = take();
            expect(",");
            result.value.atom = atom();
            expect(")");
        } else if (keyword == "if" || keyword == "while") {
            result.kind = keyword == "if" ? Statement::Kind::If : Statement::Kind::While;
            expect("(");
            result.value.atom = atom();
            expect(")");
            result.body = block();
            if (keyword == "if") {
                expect("else");
                result.alternative = block();
            }
            return result;
        } else if (keyword == "return") {
            result.kind = Statement::Kind::Return;
            result.value.atom = atom();
        } else if (keyword == "break") result.kind = Statement::Kind::Break;
        else if (keyword == "continue") result.kind = Statement::Kind::Continue;
        else throw std::logic_error("unknown statement in checked source");
        expect(";");
        return result;
    }
    Function function(bool main) {
        if (!main) expect("func");
        Function result;
        result.name = take();
        if (main && result.name != "main") throw std::logic_error("missing main in checked source");
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
        result.body = block();
        return result;
    }
};

class Emitter {
public:
    std::string program(const std::vector<Function> & functions) {
        for (const auto & function : functions) emit_function(function);
        std::string result;
        if (int_cells_) result += cell_class("TypedIntCell", "Int64");
        if (bool_cells_) result += cell_class("TypedBoolCell", "Bool");
        return result + output_;
    }

private:
    std::string output_;
    size_t depth_ = 0;
    bool int_cells_ = false;
    bool bool_cells_ = false;

    // Disjoint prefixes avoid collisions with Cangjie keywords, runtime classes,
    // and later function names that an earlier body used as a local variable.
    static std::string variable(const std::string & name) { return "typed_var_" + name; }
    static std::string callable(const std::string & name) { return "typed_fn_" + name; }
    std::string type(Type value) {
        switch (value) {
            case Type::Int: return "Int64";
            case Type::Bool: return "Bool";
            case Type::RefInt: int_cells_ = true; return "TypedIntCell";
            case Type::RefBool: bool_cells_ = true; return "TypedBoolCell";
        }
        throw std::logic_error("unknown checked type");
    }
    static std::string cell_class(const std::string & name, const std::string & value_type) {
        return "class " + name + " {\n    public var value: " + value_type +
            "\n    public init(value: " + value_type + ") {\n        this.value = value\n    }\n}\n\n";
    }
    void line(const std::string & text) { output_ += std::string(depth_ * 4, ' ') + text + '\n'; }
    static std::string atom(const Atom & value) {
        if (value.kind == Atom::Kind::Literal) return value.text;
        return variable(value.text) + (value.kind == Atom::Kind::Dereference ? ".value" : "");
    }
    std::string expression(const Expression & value) {
        if (value.atom) return atom(*value.atom);
        const auto & name = value.callee;
        const auto & args = value.arguments;
        const char * op = name == "add" ? "+" : name == "sub" ? "-" : name == "mul" ? "*" :
                          name == "eq" ? "==" : name == "lt" ? "<" :
                          name == "and" ? "&&" : name == "or" ? "||" : nullptr;
        if (op) return "(" + atom(args.at(0)) + " " + op + " " + atom(args.at(1)) + ")";
        if (name == "not") return "(!" + atom(args.at(0)) + ")";
        if (name == "refInt") {
            int_cells_ = true;
            return "TypedIntCell(" + atom(args.at(0)) + ")";
        }
        if (name == "refBool") {
            bool_cells_ = true;
            return "TypedBoolCell(" + atom(args.at(0)) + ")";
        }
        std::string result = callable(name) + "(";
        for (size_t i = 0; i < args.size(); ++i) result += (i ? ", " : "") + atom(args[i]);
        return result + ")";
    }
    // Used only to decide whether a fallthrough trap is needed in generated CJ.
    // It does not reject source or change the prefix checker's return rules.
    bool block(const std::vector<Statement> & statements) {
        bool falls_through = true;
        for (const auto & statement : statements) {
            bool continues = true;
            switch (statement.kind) {
                case Statement::Kind::Let: {
                    const auto declared_type = type(statement.type);
                    const auto rhs = expression(statement.value);
                    line("let " + variable(statement.name) + ": " + declared_type + " = " + rhs);
                    break;
                }
                case Statement::Kind::Assign:
                    line(variable(statement.name) + ".value = " + expression(statement.value));
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
                    line("return " + expression(statement.value));
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
        if (!output_.empty()) output_ += '\n';
        std::string header = function.name == "main" ? "main(" : "func " + callable(function.name) + "(";
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
