#pragma once
// Minimal SkColor stub for native tests (no Skia dependency)
#include <cstdint>

using SkColor = uint32_t;

inline uint8_t SkColorGetA(SkColor c) { return (c >> 24) & 0xFF; }
inline uint8_t SkColorGetR(SkColor c) { return (c >> 16) & 0xFF; }
inline uint8_t SkColorGetG(SkColor c) { return (c >> 8) & 0xFF; }
inline uint8_t SkColorGetB(SkColor c) { return c & 0xFF; }

inline SkColor SkColorSetARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
