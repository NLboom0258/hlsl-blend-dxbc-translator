// translator.h - main line-by-line translator
#pragma once

#include <map>
#include <string>
#include <vector>

#include "ast.h"
#include "codegen.h"
#include "function_lib.h"
#include "symbols.h"

namespace hb {

class Translator {
public:
    Translator();

    // Set base directory for resolving HLSLFunctionImport relative paths.
    void set_data_dir(const std::string& dir) { data_dir_ = dir; }

    // Translate input lines (DXBC + HLSL blend markers) to pure DXBC asm lines.
    // Output lines have no trailing newline. Returns false on fatal error.
    bool run(const std::vector<std::string>& input_lines,
             std::vector<std::string>& out_lines,
             std::string& error);

private:
    std::string data_dir_;
    std::map<std::string, FunctionDef> functions_;

    // Per-run state
    SymbolTable* symtab_ = nullptr;
    Arena arena_;
    StmtArena stmt_arena_;
    CodeGen* cg_ = nullptr;
    std::vector<std::string>* out_ = nullptr;
    std::string return_target_;  // set during custom function expansion

    int find_max_register(const std::vector<std::string>& lines) const;
    bool load_function_libraries(const std::vector<std::string>& lines, std::string& error);

    // Marker handlers
    bool handle_hlsl_mov(const std::string& line, const std::string& indent, std::string& error);
    bool handle_dxbc_mov(const std::string& line, const std::string& indent, std::string& error);
    bool handle_texture(const std::string& line, const std::string& indent, std::string& error);
    bool handle_sampler(const std::string& line, const std::string& indent, std::string& error);
    bool handle_hlsl_statement(const std::string& stmt_text, const std::string& indent, std::string& error);
    bool handle_hlsl_snippet(const std::vector<std::string>& lines, size_t& i,
                             const std::string& indent, std::string& error);

    // Statement translation
    bool translate_stmt(Stmt* s, const std::string& indent, std::string& error);
    bool translate_decl(Stmt* s, const std::string& indent, std::string& error);
    bool translate_assign(Stmt* s, const std::string& indent, std::string& error);
    bool translate_if(Stmt* s, const std::string& indent, std::string& error);
    bool translate_while(Stmt* s, const std::string& indent, std::string& error);
    bool translate_for(Stmt* s, const std::string& indent, std::string& error);
    bool gen_condition(Expr* cond, std::string& cond_reg, std::string& error);
    bool expand_function_call(Expr* call, const std::string& target, std::string& error);

    // Helpers
    static std::string strip_comments(const std::string& s);
    static bool extract_braced_block(const std::vector<std::string>& lines, size_t start,
                                     std::string& body, size_t& next_index);
    static bool split_lhs_type_name(const std::string& lhs, bool& has_type,
                                    std::string& type_name, std::string& var_name,
                                    std::string& var_mask);
    bool bind_variable(const std::string& name, const std::string& reg,
                       const std::string& mask, const Type& type);
};

// Standalone entry point.
bool translate_lines(const std::vector<std::string>& input,
                     std::vector<std::string>& out,
                     const std::string& data_dir,
                     std::string& error);

} // namespace hb
