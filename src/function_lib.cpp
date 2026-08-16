// function_lib.cpp
#include "function_lib.h"

#include <cctype>
#include <vector>

namespace hb {

// Split a comma-separated parameter string into parts (respecting parens).
static std::vector<std::string> split_params(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    int depth = 0;
    for (char c : s) {
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        if (c == ',' && depth == 0) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

bool parse_function_library(const std::string& source, std::map<std::string, FunctionDef>& out) {
    size_t i = 0;
    const size_t n = source.size();
    bool any = false;

    while (i < n) {
        // Skip whitespace/comments (assumed stripped, but be safe).
        if (isspace((unsigned char)source[i])) { ++i; continue; }

        // Find a signature: word word ( params ) {
        // First token: return type name (possibly with modifiers)
        size_t start = i;
        size_t j = i;
        // Return type token
        while (j < n && (isalnum((unsigned char)source[j]) || source[j] == '_')) ++j;
        if (j == start) { ++i; continue; }
        std::string ret = source.substr(start, j - start);
        i = j;
        while (i < n && isspace((unsigned char)source[i])) ++i;

        // Function name
        start = i;
        j = i;
        while (j < n && (isalnum((unsigned char)source[j]) || source[j] == '_')) ++j;
        if (j == start) { ++i; continue; }
        std::string name = source.substr(start, j - start);
        i = j;
        while (i < n && isspace((unsigned char)source[i])) ++i;

        if (i >= n || source[i] != '(') {
            // Not a function; skip to next '(' or ';' or '{' boundary.
            while (i < n && source[i] != ';' && source[i] != '{' && source[i] != '}') ++i;
            ++i;
            continue;
        }

        // Parse params up to matching ')'
        ++i; // skip '('
        int depth = 1;
        std::string params_str;
        while (i < n && depth > 0) {
            if (source[i] == '(') ++depth;
            else if (source[i] == ')') { --depth; if (depth == 0) break; }
            params_str.push_back(source[i]);
            ++i;
        }
        ++i; // skip ')'
        while (i < n && isspace((unsigned char)source[i])) ++i;

        // Expect '{'
        if (i >= n || source[i] != '{') {
            while (i < n && source[i] != ';' && source[i] != '}') ++i;
            ++i;
            continue;
        }
        ++i; // skip '{'
        depth = 1;
        std::string body;
        while (i < n && depth > 0) {
            if (source[i] == '{') ++depth;
            else if (source[i] == '}') { --depth; if (depth == 0) break; }
            body.push_back(source[i]);
            ++i;
        }
        ++i; // skip '}'

        FunctionDef def;
        def.return_type_name = ret;
        Type rt;
        def.return_type = parse_type_name(ret, rt) ? rt : make_scalar(BaseType::Float);

        for (std::string& part : split_params(params_str)) {
            // strip whitespace
            size_t a = part.find_first_not_of(" \t");
            if (a == std::string::npos) continue;
            size_t b = part.find_last_not_of(" \t");
            part = part.substr(a, b - a + 1);

            // optional in/out/inout modifier
            std::string modifier = "in";
            size_t sp = part.find_first_of(" \t");
            std::string first = sp == std::string::npos ? part : part.substr(0, sp);
            if (first == "in" || first == "out" || first == "inout") {
                modifier = first;
                part = part.substr(sp + 1);
                a = part.find_first_not_of(" \t");
                if (a == std::string::npos) continue;
                b = part.find_last_not_of(" \t");
                part = part.substr(a, b - a + 1);
            }
            sp = part.find_first_of(" \t");
            if (sp == std::string::npos) continue;
            std::string ptype = part.substr(0, sp);
            std::string pname = part.substr(sp + 1);
            a = pname.find_first_not_of(" \t");
            if (a == std::string::npos) continue;
            b = pname.find_last_not_of(" \t");
            pname = pname.substr(a, b - a + 1);

            FuncParam p;
            p.type_name = ptype;
            p.name = pname;
            p.modifier = modifier;
            Type pt;
            p.type = parse_type_name(ptype, pt) ? pt : make_scalar(BaseType::Float);
            def.params.push_back(p);
        }

        // Split body into lines (strip leading/trailing whitespace).
        std::string line;
        for (char c : body) {
            if (c == '\n') {
                size_t a = line.find_first_not_of(" \t\r");
                if (a != std::string::npos) def.body_lines.push_back(line.substr(a));
                line.clear();
            } else {
                line.push_back(c);
            }
        }
        if (!line.empty()) {
            size_t a = line.find_first_not_of(" \t\r");
            if (a != std::string::npos) def.body_lines.push_back(line.substr(a));
        }

        out[name] = def;
        any = true;
        i = (i <= n) ? i : n;
    }
    return any;
}

} // namespace hb
