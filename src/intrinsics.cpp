// intrinsics.cpp - centralized intrinsic table + lowering (DXC-style)
#include "intrinsics.h"

#include <algorithm>

#include "codegen.h"

namespace hb {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool check_args(Expr* call, int n) {
    return call->args.size() == (size_t)n;
}

// Evaluate all args into operands, collecting temp bases.
static bool eval_args(CodeGen& cg, Expr* call, std::vector<Operand>& ops,
                      std::vector<std::string>& temps) {
    for (Expr* a : call->args) {
        Operand op;
        if (!cg.eval(a, op, temps)) return false;
        ops.push_back(op);
    }
    return true;
}

static std::string dst_mask(const std::string& target) {
    return CodeGen::target_mask_of(target);
}

// ---------------------------------------------------------------------------
// Simple binary/unary intrinsics
// ---------------------------------------------------------------------------

static bool bin_intrinsic(CodeGen& cg, Expr* call, const std::string& target, const char* mnem) {
    if (!check_args(call, 2)) return false;
    std::vector<std::string> temps;
    Operand a, b;
    if (!cg.eval(call->args[0], a, temps)) return false;
    if (!cg.eval(call->args[1], b, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit(std::string(mnem) + " " + target + ", " + cg.fmt_operand(a, dm) + ", " +
            cg.fmt_operand(b, dm));
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool unary_intrinsic(CodeGen& cg, Expr* call, const std::string& target, const char* mnem) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit(std::string(mnem) + " " + target + ", " + cg.fmt_operand(a, dm));
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool iadd(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "add"); }
static bool isub(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "add"); }
static bool imul(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "mul"); }
static bool idiv(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "div"); }
static bool imax(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "max"); }
static bool imin(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "min"); }
static bool iand(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "and"); }
static bool ior(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "or"); }
static bool ixor(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "xor"); }
static bool ishl(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "ishl"); }
static bool ishr(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "ishr"); }
static bool iushr(CodeGen& cg, Expr* c, const std::string& t) { return bin_intrinsic(cg, c, t, "ushr"); }

static bool inot(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("not " + target + ", " + cg.fmt_operand(a, dm));
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool isqrt(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "sqrt"); }
static bool irsqrt(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "rsq"); }
static bool iddx(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "ddx"); }
static bool iddy(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "ddy"); }
static bool ircp(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "rcp"); }
static bool ifloor(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "round_ni"); }
static bool iceil(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "round_pi"); }
static bool itrunc(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "round_z"); }
static bool iround(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "round_ne"); }
static bool ifrc(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "frc"); }
static bool iexp2(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "exp"); }
static bool ilog2(CodeGen& cg, Expr* c, const std::string& t) { return unary_intrinsic(cg, c, t, "log"); }

static bool isaturate(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("mov_sat " + target + ", " + cg.fmt_operand(a, dm));
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool iabs(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    if (call->args[0]->kind == Expr::Kind::ConstVec) {
        std::vector<double> vals;
        for (double v : call->args[0]->elems) vals.push_back(v < 0 ? -v : v);
        std::string dm = dst_mask(target);
        cg.emit("mov " + target + ", " + cg.format_imm(vals, dm));
        return true;
    }
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("mov " + target + ", |" + cg.fmt_operand(a, dm) + "|");
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool isin(CodeGen& cg, Expr* call, const std::string& target);
static bool icos(CodeGen& cg, Expr* call, const std::string& target);

static bool isincos(CodeGen& cg, Expr* call, const std::string& target) {
    // sincos(sinDest, src, cosDest) - void statement form
    if (!check_args(call, 3)) return false;
    std::vector<std::string> temps;
    Operand sin_arg, src_arg, cos_arg;
    if (!cg.eval(call->args[0], sin_arg, temps)) return false;
    if (!cg.eval(call->args[1], src_arg, temps)) return false;
    if (!cg.eval(call->args[2], cos_arg, temps)) return false;
    std::string sm = dst_mask(target);
    // dest operands use writemask form
    auto dest_form = [](const Operand& o) -> std::string {
        return o.is_immediate ? std::string("r?") : o.reg + "." + (o.mask.empty() ? "x" : o.mask);
    };
    cg.emit("sincos " + dest_form(sin_arg) + ", " + cg.fmt_operand(src_arg, "x") + ", " +
            dest_form(cos_arg));
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool isin(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    std::string cos_t = cg.alloc_temp(dm, temps);
    cg.emit("sincos " + target + ", " + cg.fmt_operand(a, "x") + ", " + cos_t);
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool icos(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    std::string sin_t = cg.alloc_temp(dm, temps);
    cg.emit("sincos " + sin_t + ", " + cg.fmt_operand(a, "x") + ", " + target);
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

// ---------------------------------------------------------------------------
// Vector math
// ---------------------------------------------------------------------------

static bool idot(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 2)) return false;
    std::vector<std::string> temps;
    Operand a, b;
    if (!cg.eval(call->args[0], a, temps)) return false;
    if (!cg.eval(call->args[1], b, temps)) return false;
    int dim = std::max(a.dim, b.dim);
    if (dim < 1) dim = 1;
    if (dim > 4) dim = 4;
    // dp2/dp3/dp4 read components in natural order; format with "xyzw" dst.
    cg.emit(std::string("dp") + std::to_string(dim) + " " + target + ", " +
            cg.fmt_operand(a, "xyzw") + ", " + cg.fmt_operand(b, "xyzw"));
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool ilength(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    int dim = a.dim;
    std::string t = cg.alloc_temp("x", temps);
    cg.emit(std::string("dp") + std::to_string(dim) + " " + t + ", " +
            cg.fmt_operand(a, "xyzw") + ", " + cg.fmt_operand(a, "xyzw"));
    std::string tb = t.substr(0, t.find('.'));
    cg.emit("sqrt " + target + ", " + cg.fmt_operand(make_reg_operand(tb, "x"), dst_mask(target)));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool idistance(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 2)) return false;
    std::vector<std::string> temps;
    Operand a, b;
    if (!cg.eval(call->args[0], a, temps)) return false;
    if (!cg.eval(call->args[1], b, temps)) return false;
    int dim = std::max(a.dim, b.dim);
    std::string dm = std::string("xyzw").substr(0, (size_t)dim);
    std::string diff = cg.alloc_temp(dm, temps);
    cg.emit("add " + diff + ", " + cg.fmt_operand(a, dm) + ", -" + cg.fmt_operand(b, dm));
    std::string t = cg.alloc_temp("x", temps);
    cg.emit(std::string("dp") + std::to_string(dim) + " " + t + ", " +
            cg.format_src(diff.substr(0, diff.find('.')), dm, "xyzw") + ", " +
            cg.format_src(diff.substr(0, diff.find('.')), dm, "xyzw"));
    cg.emit("sqrt " + target + ", " + cg.format_src(t.substr(0, t.find('.')), "x", dst_mask(target)));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool inormalize(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    int dim = a.dim;
    std::string dm = std::string("xyzw").substr(0, (size_t)dim);
    std::string t = cg.alloc_temp("x", temps);
    cg.emit(std::string("dp") + std::to_string(dim) + " " + t + ", " +
            cg.fmt_operand(a, "xyzw") + ", " + cg.fmt_operand(a, "xyzw"));
    cg.emit("rsq " + t + ", " + cg.format_src(t.substr(0, t.find('.')), "x", "x"));
    std::string tbase = t.substr(0, t.find('.'));
    cg.emit("mul " + target + ", " + cg.fmt_operand(a, dm) + ", " + tbase + ".xxxx");
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool icross(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 2)) return false;
    std::vector<std::string> temps;
    Operand a, b;
    if (!cg.eval(call->args[0], a, temps)) return false;
    if (!cg.eval(call->args[1], b, temps)) return false;
    std::string dm = dst_mask(target);
    if (dm.empty()) dm = "xyz";
    std::string m = dm.substr(0, 3);
    std::string t1 = cg.alloc_temp(m, temps);
    std::string t2 = cg.alloc_temp(m, temps);
    std::string t1b = t1.substr(0, t1.find('.'));
    std::string t2b = t2.substr(0, t2.find('.'));
    cg.emit("mov " + t1 + ", " + cg.fmt_operand(a, m));
    cg.emit("mov " + t2 + ", " + cg.fmt_operand(b, m));
    cg.emit("mul " + target + ", " + cg.format_src(t1b, "yzx", m) + ", " + cg.format_src(t2b, "zxy", m));
    cg.emit("mad " + target + ", -" + cg.format_src(t1b, "zxy", m) + ", " + cg.format_src(t2b, "yzx", m) + ", " + target);
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

// ---------------------------------------------------------------------------
// Scalar helpers
// ---------------------------------------------------------------------------

static bool iclamp(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 3)) return false;
    // Optimize clamp(x, 0, 1) -> mov_sat
    bool lo0 = call->args[1]->kind == Expr::Kind::ConstVec && call->args[1]->elems.size() == 1 &&
               call->args[1]->elems[0] == 0.0;
    bool hi1 = call->args[2]->kind == Expr::Kind::ConstVec && call->args[2]->elems.size() == 1 &&
               call->args[2]->elems[0] == 1.0;
    if (lo0 && hi1) return isaturate(cg, call, target);

    std::vector<std::string> temps;
    Operand x, lo, hi;
    if (!cg.eval(call->args[0], x, temps)) return false;
    if (!cg.eval(call->args[1], lo, temps)) return false;
    if (!cg.eval(call->args[2], hi, temps)) return false;
    std::string dm = dst_mask(target);
    std::string t = cg.alloc_temp(dm, temps);
    std::string tb = t.substr(0, t.find('.'));
    cg.emit("min " + t + ", " + cg.fmt_operand(x, dm) + ", " + cg.fmt_operand(hi, dm));
    cg.emit("max " + target + ", " + cg.fmt_operand(make_reg_operand(tb, dm), dm) + ", " + cg.fmt_operand(lo, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool istep(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 2)) return false;
    std::vector<std::string> temps;
    Operand edge, x;
    if (!cg.eval(call->args[0], edge, temps)) return false;
    if (!cg.eval(call->args[1], x, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("ge " + target + ", " + cg.fmt_operand(x, dm) + ", " + cg.fmt_operand(edge, dm));
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool ismoothstep(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 3)) return false;
    std::vector<std::string> temps;
    Operand lo, hi, x;
    if (!cg.eval(call->args[0], lo, temps)) return false;
    if (!cg.eval(call->args[1], hi, temps)) return false;
    if (!cg.eval(call->args[2], x, temps)) return false;
    std::string dm = dst_mask(target);
    if (dm.empty()) dm = "xyzw";
    std::string t1 = cg.alloc_temp(dm, temps);
    std::string t2 = cg.alloc_temp(dm, temps);
    std::string t3 = cg.alloc_temp(dm, temps);
    std::string t1b = t1.substr(0, t1.find('.'));
    std::string t2b = t2.substr(0, t2.find('.'));
    std::string t3b = t3.substr(0, t3.find('.'));
    // t = clamp((x-lo)/(hi-lo), 0, 1); result = t*t*(3-2t)
    cg.emit("add " + t1 + ", " + cg.fmt_operand(x, dm) + ", -" + cg.fmt_operand(lo, dm));
    // Constant edges: fold 1/(hi-lo) into a multiply (matches fxc).
    if (lo.is_immediate && hi.is_immediate && lo.vals.size() == 1 && hi.vals.size() == 1) {
        double inv = 1.0 / (hi.vals[0] - lo.vals[0]);
        cg.emit("mul_sat " + t1 + ", " + cg.format_src(t1b, dm, dm) + ", l(" + format_float(inv) + ")");
    } else {
        cg.emit("add " + t2 + ", " + cg.fmt_operand(hi, dm) + ", -" + cg.fmt_operand(lo, dm));
        cg.emit("div " + t1 + ", " + cg.format_src(t1b, dm, dm) + ", " + cg.format_src(t2b, dm, dm));
        cg.emit("mov_sat " + t1 + ", " + cg.format_src(t1b, dm, dm));
    }
    cg.emit("mad " + t2 + ", " + cg.format_src(t1b, dm, dm) + ", l(-2.0), l(3.0)");  // 3-2t
    cg.emit("mul " + t3 + ", " + cg.format_src(t1b, dm, dm) + ", " + cg.format_src(t1b, dm, dm));  // t^2
    cg.emit("mul " + target + ", " + cg.format_src(t3b, dm, dm) + ", " + cg.format_src(t2b, dm, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool ilerp(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 3)) return false;
    std::vector<std::string> temps;
    Operand a, b, s;
    if (!cg.eval(call->args[0], a, temps)) return false;
    if (!cg.eval(call->args[1], b, temps)) return false;
    if (!cg.eval(call->args[2], s, temps)) return false;
    std::string dm = dst_mask(target);
    std::string t = cg.alloc_temp(dm, temps);
    std::string tb = t.substr(0, t.find('.'));
    cg.emit("add " + t + ", " + cg.fmt_operand(b, dm) + ", -" + cg.fmt_operand(a, dm));
    cg.emit("mad " + target + ", " + cg.fmt_operand(make_reg_operand(tb, dm), dm) + ", " + cg.fmt_operand(s, dm) + ", " + cg.fmt_operand(a, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool ipow(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 2)) return false;
    // Constant small integer exponent: expand to repeated multiplication
    // (matches DXC/fxc for pow(x, 2/3/4)).
    if (call->args[1]->kind == Expr::Kind::ConstVec && call->args[1]->elems.size() == 1) {
        double p = call->args[1]->elems[0];
        int n = (int)p;
        if (p == (double)n && n >= 1 && n <= 4) {
            std::vector<std::string> temps;
            Operand x;
            if (!cg.eval(call->args[0], x, temps)) return false;
            std::string dm = dst_mask(target);
            if (n == 1) {
                cg.emit("mov " + target + ", " + cg.fmt_operand(x, dm));
            } else if (n == 2) {
                cg.emit("mul " + target + ", " + cg.fmt_operand(x, dm) + ", " + cg.fmt_operand(x, dm));
            } else {
                std::string acc = cg.alloc_temp(dm, temps);
                std::string accb = acc.substr(0, acc.find('.'));
                cg.emit("mul " + acc + ", " + cg.fmt_operand(x, dm) + ", " + cg.fmt_operand(x, dm));
                for (int i = 2; i < n; ++i)
                    cg.emit("mul " + acc + ", " + cg.format_src(accb, dm, dm) + ", " + cg.fmt_operand(x, dm));
                cg.emit("mov " + target + ", " + cg.format_src(accb, dm, dm));
            }
            for (auto& tt : temps) cg.sym.free_temp(tt);
            return true;
        }
    }
    std::vector<std::string> temps;
    Operand x, y;
    if (!cg.eval(call->args[0], x, temps)) return false;
    if (!cg.eval(call->args[1], y, temps)) return false;
    std::string dm = dst_mask(target);
    std::string t = cg.alloc_temp(dm, temps);
    std::string tb = t.substr(0, t.find('.'));
    cg.emit("log " + t + ", " + cg.fmt_operand(x, dm));
    cg.emit("mul " + t + ", " + cg.format_src(tb, dm, dm) + ", " + cg.fmt_operand(y, dm));
    cg.emit("exp " + target + ", " + cg.format_src(tb, dm, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool iexp(CodeGen& cg, Expr* call, const std::string& target) {
    // exp(x) = exp2(x * log2(e))
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand x;
    if (!cg.eval(call->args[0], x, temps)) return false;
    std::string dm = dst_mask(target);
    std::string t = cg.alloc_temp(dm, temps);
    std::string tb = t.substr(0, t.find('.'));
    cg.emit("mul " + t + ", " + cg.fmt_operand(x, dm) + ", l(1.44269504088896)");
    cg.emit("exp " + target + ", " + cg.format_src(tb, dm, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool ilog(CodeGen& cg, Expr* call, const std::string& target) {
    // log(x) = log2(x) * ln(2)
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand x;
    if (!cg.eval(call->args[0], x, temps)) return false;
    std::string dm = dst_mask(target);
    std::string t = cg.alloc_temp(dm, temps);
    std::string tb = t.substr(0, t.find('.'));
    cg.emit("log " + t + ", " + cg.fmt_operand(x, dm));
    cg.emit("mul " + target + ", " + cg.format_src(tb, dm, dm) + ", l(0.693147180559945)");
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool isign(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    if (call->args[0]->kind == Expr::Kind::ConstVec) {
        std::vector<double> vals;
        for (double v : call->args[0]->elems)
            vals.push_back(v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0));
        std::string dm = dst_mask(target);
        cg.emit("mov " + target + ", " + cg.format_imm(vals, dm));
        return true;
    }
    std::vector<std::string> temps;
    Operand x;
    if (!cg.eval(call->args[0], x, temps)) return false;
    std::string dm = dst_mask(target);
    std::string t = cg.alloc_temp(dm, temps);
    std::string tb = t.substr(0, t.find('.'));
    std::string lt = cg.alloc_temp(dm, temps);
    std::string gt = cg.alloc_temp(dm, temps);
    cg.emit("lt " + lt + ", " + cg.fmt_operand(x, dm) + ", l(0.0)");
    cg.emit("gt " + gt + ", " + cg.fmt_operand(x, dm) + ", l(0.0)");
    cg.emit("movc " + t + ", " + cg.format_src(lt.substr(0, lt.find('.')), dm, dm) + ", l(-1.0), l(0.0)");
    cg.emit("movc " + target + ", " + cg.format_src(gt.substr(0, gt.find('.')), dm, dm) + ", l(1.0), " + cg.format_src(tb, dm, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool iselect(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 3)) return false;
    std::vector<std::string> temps;
    Operand b, t, f;
    if (!cg.eval(call->args[0], b, temps)) return false;
    if (!cg.eval(call->args[1], t, temps)) return false;
    if (!cg.eval(call->args[2], f, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("movc " + target + ", " + cg.fmt_operand(b, dm) + ", " + cg.fmt_operand(t, dm) + ", " +
            cg.fmt_operand(f, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

static bool imov(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("mov " + target + ", " + cg.fmt_operand(a, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

// Reduce a vector to a scalar bool via or/and chain.
static bool reduce_bool(CodeGen& cg, Expr* call, const std::string& target, const char* op) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string mask = a.is_immediate ? "x" : a.mask;
    if (mask.empty()) mask = "x";
    if (mask.size() <= 1) {
        cg.emit("mov " + target + ", " + cg.fmt_operand(a, "x"));
    } else {
        std::string acc = cg.alloc_temp("x", temps);
        std::string accb = acc.substr(0, acc.find('.'));
        cg.emit(std::string(op) + " " + acc + ", " + cg.format_src(a.reg, std::string(1, mask[0]), "x") +
                ", " + cg.format_src(a.reg, std::string(1, mask[1]), "x"));
        for (size_t i = 2; i < mask.size(); ++i)
            cg.emit(std::string(op) + " " + acc + ", " + cg.format_src(accb, "x", "x") + ", " +
                    cg.format_src(a.reg, std::string(1, mask[i]), "x"));
        cg.emit("mov " + target + ", " + cg.format_src(accb, "x", "x"));
    }
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

static bool iany(CodeGen& cg, Expr* call, const std::string& target) {
    return reduce_bool(cg, call, target, "or");
}
static bool iall(CodeGen& cg, Expr* call, const std::string& target) {
    return reduce_bool(cg, call, target, "and");
}

// radians(x) = x * pi/180; degrees(x) = x * 180/pi
static bool iscale(CodeGen& cg, Expr* call, const std::string& target, double factor) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("mul " + target + ", " + cg.fmt_operand(a, dm) + ", l(" + format_float(factor) + ")");
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}
static bool iradians(CodeGen& cg, Expr* call, const std::string& t) {
    return iscale(cg, call, t, 0.017453292519943295);  // pi/180
}
static bool idegrees(CodeGen& cg, Expr* call, const std::string& t) {
    return iscale(cg, call, t, 57.29577951308232);  // 180/pi
}

// isnan(x): x != x  ;  isfinite(x): abs(x) <= FLT_MAX
static bool iisnan(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    std::string s = cg.fmt_operand(a, dm);
    cg.emit("ne " + target + ", " + s + ", " + s);
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}
static bool iisfinite(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 1)) return false;
    std::vector<std::string> temps;
    Operand a;
    if (!cg.eval(call->args[0], a, temps)) return false;
    std::string dm = dst_mask(target);
    cg.emit("mov " + target + ", |" + cg.fmt_operand(a, dm) + "|");
    cg.emit("le " + target + ", " + target + ", l(3.402823e+38)");
    for (auto& t : temps) cg.sym.free_temp(t);
    return true;
}

// fmod(x, y) = x - y * trunc(x / y)
static bool ifmod(CodeGen& cg, Expr* call, const std::string& target) {
    if (!check_args(call, 2)) return false;
    std::vector<std::string> temps;
    Operand x, y;
    if (!cg.eval(call->args[0], x, temps)) return false;
    if (!cg.eval(call->args[1], y, temps)) return false;
    std::string dm = dst_mask(target);
    if (dm.empty()) dm = "xyzw";
    std::string t = cg.alloc_temp(dm, temps);
    std::string tb = t.substr(0, t.find('.'));
    cg.emit("div " + t + ", " + cg.fmt_operand(x, dm) + ", " + cg.fmt_operand(y, dm));
    cg.emit("round_z " + t + ", " + cg.format_src(tb, dm, dm));
    cg.emit("mad " + target + ", " + cg.format_src(tb, dm, dm) + ", -" + cg.fmt_operand(y, dm) + ", " + cg.fmt_operand(x, dm));
    for (auto& tt : temps) cg.sym.free_temp(tt);
    return true;
}

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------

const std::vector<std::pair<std::string, IntrinsicInfo>>& intrinsic_table() {
    static const std::vector<std::pair<std::string, IntrinsicInfo>> table = {
        {"add", {iadd, "float"}},
        {"sub", {isub, "float"}},
        {"mul", {imul, "float"}},
        {"div", {idiv, "float"}},
        {"max", {imax, "float"}},
        {"min", {imin, "float"}},
        {"clamp", {iclamp, "float", 3}},
        {"saturate", {isaturate, "float", 1}},
        {"abs", {iabs, "float", 1}},
        {"sqrt", {isqrt, "float", 1}},
        {"rsqrt", {irsqrt, "float", 1}},
        {"ddx", {iddx, "float", 1}},
        {"ddy", {iddy, "float", 1}},
        {"rcp", {ircp, "float", 1}},
        {"dot", {idot, "float", 2}},
        {"length", {ilength, "float", 1}},
        {"distance", {idistance, "float", 2}},
        {"normalize", {inormalize, "float3", 1}},
        {"cross", {icross, "float3", 2}},
        {"step", {istep, "float", 2}},
        {"smoothstep", {ismoothstep, "float", 3}},
        {"lerp", {ilerp, "float", 3}},
        {"pow", {ipow, "float", 2}},
        {"exp", {iexp, "float", 1}},
        {"exp2", {iexp2, "float", 1}},
        {"log", {ilog, "float", 1}},
        {"log2", {ilog2, "float", 1}},
        {"sin", {isin, "float", 1}},
        {"cos", {icos, "float", 1}},
        {"sincos", {isincos, "void", 3}},
        {"floor", {ifloor, "float", 1}},
        {"ceil", {iceil, "float", 1}},
        {"trunc", {itrunc, "float", 1}},
        {"frc", {ifrc, "float", 1}},
        {"frac", {ifrc, "float", 1}},
        {"round", {iround, "float", 1}},
        {"sign", {isign, "float", 1}},
        {"select", {iselect, "float", 3}},
        {"any", {iany, "bool", 1}},
        {"all", {iall, "bool", 1}},
        {"radians", {iradians, "float", 1}},
        {"degrees", {idegrees, "float", 1}},
        {"isnan", {iisnan, "bool", 1}},
        {"isfinite", {iisfinite, "bool", 1}},
        {"fmod", {ifmod, "float", 2}},
        {"and", {iand, "uint", 2}},
        {"or", {ior, "uint", 2}},
        {"xor", {ixor, "uint", 2}},
        {"not", {inot, "uint", 1}},
        {"shl", {ishl, "uint", 2}},
        {"shr", {ishr, "uint", 2}},
        {"ushr", {iushr, "uint", 2}},
        {"asfloat", {imov, "float", 1}},
        {"asuint", {imov, "uint", 1}},
        {"asint", {imov, "int", 1}},
    };
    return table;
}

int intrinsic_return_dim(const std::string& name, const std::vector<Expr*>& args,
                         SymbolTable& sym) {
    if (name == "dot" || name == "length" || name == "distance")
        return 1;
    if (name == "sincos")
        return 0;
    if (name == "cross")
        return 3;
    if (name == "normalize") {
        if (!args.empty())
            return std::max(1, infer_expr_dim(args[0], sym));
        return 3;
    }
    int maxd = 1;
    for (Expr* a : args)
        maxd = std::max(maxd, infer_expr_dim(a, sym));
    if (name == "step" && args.size() == 2)
        return std::max(1, infer_expr_dim(args[1], sym));
    return maxd;
}

Type intrinsic_return_type(const std::string& name, const std::vector<Expr*>& args,
                           SymbolTable& sym) {
    int dim = intrinsic_return_dim(name, args, sym);
    BaseType base = BaseType::Float;
    if (name == "and" || name == "or" || name == "xor" || name == "not" || name == "shl" ||
        name == "shr" || name == "ushr" || name == "asuint")
        base = BaseType::Uint;
    else if (name == "asint")
        base = BaseType::Int;
    if (dim <= 1) return make_scalar(base);
    return make_vector(dim, base);
}

} // namespace hb
