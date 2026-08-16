// parser.h - recursive descent HLSL expression/statement parser
#pragma once

#include <string>
#include <vector>

#include "ast.h"
#include "lexer.h"

namespace hb {

class Parser {
public:
    Parser(Arena& arena, StmtArena& stmt_arena);

    // Parse a single expression (no trailing statement).
    // Returns nullptr on error.
    Expr* parse_expression(const std::string& text);

    // Parse a sequence of statements from text. Returns true on success.
    // Statements are appended to `out` (owned by stmt_arena).
    bool parse_statements(const std::string& text, std::vector<Stmt*>& out);

    // Parse a single statement.
    Stmt* parse_statement(const std::string& text);

    const std::string& error() const { return error_; }
    void clear_error() { error_.clear(); }

private:
    Arena& arena_;
    StmtArena& stmt_arena_;
    std::vector<Token> toks_;
    size_t pos_ = 0;
    std::string error_;

    const Token& cur() const { return toks_[pos_]; }
    const Token& peek(int off = 1) const {
        size_t p = pos_ + off;
        if (p >= toks_.size()) p = toks_.size() - 1;
        return toks_[p];
    }
    bool at_end() const { return cur().kind == TokKind::End; }
    bool is_punct(const char* p) const { return cur().kind == TokKind::Punct && cur().text == p; }
    bool accept_punct(const char* p);
    bool expect_punct(const char* p);
    bool is_ident(const char* n) const { return cur().kind == TokKind::Ident && cur().text == n; }
    bool accept_ident(const char* n);

    void set_error(const std::string& msg) { if (error_.empty()) error_ = msg; }

    // Expression precedence levels
    Expr* parse_ternary();
    Expr* parse_binary(int min_prec);
    Expr* parse_unary();
    Expr* parse_postfix();
    Expr* parse_primary();

    Expr* make_binop(const std::string& op, Expr* l, Expr* r);
    Expr* make_compare(const std::string& op, Expr* l, Expr* r);
    static int binop_precedence(const std::string& op); // 0 if not binary
    static bool is_compare_op(const std::string& op);

    // Statements
    Stmt* parse_statement_inner();
    Stmt* parse_block();
    Stmt* parse_if();
    Stmt* parse_while();
    Stmt* parse_for();
    Stmt* parse_switch();
    Stmt* parse_decl_or_assign();
    bool looks_like_type_name(const std::string& ident) const;
};

// Convenience: parse a single expression, returns nullptr on failure.
Expr* parse_expr_text(const std::string& text, Arena& arena, std::string& err);

} // namespace hb
