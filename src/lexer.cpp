// lexer.cpp
#include "lexer.h"

#include <cctype>

namespace hb {

bool is_ident_start(char c) {
    return std::isalpha((unsigned char)c) || c == '_';
}
bool is_ident_char(char c) {
    return std::isalnum((unsigned char)c) || c == '_';
}

std::vector<Token> tokenize(const std::string& text) {
    std::vector<Token> toks;
    size_t i = 0;
    const size_t n = text.size();
    auto push = [&](TokKind k, const std::string& t, size_t pos) {
        Token tok;
        tok.kind = k;
        tok.text = t;
        tok.pos = pos;
        toks.push_back(tok);
    };

    while (i < n) {
        char c = text[i];
        if (isspace((unsigned char)c)) { ++i; continue; }

        // Comments (should normally be stripped before tokenizing, but be safe)
        if (c == '/' && i + 1 < n && text[i + 1] == '/') {
            while (i < n && text[i] != '\n') ++i;
            continue;
        }
        if (c == '/' && i + 1 < n && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(text[i] == '*' && text[i + 1] == '/')) ++i;
            i += 2;
            continue;
        }

        // Identifier
        if (is_ident_start(c)) {
            size_t start = i;
            while (i < n && is_ident_char(text[i])) ++i;
            push(TokKind::Ident, text.substr(start, i - start), start);
            continue;
        }

        // Number (int or float, optional sign handled by parser unary)
        if (isdigit((unsigned char)c)) {
            size_t start = i;
            bool is_float = false;
            bool is_hex = false;
            if (c == '0' && i + 1 < n && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
                is_hex = true;
                i += 2;
                while (i < n && isxdigit((unsigned char)text[i])) ++i;
            } else {
                while (i < n && isdigit((unsigned char)text[i])) ++i;
                if (i < n && text[i] == '.') { is_float = true; ++i; while (i < n && isdigit((unsigned char)text[i])) ++i; }
                if (i < n && (text[i] == 'e' || text[i] == 'E')) {
                    is_float = true;
                    ++i;
                    if (i < n && (text[i] == '+' || text[i] == '-')) ++i;
                    while (i < n && isdigit((unsigned char)text[i])) ++i;
                }
            }
            std::string tok_str = text.substr(start, i - start);
            Token tok;
            tok.kind = TokKind::Number;
            tok.text = tok_str;
            tok.pos = start;
            tok.is_int = !is_float && !is_hex;
            if (is_hex) {
                tok.num = (double)strtoull(tok_str.c_str() + 2, nullptr, 16);
            } else {
                tok.num = strtod(tok_str.c_str(), nullptr);
            }
            toks.push_back(tok);
            continue;
        }

        // Punctuation / operators (longest match)
        static const char* multi[] = {"<<=", ">>=", "&&=", "||=", "==", "!=", "<=", ">=",
                                       "+=", "-=", "*=", "/=", "&&", "||", "<<", ">>", "++", "--"};
        bool matched = false;
        for (const char* op : multi) {
            size_t len = strlen(op);
            if (i + len <= n && text.compare(i, len, op) == 0) {
                push(TokKind::Punct, op, i);
                i += len;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        // Single-char punctuation
        if (std::string("+-*/%()[]{}.,;:?<>=!&|~^").find(c) != std::string::npos) {
            push(TokKind::Punct, std::string(1, c), i);
            ++i;
            continue;
        }

        // Unknown char: skip (shouldn't happen for valid input)
        ++i;
    }

    Token end;
    end.kind = TokKind::End;
    end.pos = n;
    toks.push_back(end);
    return toks;
}

} // namespace hb
