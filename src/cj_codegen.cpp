#include "cj_codegen.h"
#include "typed_prefix.h"

#include <cctype>
#include <map>
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
    bool member = false;
    std::vector<Symbol> lambda_params;
    std::shared_ptr<Expression> lambda_body;
};

struct Statement {
    enum class Kind { Let, Var, Assign, If, While, For, Return, Break, Continue } kind;
    std::string name;
    Type type = Type::Int();
    Expression value;
    Expression range_end;
    bool range = false;
    bool has_else = false;
    std::vector<Statement> body;
    std::vector<Statement> alternative;
};

struct Method {
    std::string name;
    Type result;
    std::vector<Symbol> parameters;
    std::vector<Statement> body;
};

struct StructDecl {
    std::string name;
    std::vector<Symbol> fields;
    std::vector<Method> methods;
};

struct Function {
    std::string name;
    std::vector<std::string> type_params;
    Type result;
    std::vector<Symbol> parameters;
    std::vector<Statement> body;
};

class Parser {
public:
    explicit Parser(std::string_view source) {
        auto expects_atom = [](std::string_view previous) {
            return previous == "=" || previous == "(" || previous == "," || previous == "return" ||
                previous == "!" || previous == "[" || previous == "." || previous == "+" ||
                previous == "-" || previous == "*" || previous == "/" || previous == "%" ||
                previous == "==" || previous == "!=" || previous == "<" || previous == "<=" ||
                previous == ">" || previous == ">=" || previous == "&&" || previous == "||" ||
                previous == ".." || previous == "in";
        };
        for (size_t i = 0; i < source.size();) {
            const unsigned char ch = static_cast<unsigned char>(source[i]);
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
            } else if ((ch == '!' || ch == '<' || ch == '>') && i < source.size() && source[i] == '=') {
                const bool after_type =
                    previous == "Int64" || previous == "Bool" || previous == "String" ||
                    previous == "List" || previous == ">" || previous == ")" ||
                    (!previous.empty() && previous.front() >= 'A' && previous.front() <= 'Z');
                if (ch == '!' || !after_type) ++i;
            } else if (ch == '.' && i < source.size() && source[i] == '.') {
                ++i;
            } else if (std::isalnum(ch) || ch == '_') {
                while (i < source.size() &&
                       (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            }
            tokens_.emplace_back(source.substr(start, i - start));
        }
    }

    std::vector<Function> program() {
        while (peek("struct")) structs_.push_back(struct_decl());
        std::vector<Function> functions;
        while (peek("func")) functions.push_back(function(false));
        functions.push_back(function(true));
        if (position_ != tokens_.size()) throw std::logic_error("unparsed source after main");
        return functions;
    }

    const std::vector<StructDecl> & structs() const { return structs_; }

private:
    struct Binding { std::string name; Type type; };

    std::vector<std::string> tokens_;
    size_t position_ = 0;
    std::vector<Binding> symbols_;
    std::vector<std::string> function_names_;
    std::vector<StructDecl> structs_;

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
    bool struct_name(std::string_view name) const {
        if (name.empty() || name.front() < 'A' || name.front() > 'Z') return false;
        for (const auto & decl : structs_) if (decl.name == name) return true;
        return false;
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
        if (peek("List")) {
            take();
            expect("<");
            Type element = type();
            expect(">");
            return Type::List(std::move(element));
        }
        const auto name = take();
        if (name == "Int64") return Type::Int();
        if (name == "Bool") return Type::Bool();
        if (name == "String") return Type::String();
        if (struct_name(name)) return Type::Struct(name);
        return Type::Param(name);
    }
    Type lookup(const std::string & name) const {
        for (auto binding = symbols_.rbegin(); binding != symbols_.rend(); ++binding) {
            if (binding->name == name) return binding->type;
        }
        throw std::logic_error("unbound name in checked source");
    }
    Type type_of_expr(const Expression & expr) const {
        if (expr.callee == "[]" && !expr.arguments.empty()) {
            if (expr.arguments[0].atom) return lookup(expr.arguments[0].atom->text).list_element() ?
                *lookup(expr.arguments[0].atom->text).list_element() : Type::Int();
        }
        if (expr.member && expr.callee == "size") return Type::Int();
        if (expr.member && expr.callee == "add" && !expr.arguments.empty()) {
            if (expr.arguments[0].atom) return lookup(expr.arguments[0].atom->text);
        }
        if (expr.atom) return lookup(expr.atom->text);
        return Type::Int();
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
        if (op == "==" || op == "!=" || op == "<" || op == "<=" ||
            op == ">" || op == ">=") return 3;
        if (op == "+" || op == "-") return 4;
        if (op == "*" || op == "/" || op == "%") return 5;
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
        if (peek("List")) {
            take();
            expect("<");
            Type element = type();
            expect(">");
            expect("(");
            expect(")");
            result.callee = "ListCtor";
            result.type_arguments.push_back(std::move(element));
            return result;
        }
        const bool generic_call = position_ + 1 < tokens_.size() && tokens_[position_ + 1] == "<" &&
            known_function(tokens_[position_]);
        const bool direct_call = position_ + 1 < tokens_.size() && tokens_[position_ + 1] == "(";
        const bool struct_ctor = position_ + 1 < tokens_.size() && tokens_[position_ + 1] == "(" &&
            struct_name(tokens_[position_]);
        if (generic_call || direct_call || struct_ctor) {
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
        while (true) {
            if (peek(".") && 7 >= min_prec) {
                take();
                Expression member;
                member.member = true;
                member.callee = take();
                Type receiver_type = Type::Int();
                if (left.atom && left.atom->kind == Atom::Kind::Name)
                    receiver_type = lookup(left.atom->text);
                else if (left.callee == "ListCtor" && !left.type_arguments.empty())
                    receiver_type = Type::List(left.type_arguments[0]);
                member.arguments = { std::move(left) };
                if (member.callee == "add" && receiver_type.kind == Type::Kind::List &&
                    receiver_type.list_element()) {
                    member.type_arguments.push_back(*receiver_type.list_element());
                }
                if (peek("(")) {
                    take();
                    while (!peek(")")) {
                        member.arguments.push_back(parse_expr(0));
                        if (peek(")")) break;
                        expect(",");
                    }
                    expect(")");
                }
                left = std::move(member);
                continue;
            }
            if (peek("[") && 7 >= min_prec) {
                const Type receiver = left.atom ? lookup(left.atom->text) : Type::Int();
                const bool list = receiver.kind == Type::Kind::List;
                expect("[");
                Expression index = parse_expr(0);
                expect("]");
                Expression indexed;
                indexed.callee = list ? "[]" : "str[]";
                indexed.arguments = { std::move(left), std::move(index) };
                left = std::move(indexed);
                continue;
            }
            int precedence = 0;
            std::string op;
            for (auto candidate : { "||", "&&", "==", "!=", "<=", ">=", "<", ">",
                                    "+", "-", "*", "/", "%" }) {
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
            if (keyword == "if" && peek("else")) {
                take();
                result.has_else = true;
                result.alternative = block();
            }
            return result;
        } else if (keyword == "for") {
            result.kind = Statement::Kind::For;
            expect("(");
            result.name = take();
            expect(":");
            result.type = type();
            expect("in");
            result.value = expression();
            if (peek("..")) {
                take();
                result.range = true;
                result.range_end = expression();
            }
            expect(")");
            const size_t scope = symbols_.size();
            symbols_.push_back({result.name, result.type});
            expect("{");
            while (!peek("}")) result.body.push_back(statement());
            expect("}");
            symbols_.resize(scope);
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
    StructDecl struct_decl() {
        expect("struct");
        StructDecl result;
        result.name = take();
        expect("{");
        while (!peek("}")) {
            if (peek("func")) {
                expect("func");
                Method method;
                method.name = take();
                expect("(");
                while (!peek(")")) {
                    Symbol parameter;
                    parameter.name = take();
                    expect(":");
                    parameter.type = type();
                    method.parameters.push_back(std::move(parameter));
                    if (peek(")")) break;
                    expect(",");
                }
                expect(")");
                expect(":");
                method.result = type();
                expect("{");
                const size_t scope = symbols_.size();
                symbols_.push_back({ "this", Type::Struct(result.name) });
                for (const auto & field : result.fields) symbols_.push_back({ field.name, field.type });
                for (const auto & parameter : method.parameters) symbols_.push_back({ parameter.name, parameter.type });
                method.body = block_body();
                symbols_.resize(scope);
                result.methods.push_back(std::move(method));
            } else {
                Symbol field;
                field.name = take();
                expect(":");
                field.type = type();
                expect(";");
                result.fields.push_back(std::move(field));
            }
        }
        expect("}");
        return result;
    }
    std::vector<Statement> block_body() {
        std::vector<Statement> body;
        while (!peek("}")) body.push_back(statement());
        expect("}");
        return body;
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
    explicit Emitter(std::vector<StructDecl> structs) : structs_(std::move(structs)) {}

    std::string program(const std::vector<Function> & functions) {
        for (const auto & decl : structs_) emit_struct(decl);
        for (const auto & function : functions) emit_function(function);
        std::string body = std::move(output_);
        output_.clear();
        depth_ = 0;
        if (needs_list_) line("import std.collection.ArrayList");
        for (const auto & helper : list_helpers_) emit_list_helper(helper);
        if (needs_character_) {
            if (!output_.empty()) output_ += '\n';
            line("func typed_rt_str_at(typed_rt_text: String, typed_rt_index: Int64): String {");
            ++depth_;
            line("let typed_rt_chars = typed_rt_text.toRuneArray()");
            line("String([typed_rt_chars[typed_rt_index]])");
            --depth_;
            line("}");
        }
        for (const auto & copy : struct_copies_) emit_struct_copy(copy);
        if (!output_.empty()) output_ += '\n';
        output_ += body;
        return output_;
    }

private:
    std::vector<StructDecl> structs_;
    std::string output_;
    size_t depth_ = 0;
    bool needs_list_ = false;
    bool needs_character_ = false;
    Type return_type_ = Type::Int();
    std::vector<std::string> list_helpers_;
    std::vector<std::string> struct_copies_;
    const StructDecl * current_struct_ = nullptr;

    static std::string variable(const std::string & name) { return "typed_var_" + name; }
    static std::string callable(const std::string & name) { return "typed_fn_" + name; }
    static std::string helper_token(std::string text) {
        for (char & ch : text) {
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9'))) ch = '_';
        }
        return text;
    }

    std::string type(Type value) {
        switch (value.kind) {
            case Type::Kind::Int: return "Int64";
            case Type::Kind::Bool: return "Bool";
            case Type::Kind::String: return "String";
            case Type::Kind::List: {
                needs_list_ = true;
                if (!value.list_element()) return "ArrayList<Int64>";
                const std::string elem = type(*value.list_element());
                register_list_helper(elem);
                return "ArrayList<" + elem + ">";
            }
            case Type::Kind::Struct:
                register_struct_copy(value.param);
                return value.param;
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

    void register_list_helper(const std::string & element) {
        if (std::find(list_helpers_.begin(), list_helpers_.end(), element) != list_helpers_.end()) return;
        list_helpers_.push_back(element);
    }

    void emit_list_helper(const std::string & element) {
        const std::string list_type = "ArrayList<" + element + ">";
        const std::string name = "typed_rt_list_add_" + helper_token(element);
        line("func " + name + "(typed_rt_items: " + list_type + ", typed_rt_value: " +
             element + "): " + list_type + " {");
        ++depth_;
        line("let typed_rt_copy = typed_rt_items.clone()");
        line("typed_rt_copy.add(typed_rt_value)");
        line("typed_rt_copy");
        --depth_;
        line("}");
    }

    void register_struct_copy(const std::string & name) {
        if (std::find(struct_copies_.begin(), struct_copies_.end(), name) == struct_copies_.end())
            struct_copies_.push_back(name);
    }

    void emit_struct_copy(const std::string & name) {
        const auto * decl = find_struct(name);
        if (!decl) return;
        line("func typed_rt_copy_" + name + "(typed_rt_value: " + name + "): " + name + " {");
        ++depth_;
        std::string args;
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            if (i) args += ", ";
            const auto & field = decl->fields[i];
            std::string field_expr = "typed_rt_value." + variable(field.name);
            if (field.type.kind == Type::Kind::List) {
                register_list_helper(type(*field.type.list_element()));
                field_expr = field_expr + ".clone()";
            } else if (field.type.kind == Type::Kind::Struct) {
                field_expr = "typed_rt_copy_" + field.type.param + "(" + field_expr + ")";
            }
            args += field_expr;
        }
        line(name + "(" + args + ")");
        --depth_;
        line("}");
    }

    const StructDecl * find_struct(const std::string & name) const {
        for (const auto & decl : structs_) if (decl.name == name) return &decl;
        return nullptr;
    }

    bool needs_copy(Type destination) const {
        if (destination.kind == Type::Kind::List) return true;
        if (destination.kind == Type::Kind::Struct) return true;
        return false;
    }

    std::string stored(Type destination, const Expression & value) {
        std::string text = expression(value);
        if (needs_copy(destination) && value.atom && value.atom->kind == Atom::Kind::Name) {
            if (destination.kind == Type::Kind::List) text += ".clone()";
            if (destination.kind == Type::Kind::Struct)
                text = "typed_rt_copy_" + destination.param + "(" + text + ")";
        }
        return text;
    }

    void line(const std::string & text) { output_ += std::string(depth_ * 4, ' ') + text + '\n'; }

    std::string atom(const Atom & value) {
        if (value.kind == Atom::Kind::Literal) return value.text;
        if (value.function_ref) return callable(value.text);
        if (current_struct_) {
            for (const auto & field : current_struct_->fields) {
                if (field.name == value.text) return "this." + variable(value.text);
            }
        }
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
        if (value.callee == "ListCtor" && !value.type_arguments.empty()) {
            needs_list_ = true;
            const std::string elem = type(value.type_arguments[0]);
            register_list_helper(elem);
            return "ArrayList<" + elem + ">()";
        }
        if (value.member) {
            const std::string recv = expression(value.arguments.at(0));
            if (value.callee == "size") return "(" + recv + ".size)";
            if (value.callee == "add" && value.arguments.size() >= 2) {
                std::string elem = "Int64";
                if (!value.type_arguments.empty()) elem = type(value.type_arguments[0]);
                register_list_helper(elem);
                return "typed_rt_list_add_" + helper_token(elem) + "(" + recv + ", " +
                       expression(value.arguments[1]) + ")";
            }
            if (value.arguments.size() == 1) return recv + "." + variable(value.callee);
            std::string call = recv + "." + value.callee + "(";
            for (size_t i = 1; i < value.arguments.size(); ++i)
                call += (i > 1 ? ", " : "") + expression(value.arguments[i]);
            return call + ")";
        }
        if (value.atom) {
            if (value.function_ref) return callable(value.atom->text);
            return atom(*value.atom);
        }
        const auto & name = value.callee;
        const auto & args = value.arguments;
        if (name == "+" || name == "-" || name == "*" || name == "/" || name == "%" ||
            name == "==" || name == "!=" || name == "<" || name == "<=" ||
            name == ">" || name == ">=" || name == "&&" || name == "||") {
            return "(" + expression(args.at(0)) + " " + name + " " + expression(args.at(1)) + ")";
        }
        if (name == "!") return "(!" + expression(args.at(0)) + ")";
        if (name == "[]") {
            needs_list_ = true;
            return "(" + expression(args.at(0)) + "[" + expression(args.at(1)) + "])";
        }
        if (name == "strLen") return "(" + expression(args.at(0)) + ".size)";
        if (name == "str[]") {
            needs_character_ = true;
            return "typed_rt_str_at(" + expression(args.at(0)) + ", " + expression(args.at(1)) + ")";
        }
        std::string result = value.callee_is_function ? callable(name) : name;
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
                    if (statement.has_else) {
                        line("} else {");
                        ++depth_;
                        const bool else_continues = block(statement.alternative);
                        --depth_;
                        line("}");
                        continues = then_continues || else_continues;
                    } else {
                        line("}");
                        continues = then_continues;
                    }
                    break;
                }
                case Statement::Kind::While:
                    line("while (" + expression(statement.value) + ") {");
                    ++depth_;
                    block(statement.body);
                    --depth_;
                    line("}");
                    break;
                case Statement::Kind::For: {
                    std::string header = "for (" + variable(statement.name) + " in ";
                    if (statement.range) {
                        header += expression(statement.value) + ".." + expression(statement.range_end);
                    } else {
                        header += expression(statement.value);
                    }
                    header += ") {";
                    line(header);
                    ++depth_;
                    block(statement.body);
                    --depth_;
                    line("}");
                    break;
                }
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

    void emit_struct(const StructDecl & decl) {
        register_struct_copy(decl.name);
        if (!output_.empty()) output_ += '\n';
        line("struct " + decl.name + " {");
        ++depth_;
        for (const auto & field : decl.fields)
            line("let " + variable(field.name) + ": " + type(field.type));
        line("init(" + struct_init_params(decl) + ") {");
        ++depth_;
        for (const auto & field : decl.fields)
            line("this." + variable(field.name) + " = " + variable(field.name));
        --depth_;
        line("}");
        for (const auto & method : decl.methods) emit_method(decl, method);
        --depth_;
        line("}");
    }

    std::string struct_init_params(const StructDecl & decl) {
        std::string text;
        for (size_t i = 0; i < decl.fields.size(); ++i) {
            if (i) text += ", ";
            text += variable(decl.fields[i].name) + ": " + type(decl.fields[i].type);
        }
        return text;
    }

    void emit_method(const StructDecl & decl, const Method & method) {
        current_struct_ = &decl;
        std::string header = "func " + method.name + "(";
        for (size_t i = 0; i < method.parameters.size(); ++i) {
            if (i) header += ", ";
            header += variable(method.parameters[i].name) + ": " + type(method.parameters[i].type);
        }
        header += "): " + type(method.result) + " {";
        line(header);
        ++depth_;
        return_type_ = method.result;
        block(method.body);
        --depth_;
        line("}");
        current_struct_ = nullptr;
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
    Parser parser(source);
    const auto functions = parser.program();
    return Emitter(parser.structs()).program(functions);
}

} // namespace typed