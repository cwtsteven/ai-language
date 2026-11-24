#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace typed {

enum class Type { Int, Bool, List, String };

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
        FunctionName, FunctionParen, ParamStart, ParamName, ParamColon, ParamType,
        AfterParam, FunctionColon, FunctionType, FunctionOpen,
        Stmt, LetName, LetColon, LetType, LetEqual, Rhs,
        CallOpen, CallArg, CallAfterArg, CallClose, LetSemi,
        AssignEqual, AssignValue, LoopControlSemi,
        IfOpenParen, IfCondition, IfCloseParen, IfOpen, Else, ElseOpen,
        WhileOpenParen, WhileCondition, WhileCloseParen, WhileOpen,
        ReturnValue, ReturnSemi, Done,
    };
    enum class Block { Main, Function, IfThen, IfElse, While };
    struct Function {
        std::string name;
        Type result = Type::Int;
        std::vector<Type> arguments;
    };
    struct Frame {
        Block kind;
        size_t symbol_count;
    };

    Phase phase_ = Phase::TopLevel;
    std::string pending_;
    std::vector<Symbol> symbols_;
    std::vector<Frame> blocks_;
    std::vector<Function> functions_;
    Function declaring_;
    Type return_type_ = Type::Int;
    std::string new_name_;
    Type new_type_ = Type::Int;
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
    bool name_available(std::string_view name) const;
    const Symbol * find(std::string_view name) const;
    const Function * find_function(std::string_view name) const;
    bool operation_possible(Type result, std::string_view partial) const;
    bool begin_operation(std::string_view name, Type result);
    bool close_block();
    bool in_loop() const;
    std::string completion_token() const;
    std::string completion_atom(Type type) const;
    std::string diagnose_message(std::string_view lexeme) const;
    Type expected_atom_type() const;
    bool expects_atom() const;
    bool string_literal_context() const;
};

} // namespace typed
