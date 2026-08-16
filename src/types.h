// types.h - HLSL type system (DXC/vkd3d style)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hb {

// Base scalar type
enum class BaseType : uint8_t {
    Float,
    Int,
    Uint,
    Bool,
    Double,
    Void,
};

inline const char* base_type_name(BaseType b) {
    switch (b) {
    case BaseType::Float: return "float";
    case BaseType::Int: return "int";
    case BaseType::Uint: return "uint";
    case BaseType::Bool: return "bool";
    case BaseType::Double: return "double";
    case BaseType::Void: return "void";
    }
    return "?";
}

enum class TypeClass : uint8_t {
    Scalar,  // rows=1, cols=1
    Vector,  // rows=1, cols=N
    Matrix,  // rows=M, cols=N
};

// Numeric type model: rows x cols of a base scalar type.
struct Type {
    TypeClass klass = TypeClass::Scalar;
    uint8_t rows = 1;
    uint8_t cols = 1;      // vectors: cols = component count
    BaseType base = BaseType::Float;

    bool is_scalar() const { return klass == TypeClass::Scalar; }
    bool is_vector() const { return klass == TypeClass::Vector; }
    bool is_matrix() const { return klass == TypeClass::Matrix; }
    bool is_float() const { return base == BaseType::Float; }
    bool is_int() const { return base == BaseType::Int; }
    bool is_uint() const { return base == BaseType::Uint; }
    bool is_bool() const { return base == BaseType::Bool; }
    bool is_numeric() const { return base == BaseType::Float || base == BaseType::Int || base == BaseType::Uint; }
    // Number of components (scalar = 1, vector = cols, matrix = rows*cols)
    int component_count() const {
        switch (klass) {
        case TypeClass::Scalar: return 1;
        case TypeClass::Vector: return cols;
        case TypeClass::Matrix: return rows * cols;
        }
        return 1;
    }
    std::string to_string() const {
        std::string s = base_type_name(base);
        if (klass == TypeClass::Vector && cols > 1)
            s += std::to_string(cols);
        else if (klass == TypeClass::Matrix)
            s += std::to_string(rows) + "x" + std::to_string(cols);
        return s;
    }
};

inline Type make_scalar(BaseType b) {
    Type t;
    t.klass = TypeClass::Scalar;
    t.rows = t.cols = 1;
    t.base = b;
    return t;
}
inline Type make_vector(int cols, BaseType b = BaseType::Float) {
    Type t;
    t.klass = TypeClass::Vector;
    t.rows = 1;
    t.cols = (uint8_t)cols;
    t.base = b;
    return t;
}

// Parse a type name like "float", "float3", "uint2" -> Type, or nullopt.
bool parse_type_name(const std::string& name, Type& out);

// Component masks. "xyzw" ordering, index 0..3.
inline constexpr char kCompChar[4] = {'x', 'y', 'z', 'w'};

// Convenience helpers on component masks.
// Note: a mask is a subset string like "xy", "xyz", "xw", "zw".
int mask_width(const std::string& mask);                  // number of chars
bool is_legal_mask(const std::string& mask);              // strictly ascending components
int mask_index_of(char c);                                // x->0 ... w->3, -1 if invalid

// Component set as bitmask (bit i set if component i in mask). 0..0xF.
uint32_t mask_to_bits(const std::string& mask);
std::string bits_to_mask(uint32_t bits);

} // namespace hb
