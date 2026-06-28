#include "container_skia_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "include/core/SkColor.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkImageFilter.h"
#include "include/effects/SkColorMatrix.h"
#include "include/effects/SkImageFilters.h"
#include "libs/litehtml/include/litehtml/css_length.h"
#include "libs/litehtml/include/litehtml/html.h"

// ============================================================================
// Internal helpers (not exported)
// ============================================================================
namespace {

/// Split a CSS token vector by comma tokens (same logic as
/// litehtml::parse_comma_separated_list from css_parser.cpp).
static std::vector<litehtml::css_token_vector> split_by_comma(
    const litehtml::css_token_vector& tokens) {
    std::vector<litehtml::css_token_vector> result;
    litehtml::css_token_vector list;
    for (auto& tok : tokens) {
        if (tok.type == ',') {
            result.push_back(list);
            list.clear();
            continue;
        }
        list.push_back(tok);
    }
    result.push_back(list);
    return result;
}

#ifndef SK_ScalarPI
#define SK_ScalarPI 3.14159265f
#endif

}  // anonymous namespace

// ============================================================================
// 1. Blend mode conversion
// ============================================================================

SkBlendMode to_skia_blend_mode(litehtml::blend_mode bm) {
    switch (bm) {
        case litehtml::blend_mode_multiply:
            return SkBlendMode::kMultiply;
        case litehtml::blend_mode_screen:
            return SkBlendMode::kScreen;
        case litehtml::blend_mode_overlay:
            return SkBlendMode::kOverlay;
        case litehtml::blend_mode_darken:
            return SkBlendMode::kDarken;
        case litehtml::blend_mode_lighten:
            return SkBlendMode::kLighten;
        case litehtml::blend_mode_color_dodge:
            return SkBlendMode::kColorDodge;
        case litehtml::blend_mode_color_burn:
            return SkBlendMode::kColorBurn;
        case litehtml::blend_mode_hard_light:
            return SkBlendMode::kHardLight;
        case litehtml::blend_mode_soft_light:
            return SkBlendMode::kSoftLight;
        case litehtml::blend_mode_difference:
            return SkBlendMode::kDifference;
        case litehtml::blend_mode_exclusion:
            return SkBlendMode::kExclusion;
        case litehtml::blend_mode_hue:
            return SkBlendMode::kHue;
        case litehtml::blend_mode_saturation:
            return SkBlendMode::kSaturation;
        case litehtml::blend_mode_color:
            return SkBlendMode::kColor;
        case litehtml::blend_mode_luminosity:
            return SkBlendMode::kLuminosity;
        default:
            return SkBlendMode::kSrcOver;
    }
}

// ============================================================================
// 2. Border uniformity check
// ============================================================================

bool is_uniform_border(const litehtml::borders& borders) {
    return borders.top.width == borders.bottom.width && borders.top.width == borders.left.width &&
           borders.top.width == borders.right.width && borders.top.color == borders.bottom.color &&
           borders.top.color == borders.left.color && borders.top.color == borders.right.color &&
           borders.top.style == borders.bottom.style && borders.top.style == borders.left.style &&
           borders.top.style == borders.right.style &&
           borders.top.style != litehtml::border_style_groove &&
           borders.top.style != litehtml::border_style_ridge &&
           borders.top.style != litehtml::border_style_inset &&
           borders.top.style != litehtml::border_style_outset;
}

// ============================================================================
// 3. CSS custom-property helpers
// ============================================================================

std::string extract_var_name(const std::string& color) {
    if (color.empty()) return "";

    std::string name = color;
    if (name.size() > 4 && name.substr(0, 4) == "var(") {
        size_t pos = name.find('(');
        size_t end = name.find_last_of(')');
        if (pos != std::string::npos && end != std::string::npos && end > pos + 1) {
            name = name.substr(pos + 1, end - pos - 1);
            // Handle fallback if present (comma)
            size_t comma = name.find(',');
            if (comma != std::string::npos) {
                name = name.substr(0, comma);
            }
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t\r\n"));
            size_t last = name.find_last_not_of(" \t\r\n");
            if (last != std::string::npos) {
                name.erase(last + 1);
            }
        }
    }

    if (name.size() > 2 && name[0] == '-' && name[1] == '-') {
        return name;
    }
    return "";
}

// ============================================================================
// 4. BiDi first-character classification
// ============================================================================

bool is_first_char_bidi_strong(const char* text) {
    if (!text || !*text) return false;
    const unsigned char c0 = static_cast<unsigned char>(text[0]);
    if (c0 < 0x80) {
        // ASCII neutral: controls (0x00-0x2F), :;<=>?@ (0x3A-0x40),
        // [\]^_` (0x5B-0x60), {|}~DEL (0x7B-0x7F)
        bool is_neutral = c0 <= 0x2F || (c0 >= 0x3A && c0 <= 0x40) || (c0 >= 0x5B && c0 <= 0x60) ||
                          (c0 >= 0x7B && c0 <= 0x7F);
        return !is_neutral;
    }
    const unsigned char c1 = static_cast<unsigned char>(text[1]);
    // CJK Unified Ideographs  U+4E00-U+9FFF  UTF-8: 0xE4-0xE9
    // CJK Extension A part 2  U+4000-U+4DBF  UTF-8: 0xE4
    if ((c0 >= 0xE4 && c0 <= 0xE9)) return true;
    // Hiragana (U+3040-U+309F) / Katakana (U+30A0-U+30FF)  UTF-8: 0xE3 0x81-0x83
    if (c0 == 0xE3 && c1 >= 0x81 && c1 <= 0x83) return true;
    // CJK Extension A part 1  U+3400-U+3FFF  UTF-8: 0xE3 0x90-0xBF
    if (c0 == 0xE3 && c1 >= 0x90) return true;
    // Hangul Syllables  U+AC00-U+D7A3  UTF-8: 0xEA-0xED
    if (c0 >= 0xEA && c0 <= 0xED) return true;
    return false;
}

// ============================================================================
// 5. CSS transform matrix computation
// ============================================================================

SkMatrix compute_transform_matrix(const std::string& name, const std::vector<float>& vals) {
    SkMatrix m;  // default-constructs as identity

    if (name == "matrix" && vals.size() >= 6) {
        m.setAll(vals[0], vals[2], vals[4], vals[1], vals[3], vals[5], 0, 0, 1);
    } else if (name == "translate" || name == "translate3d") {
        m.setTranslate(vals.size() > 0 ? vals[0] : 0, vals.size() > 1 ? vals[1] : 0);
    } else if (name == "translatex") {
        m.setTranslate(vals.size() > 0 ? vals[0] : 0, 0);
    } else if (name == "translatey") {
        m.setTranslate(0, vals.size() > 0 ? vals[0] : 0);
    } else if (name == "scale" || name == "scale3d") {
        float sx = vals.size() > 0 ? vals[0] : 1;
        float sy = vals.size() > 1 ? vals[1] : sx;
        m.setScaleTranslate(sx, sy, 0, 0);
    } else if (name == "scalex") {
        m.setScaleTranslate(vals.size() > 0 ? vals[0] : 1, 1, 0, 0);
    } else if (name == "scaley") {
        m.setScaleTranslate(1, vals.size() > 0 ? vals[0] : 1, 0, 0);
    } else if (name == "rotate" || name == "rotatez") {
        if (!vals.empty()) {
            float rad = vals[0] * SK_ScalarPI / 180.0f;
            float c = cosf(rad);
            float s = sinf(rad);
            m.setAll(c, -s, 0, s, c, 0, 0, 0, 1);
        }
    } else if (name == "skew" && !vals.empty()) {
        float kx = tanf(vals[0] * SK_ScalarPI / 180.0f);
        float ky = vals.size() > 1 ? tanf(vals[1] * SK_ScalarPI / 180.0f) : 0;
        m.setAll(1, kx, 0, ky, 1, 0, 0, 0, 1);
    } else if (name == "skewx" && !vals.empty()) {
        float kx = tanf(vals[0] * SK_ScalarPI / 180.0f);
        m.setAll(1, kx, 0, 0, 1, 0, 0, 0, 1);
    } else if (name == "skewy" && !vals.empty()) {
        float ky = tanf(vals[0] * SK_ScalarPI / 180.0f);
        m.setAll(1, 0, 0, ky, 1, 0, 0, 0, 1);
    }

    return m;
}

// ============================================================================
// 6. CSS filter-chain builders
// ============================================================================

sk_sp<SkImageFilter> build_backdrop_filter_chain(const litehtml::css_token_vector& filter) {
    sk_sp<SkImageFilter> last_filter = nullptr;

    for (const auto& tok : filter) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            auto args = split_by_comma(tok.value);
            if (name == "blur") {
                if (!args.empty() && !args[0].empty()) {
                    litehtml::css_length len;
                    len.from_token(args[0][0], litehtml::f_length | litehtml::f_positive);
                    float sigma = len.val();
                    if (sigma > 0) {
                        last_filter = SkImageFilters::Blur(sigma, sigma, last_filter);
                    }
                }
            } else if (name == "brightness") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    float mat[20] = {amount, 0, 0,      0, 0, 0, amount, 0, 0, 0,
                                     0,      0, amount, 0, 0, 0, 0,      0, 1, 0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "contrast") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    float intercept = -(0.5f * amount) + 0.5f;
                    SkColorMatrix cm;
                    float mat[20] = {amount, 0, 0,      0, intercept, 0, amount, 0, 0, intercept,
                                     0,      0, amount, 0, intercept, 0, 0,      0, 1, 0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "grayscale") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 0.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    cm.setSaturation(1.0f - amount);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "sepia") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 0.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    float mat[20] = {0.393f + 0.607f * (1.0f - amount),
                                     0.769f - 0.769f * (1.0f - amount),
                                     0.189f - 0.189f * (1.0f - amount),
                                     0,
                                     0,
                                     0.349f - 0.349f * (1.0f - amount),
                                     0.686f + 0.314f * (1.0f - amount),
                                     0.168f - 0.168f * (1.0f - amount),
                                     0,
                                     0,
                                     0.272f - 0.272f * (1.0f - amount),
                                     0.534f - 0.534f * (1.0f - amount),
                                     0.131f + 0.869f * (1.0f - amount),
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     1,
                                     0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "saturate") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    cm.setSaturation(amount);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "hue-rotate") {
                if (!args.empty() && !args[0].empty()) {
                    float angle_deg = 0.0f;
                    if (args[0][0].type == litehtml::DIMENSION) {
                        angle_deg = args[0][0].n.number;
                        const std::string& unit = args[0][0].str;
                        if (unit == "rad") {
                            angle_deg = angle_deg * 180.0f / SK_ScalarPI;
                        } else if (unit == "turn") {
                            angle_deg = angle_deg * 360.0f;
                        } else if (unit == "grad") {
                            angle_deg = angle_deg * 0.9f;
                        }
                    }
                    float c = cosf(angle_deg * SK_ScalarPI / 180.0f);
                    float s = sinf(angle_deg * SK_ScalarPI / 180.0f);
                    SkColorMatrix cm;
                    float mat[20] = {0.213f + c * 0.787f - s * 0.213f,
                                     0.715f - c * 0.715f - s * 0.715f,
                                     0.072f - c * 0.072f + s * 0.928f,
                                     0,
                                     0,
                                     0.213f - c * 0.213f + s * 0.143f,
                                     0.715f + c * 0.285f + s * 0.140f,
                                     0.072f - c * 0.072f - s * 0.283f,
                                     0,
                                     0,
                                     0.213f - c * 0.213f - s * 0.787f,
                                     0.715f - c * 0.715f + s * 0.715f,
                                     0.072f + c * 0.928f + s * 0.072f,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     1,
                                     0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "invert") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 0.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    float mat[20] = {1.0f - 2.0f * amount,
                                     0,
                                     0,
                                     0,
                                     amount,
                                     0,
                                     1.0f - 2.0f * amount,
                                     0,
                                     0,
                                     amount,
                                     0,
                                     0,
                                     1.0f - 2.0f * amount,
                                     0,
                                     amount,
                                     0,
                                     0,
                                     0,
                                     1,
                                     0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "opacity") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    last_filter = SkImageFilters::ColorFilter(
                        SkColorFilters::Blend(
                            SkColorSetARGB((uint8_t)(amount * 255), 255, 255, 255),
                            SkBlendMode::kDstIn),
                        last_filter);
                }
            }
        }
    }

    return last_filter;
}

sk_sp<SkImageFilter> build_filter_chain(const litehtml::css_token_vector& filter) {
    sk_sp<SkImageFilter> last_filter = nullptr;

    for (const auto& filter_tok : filter) {
        if (filter_tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(filter_tok.name);
            auto args = split_by_comma(filter_tok.value);

            if (name == "blur") {
                if (!args.empty() && !args[0].empty()) {
                    litehtml::css_length len;
                    len.from_token(args[0][0], litehtml::f_length | litehtml::f_positive);
                    float sigma = len.val();
                    if (sigma > 0) {
                        last_filter = SkImageFilters::Blur(sigma, sigma, last_filter);
                    }
                }
            } else if (name == "drop-shadow") {
                if (!args.empty()) {
                    float dx = 0, dy = 0, blur = 0;
                    litehtml::web_color color(0, 0, 0);  // default black
                    int i = 0;
                    for (const auto& t : args[0]) {
                        if (t.type == litehtml::WHITESPACE) continue;
                        if (i == 0) {
                            litehtml::css_length l;
                            l.from_token(t, litehtml::f_length);
                            dx = l.val();
                        } else if (i == 1) {
                            litehtml::css_length l;
                            l.from_token(t, litehtml::f_length);
                            dy = l.val();
                        } else if (i == 2) {
                            litehtml::css_length l;
                            l.from_token(t, litehtml::f_length | litehtml::f_positive);
                            blur = l.val();
                        } else if (i == 3) {
                            litehtml::parse_color(t, color, nullptr);
                        }
                        i++;
                    }
                    if (dx != 0 || dy != 0 || blur > 0) {
                        last_filter = SkImageFilters::DropShadow(
                            dx, dy, blur * 0.5f, blur * 0.5f,
                            SkColorSetARGB(color.alpha, color.red, color.green, color.blue),
                            last_filter);
                    }
                }
            }
        }
    }

    return last_filter;
}
