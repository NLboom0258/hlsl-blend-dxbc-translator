// main.cpp - CLI entry point
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "translator.h"

static std::string read_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return std::string();
    std::string out;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    // Strip UTF-8 BOM
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF &&
        (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF)
        out = out.substr(3);
    return out;
}

static bool write_file(const std::string& path, const std::string& content) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

static void split_lines(const std::string& text, std::vector<std::string>& out) {
    std::string cur;
    for (char c : text) {
        if (c == '\n') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
}

static const char* arg_value(int argc, char* argv[], const char* short_flag, const char* long_flag) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (!strcmp(argv[i], short_flag) || !strcmp(argv[i], long_flag))
            return argv[i + 1];
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    const char* input_path = arg_value(argc, argv, "-i", "-input");
    const char* output_path = arg_value(argc, argv, "-o", "-output");
    const char* data_dir = arg_value(argc, argv, "-d", "-data");

    if (!input_path) {
        fprintf(stderr, "HLSL Blend DXBC Translator (C++)\n"
                        "Usage: hlsl_blend_dxbc_translator -input <file> [-output <file>] [-data <dir>]\n");
        return 1;
    }

    std::string text = read_file(input_path);
    if (text.empty()) {
        fprintf(stderr, "Error: cannot read file '%s'\n", input_path);
        return 1;
    }

    std::vector<std::string> lines;
    split_lines(text, lines);

    std::vector<std::string> out;
    std::string error;
    std::string data = data_dir ? data_dir : ".";

    // Resolve data dir relative to the input file's directory so
    // HLSLFunctionImport "functions/lib.txt" works from anywhere.
    if (!output_path) {
        // Print to stdout
        if (!hb::translate_lines(lines, out, data, error)) {
            fprintf(stderr, "Error: %s\n", error.c_str());
            return 1;
        }
        for (const std::string& l : out) printf("%s\n", l.c_str());
        return 0;
    }

    if (!hb::translate_lines(lines, out, data, error)) {
        fprintf(stderr, "Error: %s\n", error.c_str());
        return 1;
    }
    std::string result;
    for (const std::string& l : out) { result += l; result += "\n"; }
    if (!write_file(output_path, result)) {
        fprintf(stderr, "Error: cannot write '%s'\n", output_path);
        return 1;
    }
    printf("Translation saved to: %s\n", output_path);
    return 0;
}
