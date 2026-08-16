// ast.h - expression/statement AST nodes (arena-allocated)
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "types.h"

namespace hb {

// ---- Expression nodes -----------------------------------------------------

struct Expr;
using ExprPtr = Expr*;

struct Expr {
    enum class Kind {
        VarRef,        // name[.mask]
        ConstVec,      // numeric literal (elems)
        Call,          // function call name(args)
        BinOp,         // op(left, right): + - * /
        UnaryOp,       // op(operand): -  !
        CmpOp,         // op(left, right): > < >= <= == !=
        Ternary,       // cond ? true : false
        Construct,     // floatN(a, b, ...) -> dim + args
        Sample,        // texture.Sample(sampler, uv)
        Cast,          // (type)expr
        Index,         // expr[index]  (vector/matrix indexing, array indexing)
        Member,        // expr.member (struct, matrix row access)
    };

    Kind kind;
    Type type;              // inferred type (set by parser/codegen)

    std::string name;       // VarRef / Call / Member
    std::string mask;       // VarRef swizzle ("xyz"), may be empty

    std::vector<double> elems;  // ConstVec
    bool int_literal = false;   // ConstVec: came from an integer literal (for int immediates)

    std::string op;         // BinOp / UnaryOp / CmpOp operator string
    std::vector<Expr*> args; // Call / Construct / Index

    Expr* left = nullptr;
    Expr* right = nullptr;
    Expr* operand = nullptr;
    Expr* cond = nullptr;
    Expr* true_expr = nullptr;
    Expr* false_expr = nullptr;
    Expr* sampler_expr = nullptr;  // Sample
    Expr* uv_expr = nullptr;       // Sample
    int sample_kind = 0;           // Sample: 0=Sample, 1=SampleLevel, 2=SampleCmp, 3=SampleBias, 4=SampleGrad

    // Cast target / Construct dimension
    Type cast_type;
    int dim = 0;             // Construct dim (2/3/4)

    const char* kind_name() const {
        switch (kind) {
        case Kind::VarRef: return "VarRef";
        case Kind::ConstVec: return "ConstVec";
        case Kind::Call: return "Call";
        case Kind::BinOp: return "BinOp";
        case Kind::UnaryOp: return "UnaryOp";
        case Kind::CmpOp: return "CmpOp";
        case Kind::Ternary: return "Ternary";
        case Kind::Construct: return "Construct";
        case Kind::Sample: return "Sample";
        case Kind::Cast: return "Cast";
        case Kind::Index: return "Index";
        case Kind::Member: return "Member";
        }
        return "?";
    }
};

// Arena owns all Expr allocations; cleared between top-level statement
// translations so pointers stay valid within one statement's lifetime.
class Arena {
public:
    Expr* alloc() {
        Expr* e = new Expr();
        nodes_.push_back(std::unique_ptr<Expr>(e));
        return e;
    }
    void clear() { nodes_.clear(); }
private:
    std::vector<std::unique_ptr<Expr>> nodes_;
};

// ---- Statements -----------------------------------------------------------

struct Stmt;
using StmtPtr = Stmt*;

struct Stmt {
    enum class Kind {
        Decl,          // type name[.mask] [= expr];
        Assign,        // lvalue[.mask] = expr;
        CompoundAssign,// lvalue[.mask] op= expr;
        ExprStmt,      // expression statement (e.g. pure function call)
        If,            // if (cond) { then } [else { else }]
        While,         // while (cond) { body }
        For,           // for (init; cond; step) { body }
        Return,        // return [expr];
        Block,         // { stmts }
        Break,
        Continue,
        Discard,       // discard;
        Switch,        // switch (cond) { case v: ... default: ... }
        Nop,
    };

    Kind kind;
    Type type;           // Decl: declared type
    std::string name;    // Decl / compound: var name
    std::string mask;    // Decl / compound: var write mask
    int dim = 0;         // Decl: array element count (0 = scalar)
    Expr* init = nullptr;      // Decl
    Expr* value = nullptr;     // Assign / Return
    std::string op;            // CompoundAssign op ("+=" ...)
    Expr* lhs = nullptr;       // Assign lvalue expr

    // Control flow
    Expr* cond = nullptr;
    std::vector<Stmt*> body;       // If then / While / For body
    std::vector<Stmt*> else_body;  // If else
    std::vector<Stmt*> for_init;   // For
    Stmt* for_step = nullptr;      // For

    // Switch
    struct CaseEntry {
        Expr* value = nullptr;      // case constant (nullptr = default)
        std::vector<Stmt*> body;
    };
    std::vector<CaseEntry> cases;

    const char* kind_name() const {
        switch (kind) {
        case Kind::Decl: return "Decl";
        case Kind::Assign: return "Assign";
        case Kind::CompoundAssign: return "CompoundAssign";
        case Kind::ExprStmt: return "ExprStmt";
        case Kind::If: return "If";
        case Kind::While: return "While";
        case Kind::For: return "For";
        case Kind::Return: return "Return";
        case Kind::Block: return "Block";
        case Kind::Break: return "Break";
        case Kind::Continue: return "Continue";
        case Kind::Discard: return "Discard";
        case Kind::Switch: return "Switch";
        case Kind::Nop: return "Nop";
        }
        return "?";
    }
};

class StmtArena {
public:
    Stmt* alloc() {
        Stmt* s = new Stmt();
        nodes_.push_back(std::unique_ptr<Stmt>(s));
        return s;
    }
    void clear() { nodes_.clear(); }
private:
    std::vector<std::unique_ptr<Stmt>> nodes_;
};

// ---- Function library (HLSLFunctionImport / lib.txt) -----------------------

struct FuncParam {
    std::string type_name;   // "float3", "Texture2D", ...
    std::string name;
    std::string modifier;    // "in", "out", "inout"
    Type type;
};

struct FunctionDef {
    std::string return_type_name;
    Type return_type;
    std::vector<FuncParam> params;
    std::vector<std::string> body_lines;  // raw HLSL source lines (strip comments later)
};

} // namespace hb
