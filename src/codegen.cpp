// codegen.cpp
#include "codegen.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "intrinsics.h"
#include "swizzle.h"

namespace hb {

std::string format_float(double v) {
    // Round to float32 precision (HLSL literals are float32).
    float f = (float)v;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.9g", (double)f);
    std::string s = buf;
    // Normalize: always include a decimal or exponent so the assembler
    // treats it as a float literal.
    if (s.find('.') == std::string::npos &&
        s.find('e') == std::string::npos &&
        s.find("inf") == std::string::npos &&
        s.find("nan") == std::string::npos) {
        s += ".0";
    }
    return s;
}

std::string CodeGen::format_imm_scalar(double v) const {
    return "l(" + format_float(v) + ")";
}

std::string CodeGen::format_imm_int(const std::vector<double>& vals, const std::string& dst_mask) const {
    if (vals.empty())
        return "l(0)";
    if (vals.size() == 1)
        return "l(" + std::to_string((long long)vals[0]) + ")";
    double comp[4] = {0, 0, 0, 0};
    std::string dm = dst_mask.empty() ? "xyzw" : dst_mask;
    int n = (int)vals.size();
    int used = std::min(n, (int)dm.size());
    for (int i = 0; i < used; ++i) {
        int idx = mask_index_of(dm[i]);
        if (idx >= 0) comp[idx] = vals[i];
    }
    double last = vals.back();
    for (int i = 0; i < (int)dm.size(); ++i) {
        int idx = mask_index_of(dm[i]);
        if (idx >= 0) {
            bool occupied = false;
            for (int j = 0; j < used; ++j)
                if (mask_index_of(dm[j]) == idx) { occupied = true; break; }
            if (!occupied) comp[idx] = last;
        }
    }
    std::string s = "l(";
    for (int i = 0; i < 4; ++i) {
        if (i) s += ", ";
        s += std::to_string((long long)comp[i]);
    }
    s += ")";
    return s;
}

std::string CodeGen::format_imm(const std::vector<double>& vals, const std::string& dst_mask) const {
    if (vals.empty())
        return "l(0.0)";
    if (vals.size() == 1)
        return "l(" + format_float(vals[0]) + ")";

    // Build 4 components aligned to dst mask positions, replicating the last
    // value into unoccupied positions (HLSL constructor semantics).
    double comp[4] = {0.0, 0.0, 0.0, 0.0};
    std::string dm = dst_mask.empty() ? "xyzw" : dst_mask;
    int n = (int)vals.size();
    int used = std::min(n, (int)dm.size());
    for (int i = 0; i < used; ++i) {
        int idx = mask_index_of(dm[i]);
        if (idx >= 0) comp[idx] = vals[i];
    }
    // Replicate last provided value into remaining masked positions.
    double last = vals.back();
    for (int i = 0; i < (int)dm.size(); ++i) {
        int idx = mask_index_of(dm[i]);
        if (idx >= 0) {
            bool occupied = false;
            for (int j = 0; j < used; ++j)
                if (mask_index_of(dm[j]) == idx) { occupied = true; break; }
            if (!occupied) comp[idx] = last;
        }
    }
    std::string s = "l(";
    for (int i = 0; i < 4; ++i) {
        if (i) s += ", ";
        s += format_float(comp[i]);
    }
    s += ")";
    return s;
}

std::string CodeGen::format_src(const std::string& reg, const std::string& src_mask,
                                const std::string& dst_mask) const {
    std::string dm = dst_mask.empty() ? "xyzw" : dst_mask;
    std::string sm = src_mask.empty() ? "xyzw" : src_mask;
    Swizzle sw = compute_src_swizzle(sm, dm);
    return reg + "." + swizzle_to_string(sw);
}

std::string CodeGen::target_mask_of(const std::string& target) {
    size_t dot = target.rfind('.');
    if (dot == std::string::npos) return "xyzw";
    return target.substr(dot + 1);
}

CodeGen::CodeGen(SymbolTable& symtab, Arena& arena, std::vector<std::string>& out)
    : sym(symtab), arena_(arena), out_(out) {}

// ---- Type inference --------------------------------------------------------

int infer_expr_dim(Expr* e, SymbolTable& sym) {
    if (!e) return 1;
    switch (e->kind) {
    case Expr::Kind::VarRef: {
        if (!e->mask.empty()) return (int)e->mask.size();
        Symbol* s = sym.lookup(e->name);
        if (s) {
            int w = (int)s->mask.size();
            return w > 0 ? w : 1;
        }
        return 4;
    }
    case Expr::Kind::ConstVec:
        return (int)e->elems.size();
    case Expr::Kind::BinOp:
        return std::max(infer_expr_dim(e->left, sym), infer_expr_dim(e->right, sym));
    case Expr::Kind::UnaryOp:
        return infer_expr_dim(e->operand, sym);
    case Expr::Kind::CmpOp:
        return std::max(infer_expr_dim(e->left, sym), infer_expr_dim(e->right, sym));
    case Expr::Kind::Ternary:
        return std::max({infer_expr_dim(e->cond, sym), infer_expr_dim(e->true_expr, sym),
                         infer_expr_dim(e->false_expr, sym)});
    case Expr::Kind::Call:
        return intrinsic_return_dim(e->name, e->args, sym);
    case Expr::Kind::Construct:
        return e->dim;
    case Expr::Kind::Sample:
        return 4;
    case Expr::Kind::Cast:
        return e->cast_type.component_count();
    case Expr::Kind::Index:
        return 1;
    case Expr::Kind::Member:
        return 4;
    }
    return 4;
}

int CodeGen::infer_dim(Expr* e) {
    return infer_expr_dim(e, sym);
}

Type CodeGen::infer_type(Expr* e) {
    if (!e) return make_scalar(BaseType::Float);
    switch (e->kind) {
    case Expr::Kind::VarRef: {
        Symbol* s = sym.lookup(e->name);
        if (s) {
            Type t = s->type;
            if (!e->mask.empty()) t = make_vector((int)e->mask.size(), t.base);
            return t;
        }
        return make_scalar(BaseType::Float);
    }
    case Expr::Kind::ConstVec: {
        int n = (int)e->elems.size();
        BaseType b = e->int_literal ? BaseType::Int : BaseType::Float;
        return n > 1 ? make_vector(n, b) : make_scalar(b);
    }
    case Expr::Kind::BinOp: {
        Type l = infer_type(e->left), r = infer_type(e->right);
        auto rank = [](BaseType b) {
            switch (b) {
            case BaseType::Bool: return 0;
            case BaseType::Int: return 1;
            case BaseType::Uint: return 2;
            default: return 3; // float/double
            }
        };
        BaseType base = (rank(l.base) >= rank(r.base)) ? l.base : r.base;
        if (e->op == "&&" || e->op == "||") base = BaseType::Bool;
        int dim = std::max(l.component_count(), r.component_count());
        return dim > 1 ? make_vector(dim, base) : make_scalar(base);
    }
    case Expr::Kind::UnaryOp: {
        Type t = infer_type(e->operand);
        if (e->op == "!" || e->op == "~") t.base = BaseType::Bool;
        return t;
    }
    case Expr::Kind::CmpOp: {
        int dim = std::max(infer_dim(e->left), infer_dim(e->right));
        return dim > 1 ? make_vector(dim, BaseType::Bool) : make_scalar(BaseType::Bool);
    }
    case Expr::Kind::Ternary: {
        Type t = infer_type(e->true_expr);
        int dim = std::max({infer_dim(e->true_expr), infer_dim(e->false_expr), 1});
        return dim > 1 ? make_vector(dim, t.base) : make_scalar(t.base);
    }
    case Expr::Kind::Call:
        return intrinsic_return_type(e->name, e->args, sym);
    case Expr::Kind::Construct:
        return e->cast_type;
    case Expr::Kind::Sample:
        return make_vector(4, BaseType::Float);
    case Expr::Kind::Cast:
        return e->cast_type;
    case Expr::Kind::Index:
        return make_scalar(infer_type(e->operand).base);
    case Expr::Kind::Member:
        return make_scalar(BaseType::Float);
    }
    return make_scalar(BaseType::Float);
}

// ---- Operand evaluation -----------------------------------------------------

std::string CodeGen::alloc_temp(const std::string& mask, std::vector<std::string>& temps) {
    std::string s = sym.alloc_temp(mask);
    size_t dot = s.find('.');
    temps.push_back(dot == std::string::npos ? s : s.substr(0, dot));
    return s;
}

bool CodeGen::eval(Expr* e, Operand& op, std::vector<std::string>& temps) {
    if (!e) { error_ = "null expression"; return false; }
    switch (e->kind) {
    case Expr::Kind::VarRef: {
        Symbol* s = sym.lookup(e->name);
        if (!s) { error_ = "variable not defined: " + e->name; return false; }
        op.reg = s->reg;
        op.mask = e->mask.empty() ? s->mask : e->mask;
        if (op.mask.empty()) op.mask = "xyzw";
        op.dim = (int)op.mask.size();
        op.is_immediate = false;
        return true;
    }
    case Expr::Kind::ConstVec: {
        op.is_immediate = true;
        op.vals = e->elems;
        op.dim = (int)e->elems.size();
        return true;
    }
    default: {
        // Complex expression: materialize into a fresh temp register.
        int dim = infer_dim(e);
        if (dim < 1) dim = 1;
        std::string mask = "xyzw";
        if (dim <= 4 && dim >= 1) mask = std::string("xyzw").substr(0, (size_t)dim);
        std::string temp = alloc_temp(mask, temps);
        if (!gen_internal(e, temp)) return false;
        op.reg = temp.substr(0, temp.find('.'));
        op.mask = mask;
        op.dim = dim;
        op.is_immediate = false;
        return true;
    }
    }
}

// ---- Assignment generation ---------------------------------------------------

bool CodeGen::gen_assignment(Expr* e, const std::string& target_str) {
    std::string tmask = target_mask_of(target_str);
    if (tmask.empty() || tmask == "xyzw") tmask = "xyzw";

    // Non-ascending LHS swizzle (e.g. ".wx", ".zy"): needs invert.
    if (!is_legal_mask(tmask) && (int)tmask.size() > 1) {
        // legal mask = unique components of tmask, ascending.
        std::string legal;
        for (int i = 0; i < 4; ++i)
            if (tmask.find("xyzw"[i]) != std::string::npos)
                legal.push_back("xyzw"[i]);
        // inverse swizzle: for each legal component, its position in tmask.
        std::string inv;
        for (char c : legal) {
            size_t k = tmask.find(c);
            inv.push_back("xyzw"[k]);
        }
        size_t dot = target_str.find('.');
        std::string tbase = dot == std::string::npos ? target_str : target_str.substr(0, dot);

        // Simple source (variable/constant): emit the inverted mov directly,
        // matching how fxc lowers "r.zyx = a.xyz" -> "mov r.xyz, a.zyxz".
        if (e->kind == Expr::Kind::VarRef || e->kind == Expr::Kind::ConstVec) {
            Swizzle inv_sw = compute_src_swizzle(inv, legal);
            std::string src;
            if (e->kind == Expr::Kind::VarRef) {
                Symbol* s = sym.lookup(e->name);
                if (!s) { error_ = "variable not defined: " + e->name; return false; }
                src = s->reg + "." + swizzle_to_string(inv_sw);
            } else {
                src = format_imm(e->elems, legal);
            }
            emit("mov " + tbase + "." + legal + ", " + src);
            return true;
        }

        // Complex expression: materialize into a temp, then invert.
        std::vector<std::string> temps;
        std::string tmp_mask = std::string("xyzw").substr(0, legal.size());
        std::string temp = alloc_temp(tmp_mask, temps);
        if (!gen_internal(e, temp)) return false;
        std::string tbase_temp = temp.substr(0, temp.find('.'));
        Swizzle inv_sw = compute_src_swizzle(inv, legal);
        emit("mov " + tbase + "." + legal + ", " + tbase_temp + "." + swizzle_to_string(inv_sw));
        for (auto& tb : temps) sym.free_temp(tb);
        return true;
    }

    return gen_internal(e, target_str);
}

bool CodeGen::gen_fold_saturate(Expr* arg, const std::string& target) {
    // Comparison: already returns 1.0/0.0, saturate is a no-op.
    if (arg->kind == Expr::Kind::CmpOp)
        return gen_internal(arg, target);
    bool foldable = false;
    if (arg->kind == Expr::Kind::BinOp) {
        foldable = (arg->op == "+" || arg->op == "-" || arg->op == "*" || arg->op == "/");
    } else if (arg->kind == Expr::Kind::VarRef || arg->kind == Expr::Kind::ConstVec ||
               arg->kind == Expr::Kind::UnaryOp) {
        foldable = true;
    } else if (arg->kind == Expr::Kind::Call && arg->name == "dot") {
        foldable = true;
    }
    if (!foldable)
        return false;
    fold_sat_ = true;
    bool ok = gen_internal(arg, target);
    fold_sat_ = false;
    return ok;
}

bool CodeGen::gen_internal(Expr* e, const std::string& target_str) {
    if (!e) { error_ = "null expression"; return false; }
    switch (e->kind) {
    case Expr::Kind::VarRef: {
        Symbol* s = sym.lookup(e->name);
        if (!s) { error_ = "variable not defined: " + e->name; return false; }
        std::string dm = target_mask_of(target_str);
        std::string sm = e->mask.empty() ? s->mask : e->mask;
        std::string src = format_src(s->reg, sm, dm);
        emit(std::string("mov") + sat_suffix() + " " + target_str + ", " + src);
        return true;
    }
    case Expr::Kind::ConstVec: {
        std::string dm = target_mask_of(target_str);
        // If the destination register holds an int/uint/bool variable, store
        // the literal with integer bits (l(42)) rather than float bits.
        std::string tbase = target_str.substr(0, target_str.find('.'));
        Symbol* t = sym.find_by_reg(tbase);
        bool int_target = t && (t->type.is_int() || t->type.is_uint() || t->type.is_bool());
        emit(std::string("mov") + sat_suffix() + " " + target_str + ", " +
             (int_target ? format_imm_int(e->elems, dm) : format_imm(e->elems, dm)));
        return true;
    }
    case Expr::Kind::BinOp: return gen_binop(e, target_str);
    case Expr::Kind::UnaryOp: return gen_unary(e, target_str);
    case Expr::Kind::CmpOp: return gen_cmp(e, target_str);
    case Expr::Kind::Ternary: return gen_ternary(e, target_str);
    case Expr::Kind::Call: return gen_call(e, target_str);
    case Expr::Kind::Construct: return gen_construct(e, target_str);
    case Expr::Kind::Sample: return gen_sample(e, target_str);
    case Expr::Kind::Cast: return gen_cast(e, target_str);
    case Expr::Kind::Index: return gen_index(e, target_str);
    case Expr::Kind::Member:
        error_ = "member access not supported: " + e->name;
        return false;
    }
    error_ = "unsupported expression kind";
    return false;
}

// Format an evaluated operand (register or immediate) against a dest mask.
std::string CodeGen::fmt_operand(const Operand& op, const std::string& dst_mask) const {
    if (op.is_immediate)
        return format_imm(op.vals, dst_mask);
    return format_src(op.reg, op.mask, dst_mask);
}

bool CodeGen::gen_binop(Expr* e, const std::string& target_str) {
    std::string dm = target_mask_of(target_str);
    const std::string& op = e->op;

    bool bitwise = (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>" ||
                    op == "&&" || op == "||");

    // Scalar immediate on the right: emit compact immediate form.
    if (e->right->kind == Expr::Kind::ConstVec && e->right->elems.size() == 1 &&
        op != "/" && op != "%") {
        std::vector<std::string> temps;
        Operand l;
        if (!eval(e->left, l, temps)) return false;
        double c = e->right->elems[0];
        Type lt = infer_type(e->left);
        bool int_ctx = bitwise || lt.is_int() || lt.is_uint() || lt.is_bool();
        std::string ls = fmt_operand(l, dm);
        std::string imm = int_ctx ? ("l(" + std::to_string((long long)c) + ")")
                                  : ("l(" + format_float(c) + ")");
        std::string a = int_ctx ? "iadd" : ("add" + sat_suffix());
        std::string m = int_ctx ? "imul" : ("mul" + sat_suffix());
        if (op == "+") emit(a + " " + target_str + ", " + ls + ", " + imm);
        else if (op == "-") emit(a + " " + target_str + ", " + ls + ", -" + imm);
        else if (op == "*") emit(m + " " + target_str + ", " + ls + ", " + imm);
        else if (op == "&" || op == "&&") emit("and " + target_str + ", " + ls + ", " + imm);
        else if (op == "|" || op == "||") emit("or " + target_str + ", " + ls + ", " + imm);
        else if (op == "^") emit("xor " + target_str + ", " + ls + ", " + imm);
        else if (op == "<<") emit("ishl " + target_str + ", " + ls + ", " + imm);
        else if (op == ">>") emit("ishr " + target_str + ", " + ls + ", " + imm);
        else { error_ = "unsupported binary op with scalar constant: " + op; return false; }
        for (auto& t : temps) sym.free_temp(t);
        return true;
    }

    std::vector<std::string> temps;
    Operand l, r;
    if (!eval(e->left, l, temps)) return false;
    if (!eval(e->right, r, temps)) return false;

    Type lt = infer_type(e->left), rt = infer_type(e->right);
    bool both_int = bitwise || ((lt.is_int() || lt.is_uint() || lt.is_bool()) &&
                                (rt.is_int() || rt.is_uint() || rt.is_bool()));
    bool is_uint = lt.is_uint() || rt.is_uint();

    // Format operands; use integer immediates for int/bitwise ops.
    auto fmt = [&](const Operand& o) -> std::string {
        if (o.is_immediate)
            return both_int ? format_imm_int(o.vals, dm) : format_imm(o.vals, dm);
        return format_src(o.reg, o.mask, dm);
    };
    std::string ls = fmt(l);
    std::string rs = fmt(r);

    std::string mnem;
    if (op == "+") mnem = (both_int ? "iadd" : ("add" + sat_suffix()));
    else if (op == "-") mnem = (both_int ? "iadd" : ("add" + sat_suffix()));
    else if (op == "*") mnem = (both_int ? "imul" : ("mul" + sat_suffix()));
    else if (op == "/") mnem = (both_int ? (is_uint ? "udiv" : "idiv") : ("div" + sat_suffix()));
    else if (op == "%") mnem = both_int ? (is_uint ? "umod" : "imod") : "?";
    else if (op == "||" || op == "|") mnem = "or";
    else if (op == "&&" || op == "&") mnem = "and";
    else if (op == "^") mnem = "xor";
    else if (op == "<<") mnem = "ishl";
    else if (op == ">>") mnem = (is_uint ? "ushr" : "ishr");
    else { error_ = "unsupported binary op: " + op; return false; }

    if (op == "-") {
        // sub with immediate source is restricted; use add with negated.
        emit("add" + sat_suffix() + " " + target_str + ", " + ls + ", -" + rs);
    } else {
        emit(mnem + " " + target_str + ", " + ls + ", " + rs);
    }
    for (auto& t : temps) sym.free_temp(t);
    return true;
}

bool CodeGen::gen_unary(Expr* e, const std::string& target_str) {
    std::string dm = target_mask_of(target_str);
    const std::string& op = e->op;

    if (op == "!") {
        // logical not: eq dst, src, l(0)
        std::vector<std::string> temps;
        Operand src;
        if (!eval(e->operand, src, temps)) return false;
        emit("eq " + target_str + ", " + fmt_operand(src, dm) + ", l(0.0)");
        for (auto& t : temps) sym.free_temp(t);
        return true;
    }
    if (op == "~") {
        std::vector<std::string> temps;
        Operand src;
        if (!eval(e->operand, src, temps)) return false;
        emit("not " + target_str + ", " + fmt_operand(src, dm));
        for (auto& t : temps) sym.free_temp(t);
        return true;
    }
    if (op == "+") {
        return gen_internal(e->operand, target_str);
    }
    // op == "-"
    if (e->operand->kind == Expr::Kind::ConstVec) {
        std::vector<double> neg;
        for (double v : e->operand->elems) neg.push_back(-v);
        emit("mov " + target_str + ", " + format_imm(neg, dm));
        return true;
    }
    std::vector<std::string> temps;
    Operand src;
    if (!eval(e->operand, src, temps)) return false;
    emit(std::string("mov") + sat_suffix() + " " + target_str + ", -" + fmt_operand(src, dm));
    for (auto& t : temps) sym.free_temp(t);
    return true;
}

bool CodeGen::gen_cmp(Expr* e, const std::string& target_str) {
    std::string dm = target_mask_of(target_str);
    Type lt = infer_type(e->left), rt = infer_type(e->right);
    bool is_int = (lt.is_int() || lt.is_uint() || lt.is_bool()) &&
                  (rt.is_int() || rt.is_uint() || rt.is_bool());
    bool is_uint = lt.is_uint() || rt.is_uint();
    std::vector<std::string> temps;
    Operand l, r;
    if (!eval(e->left, l, temps)) return false;
    if (!eval(e->right, r, temps)) return false;
    auto fmt = [&](const Operand& o) -> std::string {
        if (o.is_immediate)
            return is_int ? format_imm_int(o.vals, dm) : format_imm(o.vals, dm);
        return format_src(o.reg, o.mask, dm);
    };
    std::string ls = fmt(l);
    std::string rs = fmt(r);
    const std::string& op = e->op;
    std::string mn = is_int ? "i" : "";
    if (is_uint && (op == "<" || op == "<=" || op == ">" || op == ">="))
        mn = "u";
    if (op == ">") emit(mn + "lt " + target_str + ", " + rs + ", " + ls);
    else if (op == "<") emit(mn + "lt " + target_str + ", " + ls + ", " + rs);
    else if (op == ">=") emit(mn + "ge " + target_str + ", " + ls + ", " + rs);
    else if (op == "<=") emit(mn + "ge " + target_str + ", " + rs + ", " + ls);
    else if (op == "==") emit(mn + "eq " + target_str + ", " + ls + ", " + rs);
    else if (op == "!=") emit(mn + "ne " + target_str + ", " + ls + ", " + rs);
    else { error_ = "unsupported comparison: " + op; return false; }
    for (auto& t : temps) sym.free_temp(t);
    return true;
}

bool CodeGen::gen_ternary(Expr* e, const std::string& target_str) {
    std::string dm = target_mask_of(target_str);
    std::vector<std::string> temps;
    Operand cond, t, f;
    if (!eval(e->cond, cond, temps)) return false;
    if (!eval(e->true_expr, t, temps)) return false;
    if (!eval(e->false_expr, f, temps)) return false;
    emit("movc " + target_str + ", " + fmt_operand(cond, dm) + ", " +
         fmt_operand(t, dm) + ", " + fmt_operand(f, dm));
    for (auto& tb : temps) sym.free_temp(tb);
    return true;
}

bool CodeGen::gen_construct(Expr* e, const std::string& target_str) {
    std::string dm = target_mask_of(target_str);
    size_t dot = target_str.find('.');
    std::string tbase = dot == std::string::npos ? target_str : target_str.substr(0, dot);

    // All-constant args: fold into a single immediate.
    bool all_const = !e->args.empty();
    for (Expr* a : e->args)
        if (a->kind != Expr::Kind::ConstVec) { all_const = false; break; }
    if (all_const) {
        std::vector<double> vals;
        for (Expr* a : e->args) {
            for (double v : a->elems) vals.push_back(v);
        }
        emit("mov " + target_str + ", " + format_imm(vals, dm));
        return true;
    }

    std::vector<std::string> temps;
    int idx = 0;              // next component index within target mask
    int total = (int)dm.size();
    for (size_t i = 0; i < e->args.size() && idx < total; ++i) {
        Expr* a = e->args[i];
        int width = infer_dim(a);
        bool is_last = (i + 1 == e->args.size());
        int take;
        if (is_last) {
            take = total - idx;  // last arg consumes the rest
        } else {
            take = std::min(width, total - idx);
        }
        std::string pos_mask = dm.substr(idx, (size_t)take);
        Operand op;
        if (!eval(a, op, temps)) return false;
        emit("mov " + tbase + "." + pos_mask + ", " + fmt_operand(op, pos_mask));
        idx += take;
    }
    for (auto& t : temps) sym.free_temp(t);
    return true;
}

bool CodeGen::gen_sample(Expr* e, const std::string& target_str) {
    std::string dm = target_mask_of(target_str);
    Symbol* tex = sym.lookup(e->name);
    if (!tex || !tex->is_texture) { error_ = "texture not bound: " + e->name; return false; }
    if (e->sampler_expr->kind != Expr::Kind::VarRef) { error_ = "sampler must be a variable"; return false; }
    Symbol* samp = sym.lookup(e->sampler_expr->name);
    if (!samp || !samp->is_sampler) { error_ = "sampler not bound: " + e->sampler_expr->name; return false; }

    std::vector<std::string> temps;
    Operand uv;
    if (!eval(e->uv_expr, uv, temps)) return false;
    std::string uv_s = fmt_operand(uv, dm);
    if (e->sample_kind == 1) {
        // SampleLevel: sample_l dst, uv, tex, sampler, lod
        Operand lod;
        if (!eval(e->false_expr, lod, temps)) return false;
        std::string lod_s = fmt_operand(lod, "x");
        emit("sample_l " + target_str + ", " + uv_s + ", " + tex->reg + ".xyzw, " + samp->reg + ", " + lod_s);
    } else if (e->sample_kind == 2) {
        // SampleCmp: sample_c dst, uv, tex, sampler, ref
        Operand ref;
        if (!eval(e->false_expr, ref, temps)) return false;
        std::string ref_s = fmt_operand(ref, "x");
        emit("sample_c " + target_str + ", " + uv_s + ", " + tex->reg + ".xyzw, " + samp->reg + ", " + ref_s);
    } else if (e->sample_kind == 3) {
        // SampleBias: sample_b dst, uv, tex, sampler, bias
        Operand bias;
        if (!eval(e->false_expr, bias, temps)) return false;
        std::string bias_s = fmt_operand(bias, "x");
        emit("sample_b " + target_str + ", " + uv_s + ", " + tex->reg + ".xyzw, " + samp->reg + ", " + bias_s);
    } else if (e->sample_kind == 4) {
        // SampleGrad: sample_d dst, uv, tex, sampler, gradx, grady
        Operand gx, gy;
        if (!eval(e->false_expr, gx, temps)) return false;
        if (!eval(e->left, gy, temps)) return false;
        std::string gx_s = fmt_operand(gx, "x");
        std::string gy_s = fmt_operand(gy, "x");
        emit("sample_d " + target_str + ", " + uv_s + ", " + tex->reg + ".xyzw, " + samp->reg + ", " + gx_s + ", " + gy_s);
    } else {
        emit("sample " + target_str + ", " + uv_s + ", " + tex->reg + ".xyzw, " + samp->reg);
    }
    for (auto& t : temps) sym.free_temp(t);
    return true;
}

bool CodeGen::gen_cast(Expr* e, const std::string& target_str) {
    std::string dm = target_mask_of(target_str);
    std::vector<std::string> temps;
    Operand src;
    if (!eval(e->operand, src, temps)) return false;
    std::string ss = fmt_operand(src, dm);
    BaseType from = infer_type(e->operand).base;
    BaseType to = e->cast_type.base;
    std::string mnem;
    if (from == to) mnem = "mov";
    else if (from == BaseType::Float && to == BaseType::Int) mnem = "ftoi";
    else if (from == BaseType::Float && to == BaseType::Uint) mnem = "ftou";
    else if (from == BaseType::Float && to == BaseType::Bool) mnem = "mov";
    else if (from == BaseType::Int && to == BaseType::Float) mnem = "itof";
    else if (from == BaseType::Uint && to == BaseType::Float) mnem = "utof";
    else if (from == BaseType::Int && to == BaseType::Uint) mnem = "mov";
    else if (from == BaseType::Uint && to == BaseType::Int) mnem = "mov";
    else if (to == BaseType::Bool) mnem = "mov";
    else mnem = "mov";
    if (src.is_immediate && (mnem == "itof" || mnem == "ftoi" || mnem == "ftou")) {
        // constant fold conversions
        std::vector<double> nv;
        for (double v : src.vals) {
            if (mnem == "ftoi") nv.push_back((double)(int)v);
            else if (mnem == "ftou") nv.push_back((double)(unsigned)(int)v);
            else if (mnem == "itof") nv.push_back(v);
        }
        emit("mov " + target_str + ", " + format_imm(nv, dm));
    } else {
        emit(mnem + " " + target_str + ", " + ss);
    }
    for (auto& t : temps) sym.free_temp(t);
    return true;
}

bool CodeGen::gen_index(Expr* e, const std::string& target_str) {
    // Only constant integer indices are supported (dynamic vector indexing has
    // no direct DXBC lowering).
    Expr* idx = e->args.empty() ? nullptr : e->args[0];
    if (!idx || idx->kind != Expr::Kind::ConstVec) {
        error_ = "dynamic indexing not supported";
        return false;
    }
    int i = (int)idx->elems[0];
    if (i < 0 || i > 3) { error_ = "index out of range"; return false; }
    std::string dm = target_mask_of(target_str);
    if (e->operand->kind == Expr::Kind::VarRef) {
        Symbol* s = sym.lookup(e->operand->name);
        if (!s) { error_ = "variable not defined: " + e->operand->name; return false; }
        emit("mov " + target_str + ", " + format_src(s->reg, std::string(1, "xyzw"[i]), dm));
        return true;
    }
    std::vector<std::string> temps;
    Operand op;
    if (!eval(e->operand, op, temps)) return false;
    std::string sm = op.mask;
    if (i >= (int)sm.size()) { error_ = "index out of range"; return false; }
    emit("mov " + target_str + ", " + format_src(op.reg, std::string(1, sm[i]), dm));
    for (auto& t : temps) sym.free_temp(t);
    return true;
}

bool CodeGen::gen_call(Expr* e, const std::string& target_str) {
    const auto& table = intrinsic_table();
    for (const auto& kv : table) {
        if (kv.first == e->name) {
            return kv.second.fn(*this, e, target_str);
        }
    }
    if (custom_expander_) {
        if (custom_expander_(*this, e, target_str))
            return true;
        if (!error_.empty())
            return false;  // custom expander reported a failure
    }
    error_ = "unknown function: " + e->name;
    return false;
}

} // namespace hb
