// parser.cpp
#include "parser.h"

#include <cctype>
#include <cstdlib>

namespace hb {

Parser::Parser(Arena& arena, StmtArena& stmt_arena)
    : arena_(arena), stmt_arena_(stmt_arena) {}

int Parser::binop_precedence(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "|") return 3;
    if (op == "^") return 4;
    if (op == "&") return 5;
    if (op == "==" || op == "!=") return 6;
    if (op == "<" || op == "<=" || op == ">" || op == ">=") return 7;
    if (op == "<<" || op == ">>") return 8;
    if (op == "+" || op == "-") return 9;
    if (op == "*" || op == "/" || op == "%") return 10;
    return 0;
}

bool Parser::is_compare_op(const std::string& op) {
    return op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
}

bool Parser::accept_punct(const char* p) {
    if (is_punct(p)) { ++pos_; return true; }
    return false;
}

bool Parser::expect_punct(const char* p) {
    if (is_punct(p)) { ++pos_; return true; }
    set_error(std::string("expected '") + p + "', got '" + cur().text + "'");
    return false;
}

bool Parser::accept_ident(const char* n) {
    if (is_ident(n)) { ++pos_; return true; }
    return false;
}

Expr* Parser::make_binop(const std::string& op, Expr* l, Expr* r) {
    Expr* e = arena_.alloc();
    e->kind = Expr::Kind::BinOp;
    e->op = op;
    e->left = l;
    e->right = r;
    return e;
}

Expr* Parser::make_compare(const std::string& op, Expr* l, Expr* r) {
    Expr* e = arena_.alloc();
    e->kind = Expr::Kind::CmpOp;
    e->op = op;
    e->left = l;
    e->right = r;
    return e;
}

Expr* Parser::parse_expression(const std::string& text) {
    toks_ = tokenize(text);
    pos_ = 0;
    error_.clear();
    Expr* e = parse_ternary();
    if (!at_end() && error_.empty())
        set_error("unexpected token '" + cur().text + "'");
    if (error_.empty())
        return e;
    return nullptr;
}

Expr* Parser::parse_ternary() {
    Expr* cond = parse_binary(1);
    if (!cond) return nullptr;
    if (!is_punct("?")) return cond;
    ++pos_;
    Expr* t = parse_ternary();
    if (!t) return nullptr;
    if (!expect_punct(":")) return nullptr;
    Expr* f = parse_ternary();
    if (!f) return nullptr;
    Expr* e = arena_.alloc();
    e->kind = Expr::Kind::Ternary;
    e->cond = cond;
    e->true_expr = t;
    e->false_expr = f;
    return e;
}

Expr* Parser::parse_binary(int min_prec) {
    Expr* left = parse_unary();
    if (!left) return nullptr;

    while (true) {
        if (cur().kind != TokKind::Punct) break;
        const std::string& op = cur().text;
        int prec = binop_precedence(op);
        if (prec == 0 || prec < min_prec) break;
        ++pos_;
        Expr* right = parse_binary(prec + 1);
        if (!right) return nullptr;
        if (is_compare_op(op)) {
            // Chained comparisons are rare in HLSL; treat as left-assoc compare.
            left = make_compare(op, left, right);
        } else if (op == "&&" || op == "||") {
            // logical ops lower to and/or on bools
            left = make_binop(op, left, right);
        } else {
            left = make_binop(op, left, right);
        }
    }
    return left;
}

Expr* Parser::parse_unary() {
    if (is_punct("-") || is_punct("+") || is_punct("!") || is_punct("~")) {
        std::string op = cur().text;
        ++pos_;
        Expr* operand = parse_unary();
        if (!operand) return nullptr;
        // Constant-fold unary minus / plus on numeric literals.
        if (operand->kind == Expr::Kind::ConstVec) {
            if (op == "-") {
                for (double& v : operand->elems) v = -v;
                return operand;
            }
            if (op == "+") return operand;
        }
        Expr* e = arena_.alloc();
        e->kind = Expr::Kind::UnaryOp;
        e->op = op;
        e->operand = operand;
        return e;
    }
    return parse_postfix();
}

Expr* Parser::parse_postfix() {
    Expr* e = parse_primary();
    if (!e) return nullptr;

    while (true) {
        if (is_punct(".")) {
            ++pos_;
            if (cur().kind != TokKind::Ident) {
                set_error("expected identifier after '.'");
                return nullptr;
            }
            std::string ident = cur().text;
            // Swizzle: all chars in xyzw and followed by something that isn't '('
            bool is_swizzle = true;
            for (char c : ident)
                if (std::string("xyzw").find(c) == std::string::npos) { is_swizzle = false; break; }
            if (is_swizzle && !is_punct("(") && ident.size() <= 4) {
                // Attach swizzle to the operand. For VarRef, merge masks.
                if (e->kind == Expr::Kind::VarRef && e->mask.empty()) {
                    e->mask = ident;
                } else if (e->kind == Expr::Kind::VarRef) {
                    // compound swizzle like a.xy.zw is invalid; keep first
                    e->mask = ident;
                } else {
                    Expr* m = arena_.alloc();
                    m->kind = Expr::Kind::Member;
                    m->operand = e;
                    m->name = ident;
                    e = m;
                }
                ++pos_;
                continue;
            }
            // Member access or Sample
            ++pos_;
            if (ident == "Sample" && is_punct("(")) {
                // tex.Sample(sampler, uv)
                ++pos_;
                Expr* sampler = parse_ternary();
                if (!sampler) return nullptr;
                if (!expect_punct(",")) return nullptr;
                Expr* uv = parse_ternary();
                if (!uv) return nullptr;
                if (!expect_punct(")")) return nullptr;
                Expr* s = arena_.alloc();
                s->kind = Expr::Kind::Sample;
                s->name = e->name; // texture name
                s->sampler_expr = sampler;
                s->uv_expr = uv;
                e = s;
                continue;
            }
            if (ident == "SampleLevel" && is_punct("(")) {
                // tex.SampleLevel(sampler, uv, lod) -> sample_l
                ++pos_;
                Expr* sampler = parse_ternary();
                if (!sampler) return nullptr;
                if (!expect_punct(",")) return nullptr;
                Expr* uv = parse_ternary();
                if (!uv) return nullptr;
                if (!expect_punct(",")) return nullptr;
                Expr* lod = parse_ternary();
                if (!lod) return nullptr;
                if (!expect_punct(")")) return nullptr;
                Expr* s = arena_.alloc();
                s->kind = Expr::Kind::Sample;
                s->name = e->name;
                s->sampler_expr = sampler;
                s->uv_expr = uv;
                s->false_expr = lod; // stash lod (SampleLevel)
                e = s;
                continue;
            }
            Expr* m = arena_.alloc();
            m->kind = Expr::Kind::Member;
            m->operand = e;
            m->name = ident;
            e = m;
            continue;
        }
        if (is_punct("(")) {
            // Function call on an expression (e.g., after VarRef ident already parsed as Call?)
            // If e is a VarRef with no mask, convert to Call.
            if (e->kind == Expr::Kind::VarRef) {
                ++pos_;
                std::vector<Expr*> args;
                if (!is_punct(")")) {
                    while (true) {
                        Expr* a = parse_ternary();
                        if (!a) return nullptr;
                        args.push_back(a);
                        if (accept_punct(",")) continue;
                        break;
                    }
                }
                if (!expect_punct(")")) return nullptr;
                Expr* c = arena_.alloc();
                c->kind = Expr::Kind::Call;
                c->name = e->name;
                c->args = args;
                e = c;
                continue;
            }
            set_error("cannot call non-function expression");
            return nullptr;
        }
        if (is_punct("[")) {
            ++pos_;
            Expr* idx = parse_ternary();
            if (!idx) return nullptr;
            if (!expect_punct("]")) return nullptr;
            Expr* ix = arena_.alloc();
            ix->kind = Expr::Kind::Index;
            ix->operand = e;
            ix->args.push_back(idx);
            e = ix;
            continue;
        }
        break;
    }
    return e;
}

Expr* Parser::parse_primary() {
    // Parenthesized expression or cast
    if (is_punct("(")) {
        // Cast?  (type) expr
        if (peek().kind == TokKind::Ident) {
            Type t;
            if (parse_type_name(peek().text, t) && peek(2).text == ")") {
                ++pos_; ++pos_; ++pos_; // consume ( type )
                Expr* operand = parse_unary();
                if (!operand) return nullptr;
                Expr* c = arena_.alloc();
                c->kind = Expr::Kind::Cast;
                c->cast_type = t;
                c->operand = operand;
                return c;
            }
        }
        ++pos_;
        Expr* e = parse_ternary();
        if (!e) return nullptr;
        if (!expect_punct(")")) return nullptr;
        return e;
    }

    if (cur().kind == TokKind::Number) {
        Expr* e = arena_.alloc();
        e->kind = Expr::Kind::ConstVec;
        e->elems.push_back(cur().num);
        e->int_literal = cur().is_int;
        ++pos_;
        return e;
    }

    if (cur().kind == TokKind::Ident) {
        std::string ident = cur().text;
        Type t;
        bool is_type = parse_type_name(ident, t);

        // floatN(...) constructor
        if (is_type && peek().kind == TokKind::Punct && peek().text == "(") {
            ++pos_; ++pos_;
            std::vector<Expr*> args;
            if (!is_punct(")")) {
                while (true) {
                    Expr* a = parse_ternary();
                    if (!a) return nullptr;
                    args.push_back(a);
                    if (accept_punct(",")) continue;
                    break;
                }
            }
            if (!expect_punct(")")) return nullptr;
            Expr* e = arena_.alloc();
            if (t.is_vector()) {
                e->kind = Expr::Kind::Construct;
                e->dim = t.cols;
                e->cast_type = t;
                e->args = args;
            } else {
                // scalar constructor: cast semantics
                e->kind = Expr::Kind::Cast;
                e->cast_type = t;
                if (args.size() == 1) {
                    e->operand = args[0];
                } else {
                    e->operand = args.empty() ? nullptr : args[0];
                    e->args = args;
                }
            }
            return e;
        }

        ++pos_;
        Expr* e = arena_.alloc();
        e->kind = Expr::Kind::VarRef;
        e->name = ident;
        return e;
    }

    set_error("unexpected token '" + cur().text + "'");
    return nullptr;
}

bool Parser::looks_like_type_name(const std::string& ident) const {
    Type t;
    return parse_type_name(ident, t);
}

// ---------- Statements ----------

Stmt* Parser::parse_statement_inner() {
    // Block
    if (is_punct("{")) return parse_block();

    // if / while / for / return / break / continue
    if (is_ident("if")) return parse_if();
    if (is_ident("while")) return parse_while();
    if (is_ident("for")) return parse_for();

    if (is_ident("return")) {
        ++pos_;
        Stmt* s = stmt_arena_.alloc();
        s->kind = Stmt::Kind::Return;
        if (!is_punct(";")) {
            s->value = parse_ternary();
            if (!s->value) return nullptr;
        }
        expect_punct(";");
        return s;
    }
    if (is_ident("break")) {
        ++pos_;
        expect_punct(";");
        Stmt* s = stmt_arena_.alloc();
        s->kind = Stmt::Kind::Break;
        return s;
    }
    if (is_ident("continue")) {
        ++pos_;
        expect_punct(";");
        Stmt* s = stmt_arena_.alloc();
        s->kind = Stmt::Kind::Continue;
        return s;
    }
    if (is_ident("discard")) {
        ++pos_;
        expect_punct(";");
        Stmt* s = stmt_arena_.alloc();
        s->kind = Stmt::Kind::Discard;
        return s;
    }

    return parse_decl_or_assign();
}

Stmt* Parser::parse_block() {
    expect_punct("{");
    Stmt* block = stmt_arena_.alloc();
    block->kind = Stmt::Kind::Block;
    while (!is_punct("}")) {
        if (at_end()) {
            set_error("unterminated block");
            return nullptr;
        }
        Stmt* s = parse_statement_inner();
        if (!s) return nullptr;
        block->body.push_back(s);
    }
    ++pos_; // }
    return block;
}

Stmt* Parser::parse_if() {
    ++pos_; // if
    expect_punct("(");
    Expr* cond = parse_ternary();
    if (!cond) return nullptr;
    expect_punct(")");

    Stmt* s = stmt_arena_.alloc();
    s->kind = Stmt::Kind::If;
    s->cond = cond;

    if (is_punct("{")) {
        Stmt* then_b = parse_block();
        if (!then_b) return nullptr;
        s->body = then_b->body;
    } else {
        Stmt* one = parse_statement_inner();
        if (!one) return nullptr;
        s->body.push_back(one);
    }

    if (is_ident("else")) {
        ++pos_;
        if (is_punct("{")) {
            Stmt* else_b = parse_block();
            if (!else_b) return nullptr;
            s->else_body = else_b->body;
        } else if (is_ident("if")) {
            Stmt* eif = parse_if();
            if (!eif) return nullptr;
            s->else_body.push_back(eif);
        } else {
            Stmt* one = parse_statement_inner();
            if (!one) return nullptr;
            s->else_body.push_back(one);
        }
    }
    return s;
}

Stmt* Parser::parse_while() {
    ++pos_;
    expect_punct("(");
    Expr* cond = parse_ternary();
    if (!cond) return nullptr;
    expect_punct(")");

    Stmt* s = stmt_arena_.alloc();
    s->kind = Stmt::Kind::While;
    s->cond = cond;
    if (is_punct("{")) {
        Stmt* b = parse_block();
        if (!b) return nullptr;
        s->body = b->body;
    } else {
        Stmt* one = parse_statement_inner();
        if (!one) return nullptr;
        s->body.push_back(one);
    }
    return s;
}

Stmt* Parser::parse_for() {
    ++pos_;
    expect_punct("(");
    Stmt* s = stmt_arena_.alloc();
    s->kind = Stmt::Kind::For;

    // init
    if (!is_punct(";")) {
        Stmt* init = parse_decl_or_assign();
        if (!init) return nullptr;
        s->for_init.push_back(init);
        if (is_punct(",")) {
            while (accept_punct(",")) {
                Stmt* more = parse_decl_or_assign();
                if (!more) return nullptr;
                s->for_init.push_back(more);
            }
        }
    } else {
        ++pos_;
    }

    // cond
    if (!is_punct(";")) {
        s->cond = parse_ternary();
        if (!s->cond) return nullptr;
    }
    expect_punct(";");

    // step
    if (!is_punct(")")) {
        Stmt* step = parse_statement_inner();
        if (!step) return nullptr;
        s->for_step = step;
    }
    expect_punct(")");

    if (is_punct("{")) {
        Stmt* b = parse_block();
        if (!b) return nullptr;
        s->body = b->body;
    } else {
        Stmt* one = parse_statement_inner();
        if (!one) return nullptr;
        s->body.push_back(one);
    }
    return s;
}

Stmt* Parser::parse_decl_or_assign() {
    // Optional storage modifiers
    while (is_ident("const") || is_ident("static") || is_ident("extern")) {
        ++pos_;
        if (is_ident("const")) {} // ignore
    }

    // Declaration: type name [.mask] [= expr] ;
    if (cur().kind == TokKind::Ident) {
        Type t;
        bool is_type = parse_type_name(cur().text, t);
        if (is_type && peek().kind == TokKind::Ident) {
            ++pos_;
            Stmt* s = stmt_arena_.alloc();
            s->kind = Stmt::Kind::Decl;
            s->type = t;
            s->name = cur().text;
            ++pos_;
            if (is_punct(".") && peek().kind == TokKind::Ident) {
                ++pos_;
                s->mask = cur().text;
                ++pos_;
            }
            // array decl [N] - parse and remember count in dim
            if (is_punct("[") && peek().kind == TokKind::Number) {
                ++pos_;
                s->dim = (int)cur().num;
                ++pos_;
                expect_punct("]");
            }
            if (accept_punct("=")) {
                s->init = parse_ternary();
                if (!s->init) return nullptr;
            }
            expect_punct(";");
            return s;
        }
    }

    // Assignment / compound assignment / expression statement
    Expr* lhs = parse_ternary();
    if (!lhs) return nullptr;

    // Post-increment/decrement: i++ / i--  ->  i += 1 / i -= 1
    if (is_punct("++") || is_punct("--")) {
        std::string inc = cur().text;
        ++pos_;
        expect_punct(";");
        if (lhs->kind != Expr::Kind::VarRef) { set_error("++/-- requires a variable"); return nullptr; }
        Stmt* s = stmt_arena_.alloc();
        s->kind = Stmt::Kind::CompoundAssign;
        s->op = (inc == "++") ? "+=" : "-=";
        s->lhs = lhs;
        Expr* one = arena_.alloc();
        one->kind = Expr::Kind::ConstVec;
        one->elems.push_back(1.0);
        one->int_literal = true;
        s->value = one;
        return s;
    }

    if (is_punct("=") || is_punct("+=") || is_punct("-=") || is_punct("*=") || is_punct("/=")) {
        std::string op = cur().text;
        ++pos_;
        Expr* rhs = parse_ternary();
        if (!rhs) return nullptr;
        expect_punct(";");
        Stmt* s = stmt_arena_.alloc();
        if (op == "=") {
            s->kind = Stmt::Kind::Assign;
        } else {
            s->kind = Stmt::Kind::CompoundAssign;
            s->op = op;
        }
        s->lhs = lhs;
        s->value = rhs;
        return s;
    }

    expect_punct(";");
    Stmt* s = stmt_arena_.alloc();
    s->kind = Stmt::Kind::ExprStmt;
    s->value = lhs;
    return s;
}

Stmt* Parser::parse_statement(const std::string& text) {
    toks_ = tokenize(text);
    pos_ = 0;
    error_.clear();
    Stmt* s = parse_statement_inner();
    if (!at_end() && error_.empty())
        set_error("unexpected token '" + cur().text + "'");
    return error_.empty() ? s : nullptr;
}

bool Parser::parse_statements(const std::string& text, std::vector<Stmt*>& out) {
    toks_ = tokenize(text);
    pos_ = 0;
    error_.clear();
    while (!at_end()) {
        if (is_punct(";")) { ++pos_; continue; }
        Stmt* s = parse_statement_inner();
        if (!s) return false;
        out.push_back(s);
    }
    return true;
}

Expr* parse_expr_text(const std::string& text, Arena& arena, std::string& err) {
    StmtArena stmt_arena;
    Parser p(arena, stmt_arena);
    Expr* e = p.parse_expression(text);
    err = p.error();
    return e;
}

} // namespace hb
