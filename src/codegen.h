// codegen.h - expression -> DXBC instruction generation
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ast.h"
#include "symbols.h"

namespace hb {

class CodeGen;

// Result of evaluating an expression into an operand.
struct Operand {
    std::string reg;          // register base for register operands ("r5", "cb0[3]")
    std::string mask;         // raw source mask (1-4 chars) for register operands
    std::vector<double> vals; // immediate values for immediate operands
    bool is_immediate = false;
    int dim = 1;              // number of meaningful components
};

// Build a register operand helper.
inline Operand make_reg_operand(const std::string& reg, const std::string& mask, int dim = 1) {
    Operand o;
    o.reg = reg;
    o.mask = mask;
    o.dim = dim;
    return o;
}

// Intrinsic implementation signature. Emits instructions into cg's output and
// returns true on success. target_str is the destination ("r5.xyzw").
using IntrinsicFn = bool (*)(CodeGen& cg, Expr* call, const std::string& target_str);

struct IntrinsicInfo {
    IntrinsicFn fn;
    std::string return_type;   // "float3", "float", "void", ...
    int fixed_args = -1;       // -1 = variadic
};

// Central intrinsic table (DXC-style).
const std::vector<std::pair<std::string, IntrinsicInfo>>& intrinsic_table();

class CodeGen {
public:
    CodeGen(SymbolTable& symtab, Arena& arena, std::vector<std::string>& out);

    // Emit instructions assigning expr to target_str ("r5.xy", "o0.xyzw").
    // Returns false on failure (recorded in error_).
    bool gen_assignment(Expr* e, const std::string& target_str);

    // Public wrapper for emitting a function-call statement (void intrinsics /
    // custom functions). target_str may be empty for pure statements.
    bool emit_call(Expr* e, const std::string& target_str) { return gen_call(e, target_str); }

    // Evaluate expr to an operand. Allocates temps as needed; the allocated
    // temp bases are appended to `temps` (caller frees them after use).
    bool eval(Expr* e, Operand& out, std::vector<std::string>& temps);

    // Type inference
    Type infer_type(Expr* e);
    int infer_dim(Expr* e);

    // ---- emission helpers (used by intrinsic impls) ----
    // Allocate a temp with mask; returns "rN.mask" and records base in temps_.
    std::string alloc_temp(const std::string& mask, std::vector<std::string>& temps);
    // Set the indent prepended to every emitted instruction line.
    void set_indent(const std::string& indent) { indent_ = indent; }
    void emit(const std::string& line) { out_.push_back(indent_ + line); }
    std::vector<std::string>& output() { return out_; }

    // Format a source register operand given source mask and dest writemask.
    std::string format_src(const std::string& reg, const std::string& src_mask,
                           const std::string& dst_mask) const;
    // Format an immediate from a const vector, aligned to dst mask.
    std::string format_imm(const std::vector<double>& vals, const std::string& dst_mask) const;
    // Format an immediate as integer literals (for int/uint operations).
    std::string format_imm_int(const std::vector<double>& vals, const std::string& dst_mask) const;
    std::string format_imm_scalar(double v) const;
    // Format an evaluated operand (register or immediate) against a dest mask.
    std::string fmt_operand(const Operand& op, const std::string& dst_mask) const;

    // Attempt to fold saturate(expr) into the expr's final instruction
    // (e.g. mul_sat). Returns true if folded; caller falls back to mov_sat.
    bool gen_fold_saturate(Expr* arg, const std::string& target_str);

    // Dest mask from a target string ("r5.xy" -> "xy").
    static std::string target_mask_of(const std::string& target);
    // Register of the i-th row of a matrix variable: "r5" -> "r6",
    // "cb0[3]" -> "cb0[4]".
    static std::string reg_plus(const std::string& base, int i);

    const std::string& error() const { return error_; }
    void set_error(const std::string& s) { error_ = s; }

    SymbolTable& sym;

    // Current instruction saturate fold state (set while lowering saturate(expr)).
    bool fold_sat_ = false;
    // When true, saturate() is lowered as min/max instead of _sat modifiers
    // (mirrors the Python reference output; useful for A/B testing).
    bool use_minmax_sat = false;
    std::string sat_suffix() const { return fold_sat_ ? "_sat" : ""; }

    // Custom function expansion hook (set by the Translator for
    // HLSLFunctionImport libraries). Returns true on success, false if the
    // function is not a known custom function (gen_call then reports an error).
    using CustomExpander = std::function<bool(CodeGen&, Expr* call, const std::string& target_str)>;
    void set_custom_expander(CustomExpander f) { custom_expander_ = std::move(f); }

private:
    bool gen_internal(Expr* e, const std::string& target_str);
    bool gen_binop(Expr* e, const std::string& target_str);
    bool gen_unary(Expr* e, const std::string& target_str);
    bool gen_cmp(Expr* e, const std::string& target_str);
    bool gen_ternary(Expr* e, const std::string& target_str);
    bool gen_call(Expr* e, const std::string& target_str);
    bool gen_construct(Expr* e, const std::string& target_str);
    bool gen_sample(Expr* e, const std::string& target_str);
    bool gen_cast(Expr* e, const std::string& target_str);
    bool gen_index(Expr* e, const std::string& target_str);

    bool eval_binary_operand(Expr* operand_expr, std::string& operand, int& dim,
                             std::vector<std::string>& temps, std::string& pre_instrs);

    Arena& arena_;
    std::vector<std::string>& out_;
    std::string error_;
    std::string indent_;
    int temp_counter_ = 0;
    CustomExpander custom_expander_;
};

// Formatting helpers
std::string format_float(double v);
std::string format_operand(const std::string& reg, const std::string& mask);

// Standalone dimension inference (usable without a CodeGen instance).
int infer_expr_dim(Expr* e, SymbolTable& sym);

} // namespace hb
