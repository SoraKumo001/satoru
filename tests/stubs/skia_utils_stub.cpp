// Stub implementation of skia_utils functions needed by font_manager.cpp
#include <core/SkRRect.h>
#include <core/SkRefCnt.h>
#include <litehtml.h>
#include <string>
#include <vector>

// Forward declare functions from skia_utils.h
std::string clean_font_name(const char* name);

std::string clean_font_name(const char* name) {
    if (!name) return "";
    std::string result;
    for (const char* p = name; *p; ++p) {
        char c = *p;
        if (c == '\'' || c == '\"') continue;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        result += (char)std::tolower((unsigned char)c);
    }
    return result;
}

std::string base64_encode(const uint8_t* data, size_t len) {
    // Stub — not needed for font_manager tests
    return "";
}

std::vector<uint8_t> base64_decode(const std::string& in) {
    return {};
}

std::string url_decode(const std::string& in) {
    return in;
}

SkRRect make_rrect(const litehtml::position& pos, const litehtml::border_radiuses& radius) {
    return SkRRect();
}

// ── container_skia stubs needed by test_container_layer.cpp ────────────────
#include "container_skia.h"

SkBlendMode container_skia::to_skia_blend_mode(litehtml::blend_mode bm) {
    // Stub mapping — real implementation in container_skia.cpp
    switch (bm) {
        case litehtml::blend_mode::blend_mode_normal:  return SkBlendMode::kSrcOver;
        case litehtml::blend_mode::blend_mode_multiply: return SkBlendMode::kMultiply;
        case litehtml::blend_mode::blend_mode_screen:   return SkBlendMode::kScreen;
        case litehtml::blend_mode::blend_mode_overlay:  return SkBlendMode::kOverlay;
        case litehtml::blend_mode::blend_mode_darken:   return SkBlendMode::kDarken;
        case litehtml::blend_mode::blend_mode_lighten:  return SkBlendMode::kLighten;
        case litehtml::blend_mode::blend_mode_color_dodge: return SkBlendMode::kColorDodge;
        case litehtml::blend_mode::blend_mode_color_burn:  return SkBlendMode::kColorBurn;
        case litehtml::blend_mode::blend_mode_hard_light:  return SkBlendMode::kHardLight;
        case litehtml::blend_mode::blend_mode_soft_light:  return SkBlendMode::kSoftLight;
        case litehtml::blend_mode::blend_mode_difference:  return SkBlendMode::kDifference;
        case litehtml::blend_mode::blend_mode_exclusion:   return SkBlendMode::kExclusion;
        case litehtml::blend_mode::blend_mode_hue:         return SkBlendMode::kHue;
        case litehtml::blend_mode::blend_mode_saturation:  return SkBlendMode::kSaturation;
        case litehtml::blend_mode::blend_mode_color:       return SkBlendMode::kColor;
        case litehtml::blend_mode::blend_mode_luminosity:  return SkBlendMode::kLuminosity;
        default: return SkBlendMode::kSrcOver;
    }
}
