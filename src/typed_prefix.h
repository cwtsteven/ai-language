#pragma once

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace typed {

// Base data types are the named ones. List carries its element type. A struct is
// nominal by name. A function type exists only once an annotation or a lambda
// signature has written it. A type parameter exists only inside the generic
// function that declared it.
struct Type {
    enum class Kind { Int, Bool, List, String, Fun, Param, Struct };
    Kind kind = Kind::Int;
    std::string param;
    std::vector<Type> parameters;
    std::shared_ptr<Type> result;

    static Type Int();
    static Type Bool();
    static Type List(Type element);
    static Type String();
    static Type Param(std::string name);
    static Type Struct(std::string name);
    static Type Fun(std::vector<Type> parameters, Type result);

    bool operator==(const Type & other) const;
    bool operator!=(const Type & other) const { return !(*this == other); }
    const Type * list_element() const;
};

std::string type_name(const Type & type);

struct Symbol {
    std::string name;
    Type type;
    bool mutable_binding = false;
};

struct Diagnostic {
    size_t line = 1;    // 1-based
    size_t column = 1;  // 1-based
    size_t offset = 0;  // 0-based byte offset into the source so far
    size_t length = 1;  // highlighted span in bytes
    std::string message;
};

// Print a compiler-style diagnostic for source under path (e.g. "<input>" or "<stdin>").
void print_diagnostic(FILE * out, std::string_view path, std::string_view source,
                      const Diagnostic & diagnostic);

// The accepted language is specified in docs/language.md. Each public operation is
// transactional: a rejected model token never changes the checker state.
class Prefix {
public:
    bool append(std::string_view bytes);
    bool can_append(std::string_view bytes) const;
    bool can_end() const;
    // Construct a valid suffix from any viable prefix. The sampler uses this
    // only after its model-token budget is exhausted.
    std::optional<std::string> completion() const;
    // Diagnostic state for decoder tracing.
    std::string expected_for_debug() const;
    std::string_view partial_for_debug() const { return pending_; }
    Prefix state_before_rejection_for_debug(std::string_view bytes) const;
    const std::vector<Symbol> & symbols() const { return symbols_; }

    // Set when the most recent append rejected its argument. Cleared on success.
    const std::optional<Diagnostic> & diagnostic() const { return diagnostic_; }
    // For a viable but unfinished prefix (format/cj): unexpected end of input.
    Diagnostic end_diagnostic() const;

private:
    enum class Phase {
        TopLevel, OpenParen, CloseParen, Colon, MainType, MainOpen,
        FunctionName, FunctionGenericOrParen, TypeParamName, TypeParamNext,
        FunctionParen, ParamStart, ParamName, ParamColon, ParamType,
        AfterParam, FunctionColon, FunctionType, FunctionOpen,
        StructName, StructOpen, StructMember, FieldColon, FieldSemi,
        MethodName, MethodParen, MethodParamStart, MethodParamName, MethodParamColon,
        MethodAfterParam, MethodColon, MethodOpen,
        Stmt, LetName, LetColon, LetType, LetEqual, ExprStart,
        ExprAfterLeft, InfixRight, IndexArg, IndexClose, UnaryArg,
        IfAtom, WhileAtom, DotName, MemberCallOpen,
        CallTypeOpen, CallTypeArg, CallTypeNext,
        CallOpen, CallArg, CallAfterArg, CallClose, LetSemi,
        LambdaParen, LambdaParamStart, LambdaParamName, LambdaParamColon, LambdaParamType,
        LambdaAfterParam, LambdaColon, LambdaResult, LambdaEq, LambdaGt,
        TypeAtom, TypeAfter, TypeArrow, TypeArrowGt, TypeListOpen, TypeListClose,
        ListCtorOpen, ListCtorClose,
        AssignEqual, LoopControlSemi,
        IfOpenParen, IfCloseParen, IfOpen, AfterIfThen, ElseOpen,
        WhileOpenParen, WhileCloseParen, WhileOpen,
        ForOpenParen, ForName, ForColon, ForIn, ForCloseParen, ForOpen,
        ReturnSemi, Done,
    };
    enum class Block { Main, Function, Method, IfThen, IfElse, While, For };
    enum class ExprFollow {
        LetSemi, ReturnSemi, IfClose, WhileClose, ForIterable, ForRangeEnd,
        LambdaEnd, Nested, Index, Group, Arg
    };
    enum class ResumeKind { Infix, Index, Group, Arg, Unary };
    enum class TypeDest {
        Let, Param, Return, LambdaParam, LambdaResult, CallArg, Field, MethodReturn,
        ListCtor, ListElement, For
    };
    struct TypeBuild {
        std::vector<Type> parameters;
        bool result = false;
        bool list = false;
    };
    struct LambdaFrame {
        ExprFollow follow = ExprFollow::LetSemi;
        Type expr_needed = Type::Int();
        Type signature = Type::Int();
        size_t base = 0;
        size_t param = 0;
        int min_prec = 0;
        std::string name;
    };
    struct ExprFrame {
        ResumeKind kind = ResumeKind::Infix;
        Type resume_type = Type::Int();
        Type needed = Type::Int();
        ExprFollow follow = ExprFollow::LetSemi;
        Type left = Type::Int();
        int min_prec = 0;
        std::vector<Type> call_arguments;
        size_t argument = 0;
        std::string call_name;
        std::vector<Type> call_type_args;
        Type call_result = Type::Int();
        Type infix_right = Type::Int();
    };
    struct Function {
        std::string name;
        std::vector<std::string> type_params;
        Type result = Type::Int();
        std::vector<Type> arguments;
    };
    struct Method {
        std::string name;
        Type result = Type::Int();
        std::vector<Type> arguments;
        std::vector<std::string> argument_names;
    };
    struct StructDecl {
        std::string name;
        std::vector<Symbol> fields;
        std::vector<Method> methods;
    };
    struct Frame {
        Block kind;
        size_t symbol_count;
        size_t struct_index = 0;
        size_t method_index = 0;
    };

    Phase phase_ = Phase::TopLevel;
    std::string pending_;
    std::vector<Symbol> symbols_;
    std::vector<Frame> blocks_;
    std::vector<Function> functions_;
    std::vector<StructDecl> structs_;
    Function declaring_;
    StructDecl declaring_struct_;
    Method declaring_method_;
    Type return_type_ = Type::Int();
    std::string new_name_;
    Type new_type_ = Type::Int();
    Type expr_needed_ = Type::Int();
    Type left_type_ = Type::Int();
    Type infix_right_type_ = Type::Int();
    int min_prec_ = 0;
    Type call_result_ = Type::Int();
    std::vector<ExprFrame> exprs_;
    ExprFollow saved_follow_ = ExprFollow::LetSemi;
    Type saved_needed_ = Type::Int();
    Type lambda_result_ = Type::Int();
    size_t lambda_base_ = 0;
    size_t lambda_param_ = 0;
    std::vector<LambdaFrame> lambdas_;
    std::vector<std::string> type_params_;
    std::vector<Type> call_type_args_;
    std::string call_name_;
    std::vector<TypeBuild> type_stack_;
    TypeDest type_dest_ = TypeDest::Let;
    bool type_at_result_ = false;
    ExprFollow expr_follow_ = ExprFollow::LetSemi;
    bool assignment_ = false;
    bool mutable_binding_ = false;
    bool quoting_ = false;
    bool after_return_ = false; // Prefer a closing brace during deterministic completion.
    std::vector<Type> call_arguments_;
    size_t argument_ = 0;
    size_t offset_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
    size_t token_start_offset_ = 0;
    size_t token_start_line_ = 1;
    size_t token_start_column_ = 1;
    std::optional<Diagnostic> diagnostic_;

    bool step(unsigned char ch);
    bool commit(std::string_view token);
    bool possible(std::string_view partial) const;
    bool atom_possible(Type type, std::string_view partial) const;
    bool atom_complete(Type type, std::string_view token) const;
    bool symbol_prefix(Type type, std::string_view partial) const;
    bool mutable_prefix(std::string_view partial) const;
    bool has_atom(Type type) const;
    bool has_atom(Type type, std::vector<std::string> & visiting) const;
    // True when finishing `type` in the current type_dest_/type_stack_ can succeed.
    bool type_atom_acceptable(const Type & type) const;
    // Value-struct field cycles (Cangjie rejects these); List fields do not count.
    bool struct_field_embeds(const Type & type, const std::string & origin,
                             std::vector<std::string> & visiting) const;
    bool name_available(std::string_view name) const;
    bool struct_name_available(std::string_view name) const;
    const Symbol * find(std::string_view name) const;
    const Function * find_function(std::string_view name) const;
    const StructDecl * find_struct(std::string_view name) const;
    const Method * find_method(const Type & receiver, std::string_view name) const;
    const Symbol * find_field(const Type & receiver, std::string_view name) const;
    bool operation_possible(Type result, std::string_view partial) const;
    bool begin_operation(std::string_view name, Type result);
    bool list_ctor_possible(Type needed, std::string_view partial) const;
    bool struct_ctor_possible(Type needed, std::string_view partial) const;
    bool begin_list_ctor();
    bool begin_struct_ctor(std::string_view name);
    bool member_possible(Type left, Type needed, std::string_view partial) const;
    bool close_block();
    bool in_loop() const;
    bool begin_expression(Type needed, ExprFollow follow);
    bool finish_expression();
    bool for_list_complete() const;
    bool for_range_start_complete() const;
    bool for_iterable_start_possible(std::string_view partial) const;
    bool value_start_possible(Type needed, std::string_view partial) const;
    bool symbol_callable(const Symbol & symbol, Type needed) const;
    bool left_can_reach(Type left, Type needed) const;
    bool can_reach(Type from, Type needed) const;
    bool become_primary(Type type);
    bool push_subexpr(ResumeKind kind, Type resume_type, Type needed, ExprFollow follow, int min_prec);
    bool end_subexpr(std::string_view token, bool consume);
    bool take_infix(std::string_view token);
    bool start_arg();
    bool infix_possible(Type left, Type needed, std::string_view partial) const;
    bool index_possible(Type left, Type needed) const;
    std::string completion_token() const;
    std::string completion_atom(Type type) const;
    std::string diagnose_message(std::string_view lexeme) const;
    Type expected_atom_type() const;
    bool expects_atom() const;
    bool string_literal_context() const;
    bool atom_start_phase() const;
    bool type_param_named(std::string_view name) const;
    std::optional<Type> base_or_param(std::string_view token) const;
    Type substitute(const Type & type, const std::vector<Type> & args) const;
    bool instantiate_possible(const Function & function, const Type & needed) const;
    std::vector<Type> type_choices(const Type & needed) const;
    void collect_subtypes(const Type & type, std::vector<Type> & out) const;
    bool begin_type(TypeDest dest);
    bool deliver_type(Type type);
    bool finish_type(const Type & type);
    Type function_type(const Function & function) const;
    bool member_name_taken(const StructDecl & decl, std::string_view name) const;
};

} // namespace typed
