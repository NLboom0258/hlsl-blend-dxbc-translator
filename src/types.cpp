// types.cpp
#include "types.h"

namespace hb {

bool parse_type_name(const std::string& name, Type& out) {
    // Match "float", "float2..4", "int", "uint", "bool", "double", "half", "halfN", "min16floatN"
    // Also accept matrix forms "float3x4", "float4x4".
    BaseType base;
    const char* p = name.c_str();
    if (name.rfind("float", 0) == 0) base = BaseType::Float;
    else if (name.rfind("int", 0) == 0) base = BaseType::Int;
    else if (name.rfind("uint", 0) == 0) base = BaseType::Uint;
    else if (name.rfind("bool", 0) == 0) base = BaseType::Bool;
    else if (name.rfind("double", 0) == 0) base = BaseType::Double;
    else if (name.rfind("half", 0) == 0) base = BaseType::Float; // half -> float in DXBC
    else if (name.rfind("min16float", 0) == 0) base = BaseType::Float;
    else if (name.rfind("min16int", 0) == 0) base = BaseType::Int;
    else if (name.rfind("min16uint", 0) == 0) base = BaseType::Uint;
    else return false;

    // Skip the base prefix to find the trailing numbers.
    size_t start = 0;
    if (name.rfind("min16", 0) == 0) start = 5;
    else {
        // find where the numeric suffix starts: after the alpha base token
        // base tokens: float int uint bool double half
        for (size_t i = 0; i < name.size(); ++i) {
            if (name[i] >= '0' && name[i] <= '9') { start = i; break; }
        }
        if (start == 0) {
            // pure scalar like "float", "int"
            out = make_scalar(base);
            return true;
        }
    }

    std::string num_part = name.substr(start);
    // forms: "3", "4", "3x4"
    auto xpos = num_part.find('x');
    if (xpos == std::string::npos) {
        int cols = std::atoi(num_part.c_str());
        if (cols < 1 || cols > 4) return false;
        out = make_vector(cols, base);
        return true;
    }
    int rows = std::atoi(num_part.substr(0, xpos).c_str());
    int cols = std::atoi(num_part.substr(xpos + 1).c_str());
    if (rows < 1 || rows > 4 || cols < 1 || cols > 4) return false;
    out.klass = TypeClass::Matrix;
    out.rows = (uint8_t)rows;
    out.cols = (uint8_t)cols;
    out.base = base;
    return true;
}

int mask_width(const std::string& mask) { return (int)mask.size(); }

bool is_legal_mask(const std::string& mask) {
    int last = -1;
    for (char c : mask) {
        int idx = mask_index_of(c);
        if (idx <= last) return false;
        last = idx;
    }
    return true;
}

int mask_index_of(char c) {
    switch (c) {
    case 'x': return 0;
    case 'y': return 1;
    case 'z': return 2;
    case 'w': return 3;
    default: return -1;
    }
}

uint32_t mask_to_bits(const std::string& mask) {
    uint32_t bits = 0;
    for (char c : mask) {
        int idx = mask_index_of(c);
        if (idx >= 0) bits |= (1u << idx);
    }
    return bits;
}

std::string bits_to_mask(uint32_t bits) {
    std::string s;
    for (int i = 0; i < 4; ++i)
        if (bits & (1u << i)) s.push_back("xyzw"[i]);
    return s;
}

} // namespace hb
