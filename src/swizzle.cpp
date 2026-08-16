// swizzle.cpp - swizzle/writemask utilities (vkd3d hlsl model)
#include "swizzle.h"

#include <cassert>

#include "types.h"

namespace hb {

std::string swizzle_to_string(Swizzle sw) {
    char buf[4];
    for (int i = 0; i < 4; ++i)
        buf[i] = "xyzw"[swizzle_get_component(sw, i)];
    return std::string(buf, 4);
}

Swizzle swizzle_from_string(const std::string& s) {
    Swizzle sw = 0;
    int n = (int)s.size();
    if (n <= 0)
        return SWIZZLE_XYZW;
    // Pad unused components by replicating the FIRST valid component, matching
    // the convention used by Microsoft's compiler (fxc/D3DCompile) and seen in
    // 3Dmigoto disassembly: .xy -> .xyxx, .xyz -> .xyzx, .zw -> .zwzz.
    for (int i = 0; i < 4; ++i) {
        int idx = (i < n) ? mask_index_of(s[i]) : mask_index_of(s[0]);
        if (idx < 0) idx = 0;
        swizzle_set_component(sw, i, (uint8_t)idx);
    }
    return sw;
}

Swizzle swizzle_from_writemask(uint32_t writemask) {
    static const Swizzle table[16] = {
        0,
        HLSL_SWIZZLE(0, 0, 0, 0), // x
        HLSL_SWIZZLE(1, 1, 1, 1), // y
        HLSL_SWIZZLE(0, 1, 0, 0), // xy
        HLSL_SWIZZLE(2, 2, 2, 2), // z
        HLSL_SWIZZLE(0, 2, 0, 0), // xz
        HLSL_SWIZZLE(1, 2, 0, 0), // yz
        HLSL_SWIZZLE(0, 1, 2, 0), // xyz
        HLSL_SWIZZLE(3, 3, 3, 3), // w
        HLSL_SWIZZLE(0, 3, 0, 0), // xw
        HLSL_SWIZZLE(1, 3, 0, 0), // yw
        HLSL_SWIZZLE(0, 1, 3, 0), // xyw
        HLSL_SWIZZLE(2, 3, 0, 0), // zw
        HLSL_SWIZZLE(0, 2, 3, 0), // xzw
        HLSL_SWIZZLE(1, 2, 3, 0), // yzw
        HLSL_SWIZZLE(0, 1, 2, 3), // xyzw
    };
    return table[writemask & 0xf];
}

Swizzle map_swizzle(Swizzle swizzle, uint32_t writemask) {
    // Leave replicate swizzles alone; some instructions need them.
    if (swizzle == SWIZZLE_XXXX || swizzle == SWIZZLE_YYYY ||
        swizzle == SWIZZLE_ZZZZ || swizzle == SWIZZLE_WWWW)
        return swizzle;

    Swizzle ret = 0;
    uint32_t src_component = 0;
    for (uint32_t dst_component = 0; dst_component < 4; ++dst_component) {
        if (writemask & (1u << dst_component)) {
            uint8_t c = swizzle_get_component(swizzle, (int)src_component++);
            swizzle_set_component(ret, (int)dst_component, c);
        }
    }
    return ret;
}

Swizzle compute_src_swizzle(const std::string& src_mask, const std::string& dst_mask) {
    // Two-step: (1) build the source's compact 4-component swizzle by padding
    // with the last component (HLSL replicate-last semantics for scalars and
    // short masks), (2) map it through the destination writemask so components
    // land on the correct dest positions (handles non-contiguous writemasks
    // like ".xw").
    Swizzle compact = swizzle_from_string(src_mask);
    return map_swizzle(compact, mask_to_bits(dst_mask));
}

bool invert_swizzle(const std::string& swizzle_str, const std::string& declared_mask,
                    std::string& out_inverse, std::string& out_legal_mask, int& out_width) {
    // Port of vkd3d invert_swizzle() in hlsl.y.
    const std::string all_comp = "xyzw";
    uint32_t writemask_bits = mask_to_bits(declared_mask);
    int swiz_len = (int)swizzle_str.size();

    uint32_t new_writemask = 0;
    std::string new_swiz;
    new_swiz.reserve(4);

    for (int i = 0; i < 4; ++i) {
        if (writemask_bits & (1u << i)) {
            char s = (i < swiz_len) ? swizzle_str[i] : all_comp[i];
            int s_bit = 1 << mask_index_of(s);
            if (s_bit < 0)
                return false;
            if (new_writemask & (uint32_t)s_bit)
                return false; // duplicate component
            new_writemask |= (uint32_t)s_bit;
            new_swiz.push_back(s);
        }
    }

    int width = (int)new_swiz.size();
    out_legal_mask.clear();
    for (int i = 0; i < 4; ++i)
        if (new_writemask & (1u << i))
            out_legal_mask.push_back(all_comp[i]);

    // Phase 2: invert — for each component value, find its position.
    std::string inverted;
    inverted.reserve(4);
    for (int i = 0; i < 4; ++i) {
        char ch = all_comp[i];
        for (int j = 0; j < width; ++j) {
            if (new_swiz[j] == ch) {
                inverted.push_back(all_comp[j]);
                break;
            }
        }
    }
    if ((int)inverted.size() < width)
        inverted.resize(width, 'x');

    out_inverse = inverted.substr(0, (size_t)width);
    out_width = width;
    return true;
}

} // namespace hb
