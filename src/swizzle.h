// swizzle.h - swizzle/writemask utilities (vkd3d hlsl model)
#pragma once

#include <cstdint>
#include <string>

namespace hb {

// Swizzle is packed as 4 components of 2 bits each (vkd3d HLSL_SWIZZLE).
using Swizzle = uint32_t;

constexpr uint8_t SWIZZLE_X = 0;
constexpr uint8_t SWIZZLE_Y = 1;
constexpr uint8_t SWIZZLE_Z = 2;
constexpr uint8_t SWIZZLE_W = 3;

inline constexpr Swizzle HLSL_SWIZZLE(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)(a) | ((uint32_t)b << 2) | ((uint32_t)c << 4) | ((uint32_t)d << 6);
}
inline uint8_t swizzle_get_component(Swizzle sw, int idx) {
    return (uint8_t)((sw >> (2 * idx)) & 0x3);
}
inline void swizzle_set_component(Swizzle& sw, int idx, uint8_t comp) {
    sw &= ~(0x3u << (2 * idx));
    sw |= ((uint32_t)comp & 0x3) << (2 * idx);
}

constexpr Swizzle SWIZZLE_XXXX = HLSL_SWIZZLE(0, 0, 0, 0);
constexpr Swizzle SWIZZLE_YYYY = HLSL_SWIZZLE(1, 1, 1, 1);
constexpr Swizzle SWIZZLE_ZZZZ = HLSL_SWIZZLE(2, 2, 2, 2);
constexpr Swizzle SWIZZLE_WWWW = HLSL_SWIZZLE(3, 3, 3, 3);
constexpr Swizzle SWIZZLE_XYZW = HLSL_SWIZZLE(0, 1, 2, 3);

// Swizzle -> "xyzw" string (always 4 chars).
std::string swizzle_to_string(Swizzle sw);
// Parse a user-written swizzle (1-4 chars, e.g. "xyz", "wzy") into a 4-component
// swizzle. Components beyond the given length are padded by repeating the last
// component (matches vkd3d padding semantics; the padding is unused when dest
// writemask is narrower).
Swizzle swizzle_from_string(const std::string& s);

// vkd3d hlsl_swizzle_from_writemask: writemask bitmask (0..0xF) -> swizzle.
// e.g. wm=0x3 (.xy) -> XYXX
Swizzle swizzle_from_writemask(uint32_t writemask);

// vkd3d hlsl_map_swizzle: map a compact swizzle through a dst writemask.
// Replicate swizzles (xxxx/yyyy/zzzz/wwww) are returned unchanged.
Swizzle map_swizzle(Swizzle swizzle, uint32_t writemask);

// Compute the final 4-component source swizzle for a source operand with the
// given source writemask (subset string) targeted at a dst writemask.
Swizzle compute_src_swizzle(const std::string& src_mask, const std::string& dst_mask);

// vkd3d invert_swizzle: given a user LHS swizzle string (which may be
// non-ascending, e.g. "wx") and the variable's full declared mask, produce
// (inverse_swizzle, legal_ascending_writemask, width). Returns false on
// duplicate components.
bool invert_swizzle(const std::string& swizzle_str, const std::string& declared_mask,
                    std::string& out_inverse, std::string& out_legal_mask, int& out_width);

} // namespace hb
