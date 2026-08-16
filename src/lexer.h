// lexer.h - HLSL tokenizer for the blend translator
#pragma once

#include <string>
#include <vector>

namespace hb {

enum class TokKind {
    Ident,
    Number,
    Punct,
    End,
};

struct Token {
    TokKind kind = TokKind::End;
    std::string text;
    double num = 0.0;      // Number value
    bool is_int = false;   // Number was an integer literal
    size_t pos = 0;        // byte offset in source
};

// Tokenize a single expression/statement string. Operators are returned as
// individual Punct tokens (multi-char operators preserved: "==", "!=", "<=",
// ">=", "+=", "-=", "*=", "/=", "&&", "||", ">>", "<<").
std::vector<Token> tokenize(const std::string& text);

// Helpers
bool is_ident_start(char c);
bool is_ident_char(char c);

} // namespace hb
