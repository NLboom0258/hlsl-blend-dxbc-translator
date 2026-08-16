// translator.cpp
#include "translator.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>

#include "parser.h"
#include "swizzle.h"

namespace hb {

Translator::Translator() {}

std::string Translator::strip_comments(const std::string& s) {
    std::string out;
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        if (s[i] == '/' && i + 1 < n && s[i + 1] == '/') {
            // skip to end of line (the input may be multi-line)
            while (i < n && s[i] != '\n') ++i;
            continue;
        }
        if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) ++i;
            i += 2;
            continue;
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

int Translator::find_max_register(const std::vector<std::string>& lines) const {
    static const std::regex re(R"(\br(\d+)\b)");
    int max_reg = -1;
    for (const std::string& line : lines) {
        std::string clean = strip_comments(line);
        for (auto it = std::sregex_iterator(clean.begin(), clean.end(), re);
             it != std::sregex_iterator(); ++it) {
            int n = std::atoi((*it)[1].str().c_str());
            if (n > max_reg) max_reg = n;
        }
    }
    return max_reg + 1;
}

bool Translator::load_function_libraries(const std::vector<std::string>& lines, std::string& error) {
    static const std::regex import_re(R"re(^HLSLFunctionImport\s+"([^"]+)"\s*;?\s*$)re");
    for (const std::string& raw : lines) {
        std::string stripped = raw;
        stripped.erase(0, stripped.find_first_not_of(" \t"));
        std::smatch m;
        if (std::regex_match(stripped, m, import_re)) {
            std::string rel = m[1].str();
            // Normalize path separators for Windows.
            std::string path = data_dir_.empty() ? rel : data_dir_ + "\\" + rel;
            for (auto& c : path) if (c == '/') c = '\\';
            FILE* f = fopen(path.c_str(), "rb");
            if (!f) {
                // try relative to cwd too
                f = fopen(rel.c_str(), "rb");
            }
            if (!f) {
                error = "function library not found: " + rel;
                return false;
            }
            std::string src;
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) src.append(buf, n);
            fclose(f);
            std::map<std::string, FunctionDef> defs;
            parse_function_library(src, defs);
            for (auto& kv : defs)
                functions_[kv.first] = kv.second;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// HLSLMov / DXBCMov source operand formatting
// ---------------------------------------------------------------------------

static bool parse_register_source(const std::string& src_in, std::string& base,
                                  std::string& mask, bool& negated) {
    std::string s = src_in;
    // strip trailing whitespace/semicolon
    while (!s.empty() && (s.back() == ';' || s.back() == ' ' || s.back() == '\t' ||
                          s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    negated = false;
    if (!s.empty() && s[0] == '-') { negated = true; s = s.substr(1); }
    // Find the last '.' whose remainder is a valid mask.
    size_t dot = s.rfind('.');
    if (dot != std::string::npos && dot + 1 < s.size()) {
        std::string after = s.substr(dot + 1);
        bool valid = !after.empty() && after.size() <= 4;
        for (char c : after)
            if (std::string("xyzw").find(c) == std::string::npos) valid = false;
        if (valid) {
            base = s.substr(0, dot);
            mask = after;
            return true;
        }
    }
    base = s;
    mask = "xyzw";
    return true;
}

static std::string format_source_operand(const std::string& src, const std::string& dst_mask) {
    std::string base, mask;
    bool neg;
    parse_register_source(src, base, mask, neg);
    Swizzle sw = compute_src_swizzle(mask, dst_mask);
    std::string res = base + "." + swizzle_to_string(sw);
    if (neg) res = "-" + res;
    return res;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

bool Translator::run(const std::vector<std::string>& input_lines,
                     std::vector<std::string>& out_lines,
                     std::string& error) {
    out_ = &out_lines;
    out_lines.clear();
    functions_.clear();

    if (!load_function_libraries(input_lines, error)) return false;

    int min_reg = find_max_register(input_lines);
    SymbolTable symtab(min_reg);
    symtab_ = &symtab;

    CodeGen cg(symtab, arena_, out_lines);
    cg_ = &cg;
    cg.set_custom_expander([this](CodeGen&, Expr* call, const std::string& target) -> bool {
        std::string err;
        return expand_function_call(call, target, err);
    });

    int init_marker_index = -1;
    std::string init_marker_indent;

    size_t i = 0;
    const size_t n = input_lines.size();
    while (i < n) {
        std::string raw = input_lines[i];
        // Normalize line ending
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        std::string stripped = raw;
        stripped.erase(0, stripped.find_first_not_of(" \t"));
        std::string content = strip_comments(stripped);
        // trim trailing whitespace
        while (!content.empty() && (content.back() == ' ' || content.back() == '\t'))
            content.pop_back();

        std::string indent = raw.substr(0, raw.size() - stripped.size());

        if (content.compare(0, 18, "HLSLFunctionImport") == 0) {
            ++i;
            continue;
        }
        if (content == "HLSLInit") {
            init_marker_index = (int)out_lines.size();
            init_marker_indent = indent;
            ++i;
            continue;
        }
        if (content.compare(0, 7, "HLSLMov") == 0) {
            arena_.clear();
            stmt_arena_.clear();
            if (!handle_hlsl_mov(content, indent, error)) {
                out_lines.push_back(indent + "// Error: " + error);
                error.clear();
            }
            ++i;
            continue;
        }
        if (content.compare(0, 7, "DXBCMov") == 0) {
            if (!handle_dxbc_mov(content, indent, error)) {
                out_lines.push_back(indent + "// Error: " + error);
                error.clear();
            }
            ++i;
            continue;
        }
        if (content.compare(0, 11, "HLSLTexture") == 0) {
            if (!handle_texture(content, indent, error)) {
                out_lines.push_back(indent + "// Error: " + error);
                error.clear();
            }
            ++i;
            continue;
        }
        if (content.compare(0, 11, "HLSLSampler") == 0) {
            if (!handle_sampler(content, indent, error)) {
                out_lines.push_back(indent + "// Error: " + error);
                error.clear();
            }
            ++i;
            continue;
        }
        if (content == "HLSL" || content.compare(0, 5, "HLSL ") == 0) {
            arena_.clear();
            stmt_arena_.clear();
            std::string stmt_text = content.size() > 5 ? content.substr(5) : "";
            if (!handle_hlsl_statement(stmt_text, indent, error)) {
                out_lines.push_back(indent + "// HLSL: " + stmt_text + "  ; Error: " + error);
                error.clear();
            }
            ++i;
            continue;
        }
        if (content.compare(0, 11, "HLSLSnippet") == 0) {
            if (!handle_hlsl_snippet(input_lines, i, indent, error)) {
                out_lines.push_back(indent + "// HLSLSnippet error: " + error);
                error.clear();
                ++i;
            }
            continue;
        }

        // Pass through
        out_lines.push_back(raw);
        ++i;
    }

    // Emit HLSLInit block
    if (init_marker_index >= 0) {
        std::vector<std::string> block;
        block.push_back(init_marker_indent + "// HLSLInit begin");
        for (const std::string& base : symtab.all_allocated_bases())
            block.push_back(init_marker_indent + "mov " + base + ".xyzw, l(0)");
        block.push_back(init_marker_indent + "// HLSLInit end");
        out_lines.insert(out_lines.begin() + init_marker_index, block.begin(), block.end());
    }

    return true;
}

// ---------------------------------------------------------------------------
// Marker handlers
// ---------------------------------------------------------------------------

bool Translator::split_lhs_type_name(const std::string& lhs, bool& has_type,
                                     std::string& type_name, std::string& var_name,
                                     std::string& var_mask) {
    has_type = false;
    type_name.clear();
    var_name.clear();
    var_mask.clear();
    // lhs like "float3 X.xyz" or "X.xyz" or "float X"
    std::vector<std::string> toks;
    std::string cur;
    for (char c : lhs) {
        if (c == ' ' || c == '\t') {
            if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
        } else cur.push_back(c);
    }
    if (!cur.empty()) toks.push_back(cur);
    if (toks.empty()) return false;
    size_t idx = 0;
    // optional storage modifier (already stripped mostly)
    if (toks[0] == "const" || toks[0] == "static") idx = 1;
    Type t;
    if (idx < toks.size() && parse_type_name(toks[idx], t)) {
        has_type = true;
        type_name = toks[idx];
        idx++;
        if (idx >= toks.size()) return false;
    }
    std::string name_mask = toks[idx];
    size_t dot = name_mask.find('.');
    if (dot != std::string::npos) {
        var_name = name_mask.substr(0, dot);
        var_mask = name_mask.substr(dot + 1);
    } else {
        var_name = name_mask;
    }
    return true;
}

bool Translator::bind_variable(const std::string& name, const std::string& reg,
                               const std::string& mask, const Type& type) {
    symtab_->declare(name, reg, mask, type, true);
    return true;
}

bool Translator::handle_hlsl_mov(const std::string& line, const std::string& indent,
                                 std::string& error) {
    // Extract trailing comment for attachment
    std::string comment;
    {
        size_t cpos = line.find("//");
        if (cpos != std::string::npos) comment = line.substr(cpos);
    }
    std::string s = strip_comments(line).substr(7);  // after "HLSLMov"
    // trim + strip trailing ';'
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ';' || s.back() == ' ' || s.back() == '\t')) s.pop_back();

    size_t eq = s.find('=');
    if (eq == std::string::npos) { error = "HLSLMov: missing '='"; return false; }
    std::string lhs = s.substr(0, eq);
    std::string rhs = s.substr(eq + 1);
    while (!lhs.empty() && (lhs.back() == ' ' || lhs.back() == '\t')) lhs.pop_back();
    while (!rhs.empty() && (rhs.front() == ' ' || rhs.front() == '\t')) rhs.erase(0, 1);

    bool has_type;
    std::string type_name, var_name, var_mask;
    if (!split_lhs_type_name(lhs, has_type, type_name, var_name, var_mask)) {
        error = "HLSLMov: bad lhs";
        return false;
    }

    Type type;
    if (has_type && !parse_type_name(type_name, type)) {
        error = "HLSLMov: unknown type " + type_name;
        return false;
    }

    Symbol* existing = symtab_->lookup(var_name);
    if (has_type) {
        if (!existing) {
            // typed declaration
            std::string mask = var_mask;
            if (mask.empty()) {
                int w = type.component_count();
                mask = std::string("xyzw").substr(0, (size_t)std::min(w, 4));
                if (mask.empty()) mask = "x";
            }
            std::string reg_str = symtab_->alloc_temp(mask);
            bind_variable(var_name, reg_str.substr(0, reg_str.find('.')), mask, type);
            // writemask for the mov
            std::string write_mask = var_mask.empty() ? mask : var_mask;
            std::string target = reg_str.substr(0, reg_str.find('.')) + "." + write_mask;
            std::string src = format_source_operand(rhs, write_mask);
            std::string instr = "mov " + target + ", " + src;
            if (!comment.empty()) instr += " " + comment;
            out_->push_back(indent + instr);
            return true;
        } else {
            // redeclare: assign to existing variable
            std::string write_mask = var_mask.empty() ? existing->mask : var_mask;
            std::string target = existing->reg + "." + write_mask;
            std::string src = format_source_operand(rhs, write_mask);
            std::string instr = "mov " + target + ", " + src;
            if (!comment.empty()) instr += " " + comment;
            out_->push_back(indent + instr);
            return true;
        }
    } else {
        if (!existing) { error = "HLSLMov: variable not defined: " + var_name; return false; }
        std::string write_mask = var_mask.empty() ? existing->mask : var_mask;
        std::string target = existing->reg + "." + write_mask;
        std::string src = format_source_operand(rhs, write_mask);
        std::string instr = "mov " + target + ", " + src;
        if (!comment.empty()) instr += " " + comment;
        out_->push_back(indent + instr);
        return true;
    }
}

bool Translator::handle_dxbc_mov(const std::string& line, const std::string& indent,
                                 std::string& error) {
    std::string comment;
    {
        size_t cpos = line.find("//");
        if (cpos != std::string::npos) comment = line.substr(cpos);
    }
    std::string s = strip_comments(line).substr(7);  // after "DXBCMov"
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ';' || s.back() == ' ' || s.back() == '\t')) s.pop_back();

    size_t comma = s.find(',');
    if (comma == std::string::npos) { error = "DXBCMov: missing ','"; return false; }
    std::string dest = s.substr(0, comma);
    std::string var_s = s.substr(comma + 1);
    while (!dest.empty() && (dest.back() == ' ' || dest.back() == '\t')) dest.pop_back();
    while (!var_s.empty() && (var_s.front() == ' ' || var_s.front() == '\t')) var_s.erase(0, 1);

    std::string var_name = var_s, var_mask;
    size_t dot = var_s.find('.');
    if (dot != std::string::npos) {
        var_name = var_s.substr(0, dot);
        var_mask = var_s.substr(dot + 1);
    }
    Symbol* sym = symtab_->lookup(var_name);
    if (!sym) { error = "DXBCMov: variable not defined: " + var_name; return false; }

    std::string dst_mask;
    size_t ddot = dest.rfind('.');
    if (ddot != std::string::npos && ddot + 1 < dest.size())
        dst_mask = dest.substr(ddot + 1);
    if (dst_mask.empty()) dst_mask = "xyzw";

    std::string src_mask = var_mask.empty() ? sym->mask : var_mask;
    Swizzle sw = compute_src_swizzle(src_mask, dst_mask);
    std::string src = sym->reg + "." + swizzle_to_string(sw);
    std::string instr = "mov " + dest + ", " + src;
    if (!comment.empty()) instr += " " + comment;
    out_->push_back(indent + instr);
    return true;
}

bool Translator::handle_texture(const std::string& line, const std::string& indent,
                                std::string& error) {
    // HLSLTexture Texture2D Name = t6;
    std::string s = strip_comments(line).substr(10);
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ';' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t eq = s.find('=');
    if (eq == std::string::npos) { error = "HLSLTexture: missing '='"; return false; }
    std::string lhs = s.substr(0, eq);
    std::string rhs = s.substr(eq + 1);
    // lhs: <Texture2D> <Name>
    std::vector<std::string> toks;
    {
        std::string cur;
        for (char c : lhs) {
            if (c == ' ' || c == '\t') { if (!cur.empty()) { toks.push_back(cur); cur.clear(); } }
            else cur.push_back(c);
        }
        if (!cur.empty()) toks.push_back(cur);
    }
    if (toks.size() < 2) { error = "HLSLTexture: bad lhs"; return false; }
    std::string alias = toks[toks.size() - 1];
    std::string slot = rhs;
    while (!slot.empty() && (slot.front() == ' ' || slot.front() == '\t')) slot.erase(0, 1);
    while (!slot.empty() && (slot.back() == ';' || slot.back() == ' ' || slot.back() == '\t')) slot.pop_back();
    if (slot.empty() || slot[0] != 't') { error = "HLSLTexture: bad slot"; return false; }

    Symbol* existing = symtab_->lookup(alias);
    if (existing && existing->is_texture) return true;
    Type t = make_vector(4, BaseType::Float);
    Symbol sym;
    sym.reg = slot;
    sym.mask = "xyzw";
    sym.type = t;
    sym.is_temp = false;
    sym.is_texture = true;
    symtab_->declare(alias, slot, "xyzw", t, false);
    // override is_texture flag
    Symbol* s2 = symtab_->lookup(alias);
    if (s2) s2->is_texture = true;
    return true;
}

bool Translator::handle_sampler(const std::string& line, const std::string& indent,
                                std::string& error) {
    std::string s = strip_comments(line).substr(11);
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ';' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t eq = s.find('=');
    if (eq == std::string::npos) { error = "HLSLSampler: missing '='"; return false; }
    std::string lhs = s.substr(0, eq);
    std::string rhs = s.substr(eq + 1);
    std::vector<std::string> toks;
    {
        std::string cur;
        for (char c : lhs) {
            if (c == ' ' || c == '\t') { if (!cur.empty()) { toks.push_back(cur); cur.clear(); } }
            else cur.push_back(c);
        }
        if (!cur.empty()) toks.push_back(cur);
    }
    if (toks.size() < 2) { error = "HLSLSampler: bad lhs"; return false; }
    std::string alias = toks[toks.size() - 1];
    std::string slot = rhs;
    while (!slot.empty() && (slot.front() == ' ' || slot.front() == '\t')) slot.erase(0, 1);
    while (!slot.empty() && (slot.back() == ';' || slot.back() == ' ' || slot.back() == '\t')) slot.pop_back();
    if (slot.empty() || slot[0] != 's') { error = "HLSLSampler: bad slot"; return false; }
    Type t = make_scalar(BaseType::Float);
    symtab_->declare(alias, slot, "xyzw", t, false);
    Symbol* s2 = symtab_->lookup(alias);
    if (s2) s2->is_sampler = true;
    return true;
}

bool Translator::handle_hlsl_statement(const std::string& stmt_text, const std::string& indent,
                                       std::string& error) {
    Parser parser(arena_, stmt_arena_);
    Stmt* s = parser.parse_statement(stmt_text);
    if (!s) { error = parser.error(); if (error.empty()) error = "parse error"; return false; }
    return translate_stmt(s, indent, error);
}

bool Translator::extract_braced_block(const std::vector<std::string>& lines, size_t start,
                                      std::string& body, size_t& next_index) {
    size_t i = start;
    // find the line with '{'
    size_t brace_line = SIZE_MAX;
    size_t brace_col = 0;
    for (; i < lines.size(); ++i) {
        std::string l = lines[i];
        if (!l.empty() && l.back() == '\r') l.pop_back();
        size_t pos = l.find('{');
        if (pos != std::string::npos) { brace_line = i; brace_col = pos; break; }
    }
    if (brace_line == SIZE_MAX) return false;

    int depth = 0;
    bool started = false;
    std::string result;
    for (i = brace_line; i < lines.size(); ++i) {
        std::string l = lines[i];
        if (!l.empty() && l.back() == '\r') l.pop_back();
        size_t begin = (i == brace_line) ? brace_col : 0;
        for (size_t j = begin; j < l.size(); ++j) {
            char c = l[j];
            if (c == '{') {
                if (started) result.push_back(c);  // preserve nested open brace
                ++depth;
                started = true;
            } else if (c == '}') {
                --depth;
                if (depth == 0) {
                    body = result;
                    next_index = i + 1;
                    return true;
                }
                result.push_back(c);
            } else if (started) {
                result.push_back(c);
            }
        }
        if (started && depth > 0) result.push_back('\n');
    }
    return false;
}

bool Translator::handle_hlsl_snippet(const std::vector<std::string>& lines, size_t& i,
                                     const std::string& indent, std::string& error) {
    std::string body;
    size_t next_index;
    if (!extract_braced_block(lines, i, body, next_index)) {
        error = "unmatched braces";
        return false;
    }
    std::string clean = strip_comments(body);
    std::vector<Stmt*> stmts;
    Parser parser(arena_, stmt_arena_);
    if (!parser.parse_statements(clean, stmts)) {
        error = parser.error();
        if (error.empty()) error = "parse error";
        return false;
    }
    out_->push_back(indent + "// HLSLSnippet begin");
    for (Stmt* s : stmts) {
        if (!translate_stmt(s, indent, error)) return false;
    }
    out_->push_back(indent + "// HLSLSnippet end");
    i = next_index;
    return true;
}

// ---------------------------------------------------------------------------
// Statement translation
// ---------------------------------------------------------------------------

static std::string type_mask_for(const Type& t) {
    int w = t.component_count();
    if (w >= 4) return "xyzw";
    if (w <= 1) return "x";
    return std::string("xyzw").substr(0, (size_t)w);
}

bool Translator::translate_stmt(Stmt* s, const std::string& indent, std::string& error) {
    cg_->set_indent(indent);
    switch (s->kind) {
    case Stmt::Kind::Decl:
        return translate_decl(s, indent, error);
    case Stmt::Kind::Assign:
        return translate_assign(s, indent, error);
    case Stmt::Kind::CompoundAssign:
        return translate_assign(s, indent, error);
    case Stmt::Kind::ExprStmt:
        if (s->value && s->value->kind == Expr::Kind::Call) {
            std::string dummy;
            if (!cg_->emit_call(s->value, "")) { error = cg_->error(); return false; }
            return true;
        }
        error = "unsupported expression statement";
        return false;
    case Stmt::Kind::If:
        return translate_if(s, indent, error);
    case Stmt::Kind::While:
        return translate_while(s, indent, error);
    case Stmt::Kind::For:
        return translate_for(s, indent, error);
    case Stmt::Kind::Block:
        for (Stmt* c : s->body)
            if (!translate_stmt(c, indent, error)) return false;
        return true;
    case Stmt::Kind::Return:
        if (s->value) {
            if (return_target_.empty()) {
                // no enclosing function: allocate + discard
                std::string t = symtab_->alloc_temp("xyzw");
                if (!cg_->gen_assignment(s->value, t)) { error = cg_->error(); return false; }
                symtab_->free_temp(t);
                return true;
            }
            if (!cg_->gen_assignment(s->value, return_target_)) { error = cg_->error(); return false; }
        }
        return true;
    case Stmt::Kind::Break:
        out_->push_back(indent + "break");
        return true;
    case Stmt::Kind::Continue:
        out_->push_back(indent + "continue");
        return true;
    case Stmt::Kind::Discard:
        out_->push_back(indent + "discard");
        return true;
    case Stmt::Kind::Nop:
        return true;
    }
    error = "unknown statement kind";
    return false;
}

bool Translator::translate_decl(Stmt* s, const std::string& indent, std::string& error) {
    std::string name = s->name;
    Symbol* existing = symtab_->lookup(name);
    if (existing) {
        // redeclare in same scope: treat as assignment
        if (s->init) {
            std::string target = existing->reg + "." + (s->mask.empty() ? existing->mask : s->mask);
            if (!cg_->gen_assignment(s->init, target)) { error = cg_->error(); return false; }
        }
        return true;
    }
    std::string mask = type_mask_for(s->type);
    std::string reg_str = symtab_->alloc_temp(mask);
    std::string base = reg_str.substr(0, reg_str.find('.'));
    symtab_->declare(name, base, mask, s->type, true);
    if (s->init) {
        std::string write_mask = s->mask.empty() ? mask : s->mask;
        std::string target = base + "." + write_mask;
        if (!cg_->gen_assignment(s->init, target)) { error = cg_->error(); return false; }
    } else {
        out_->push_back(indent + "mov " + reg_str + ", l(0)");
    }
    return true;
}

bool Translator::translate_assign(Stmt* s, const std::string& indent, std::string& error) {
    Expr* lhs = s->lhs;
    if (!lhs || lhs->kind != Expr::Kind::VarRef) {
        error = "unsupported assignment target";
        return false;
    }
    Symbol* sym = symtab_->lookup(lhs->name);
    if (!sym) { error = "variable not defined: " + lhs->name; return false; }
    std::string write_mask = lhs->mask.empty() ? sym->mask : lhs->mask;
    std::string target = sym->reg + "." + write_mask;

    if (s->kind == Stmt::Kind::CompoundAssign) {
        // lhs op= rhs  ->  lhs = lhs op rhs
        std::string op = s->op;
        if (!op.empty()) op.pop_back();  // strip '='
        Expr* bin = arena_.alloc();
        bin->kind = Expr::Kind::BinOp;
        bin->op = op;
        Expr* left_ref = arena_.alloc();
        left_ref->kind = Expr::Kind::VarRef;
        left_ref->name = lhs->name;
        left_ref->mask = lhs->mask;
        bin->left = left_ref;
        bin->right = s->value;
        if (!cg_->gen_assignment(bin, target)) { error = cg_->error(); return false; }
        return true;
    }

    if (!cg_->gen_assignment(s->value, target)) { error = cg_->error(); return false; }
    return true;
}

bool Translator::gen_condition(Expr* cond, std::string& cond_reg, std::string& error) {
    std::string t = symtab_->alloc_temp("x");
    if (!cg_->gen_assignment(cond, t)) { error = cg_->error(); return false; }
    cond_reg = t;
    return true;
}

bool Translator::translate_if(Stmt* s, const std::string& indent, std::string& error) {
    std::string cond_reg;
    if (!gen_condition(s->cond, cond_reg, error)) return false;
    out_->push_back(indent + "if_nz " + cond_reg);
    for (Stmt* c : s->body)
        if (!translate_stmt(c, indent + "  ", error)) return false;
    if (!s->else_body.empty()) {
        out_->push_back(indent + "else");
        for (Stmt* c : s->else_body)
            if (!translate_stmt(c, indent + "  ", error)) return false;
    }
    out_->push_back(indent + "endif");
    return true;
}

bool Translator::translate_while(Stmt* s, const std::string& indent, std::string& error) {
    out_->push_back(indent + "loop");
    std::string cond_reg;
    if (!gen_condition(s->cond, cond_reg, error)) return false;
    out_->push_back(indent + "  breakc_z " + cond_reg);
    for (Stmt* c : s->body)
        if (!translate_stmt(c, indent + "  ", error)) return false;
    out_->push_back(indent + "endloop");
    return true;
}

bool Translator::translate_for(Stmt* s, const std::string& indent, std::string& error) {
    for (Stmt* init : s->for_init)
        if (!translate_stmt(init, indent, error)) return false;
    out_->push_back(indent + "loop");
    if (s->cond) {
        std::string cond_reg;
        if (!gen_condition(s->cond, cond_reg, error)) return false;
        out_->push_back(indent + "  breakc_z " + cond_reg);
    }
    for (Stmt* c : s->body)
        if (!translate_stmt(c, indent + "  ", error)) return false;
    if (s->for_step)
        if (!translate_stmt(s->for_step, indent + "  ", error)) return false;
    out_->push_back(indent + "endloop");
    return true;
}

// ---------------------------------------------------------------------------
// Custom function expansion
// ---------------------------------------------------------------------------

bool Translator::expand_function_call(Expr* call, const std::string& target, std::string& error) {
    auto it = functions_.find(call->name);
    if (it == functions_.end()) return false;
    FunctionDef& f = it->second;
    if (call->args.size() != f.params.size()) {
        error = "function " + call->name + ": argument count mismatch";
        return false;
    }

    symtab_->enter_scope();
    std::vector<std::string> arg_temps;
    bool ok = false;

    // Bind parameters
    for (size_t k = 0; k < f.params.size(); ++k) {
        FuncParam& p = f.params[k];
        Expr* arg = call->args[k];
        bool is_res = p.type_name == "Texture2D" || p.type_name == "TextureCube" ||
                      p.type_name == "Texture2DArray" || p.type_name == "SamplerState" ||
                      p.type_name == "SamplerComparisonState";
        if (is_res) {
            if (arg->kind != Expr::Kind::VarRef) { error = "resource arg must be a variable"; goto done; }
            Symbol* s = symtab_->lookup(arg->name);
            if (!s) { error = "variable not defined: " + arg->name; goto done; }
            symtab_->declare(p.name, s->reg, "xyzw", p.type, false);
            Symbol* pb = symtab_->lookup(p.name);
            bool is_samp = p.type_name == "SamplerState" || p.type_name == "SamplerComparisonState";
            if (pb) { pb->is_texture = !is_samp; pb->is_sampler = is_samp; }
            continue;
        }
        if (p.modifier == "out" || p.modifier == "inout") {
            if (arg->kind != Expr::Kind::VarRef) { error = "out param must be a variable"; goto done; }
            Symbol* s = symtab_->lookup(arg->name);
            if (!s) { error = "variable not defined: " + arg->name; goto done; }
            std::string m = arg->mask.empty() ? s->mask : arg->mask;
            symtab_->declare(p.name, s->reg, m, p.type, false);
            continue;
        }
        // in param
        Operand op;
        if (!cg_->eval(arg, op, arg_temps)) { error = cg_->error(); goto done; }
        if (op.is_immediate) {
            std::string t = symtab_->alloc_temp("xyzw");
            std::string dm = CodeGen::target_mask_of(t);
            cg_->emit("mov " + t + ", " + cg_->format_imm(op.vals, dm));
            std::string tb = t.substr(0, t.find('.'));
            symtab_->declare(p.name, tb, "xyzw", p.type, false);
        } else {
            symtab_->declare(p.name, op.reg, op.mask, p.type, false);
        }
    }

    // Translate body
    {
        std::string body_text;
        for (std::string& l : f.body_lines) {
            body_text += l;
            body_text += "\n";
        }
        body_text = strip_comments(body_text);
        std::vector<Stmt*> stmts;
        Parser parser(arena_, stmt_arena_);
        if (!parser.parse_statements(body_text, stmts)) {
            error = "function " + call->name + " body: " + parser.error();
            goto done;
        }

        bool is_void = f.return_type_name == "void";
        std::string ret_target = target;
        if (ret_target.empty() && !is_void) {
            std::string m = type_mask_for(f.return_type);
            ret_target = symtab_->alloc_temp(m);
        }
        std::string prev_ret = return_target_;
        return_target_ = ret_target;
        for (Stmt* st : stmts) {
            if (!translate_stmt(st, "", error)) {
                return_target_ = prev_ret;
                goto done;
            }
        }
        return_target_ = prev_ret;
        if (!ret_target.empty() && !is_void && target.empty()) {
            symtab_->free_temp(ret_target);
        }
    }

    ok = true;
done:
    symtab_->exit_scope();
    for (std::string& t : arg_temps) symtab_->free_temp(t);
    return ok;
}

bool translate_lines(const std::vector<std::string>& input,
                     std::vector<std::string>& out,
                     const std::string& data_dir,
                     std::string& error) {
    Translator t;
    t.set_data_dir(data_dir);
    return t.run(input, out, error);
}

} // namespace hb
