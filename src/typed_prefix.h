#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace typed {

enum class Type { Int, Bool, RefInt, RefBool };

struct Symbol {
    std::string name;
    Type type;
    bool mutable_binding = false;
};

// The accepted language is specified in README.md. Each public operation is
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

private:
    enum class Phase {
        Main, OpenParen, CloseParen, Colon, MainType, MainOpen,
        Stmt, LetName, LetColon, LetType, LetEqual, Rhs, RhsAfterAtom, RhsRight,
        CallOpen, CallArg, CallAfterArg, LetSemi,
        WriteOpen, WriteRef, WriteComma, WriteValue, WriteClose, WriteSemi,
        IfOpenParen, IfCondition, IfCloseParen, IfOpen, Else, ElseOpen,
        WhileOpenParen, WhileCondition, WhileCloseParen, WhileOpen,
        ReturnValue, ReturnSemi, MainClose, Done,
    };
    enum class Block { Main, IfThen, IfElse, While };
    struct Frame {
        Block kind;
        size_t symbol_count;
    };

    Phase phase_ = Phase::Main;
    std::string pending_;
    std::vector<Symbol> symbols_;
    std::vector<Frame> blocks_;
    std::string new_name_;
    Type new_type_ = Type::Int;
    Type rhs_lhs_type_ = Type::Int;
    Type rhs_right_type_ = Type::Int;
    bool new_mutable_ = false;
    bool assignment_ = false;
    Type value_type_ = Type::Int;
    std::string operation_;
    size_t argument_ = 0;

    bool step(unsigned char ch);
    bool commit(std::string_view token);
    bool possible(std::string_view partial) const;
    bool atom_possible(Type type, std::string_view partial) const;
    bool atom_complete(Type type, std::string_view token) const;
    bool symbol_prefix(Type type, std::string_view partial) const;
    bool mutable_symbol_prefix(std::string_view partial) const;
    bool has_atom(Type type) const;
    bool has_reference() const;
    bool name_available(std::string_view name) const;
    const Symbol * find(std::string_view name) const;
    bool operation_possible(Type result, std::string_view partial) const;
    bool begin_operation(std::string_view name, Type result);
    bool rhs_operator_possible(std::string_view partial) const;
    bool rhs_atom_possible(std::string_view partial) const;
    bool begin_rhs_operator(std::string_view token);
    bool close_block();
    std::string completion_token() const;
    std::string completion_atom(Type type) const;
};

} // namespace typed
