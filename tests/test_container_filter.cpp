// Tests for container_skia::push_backdrop_filter and push_filter
// Filter chain building logic (CSS tokens → SkImageFilter chain).
//
// Isomorphic copies of the filter-building code from:
//   push_backdrop_filter: src/cpp/core/container_skia.cpp L130-363
//   push_filter:          src/cpp/core/container_skia.cpp L2102-2190
//
// Tests verify that CSS filter/backdrop-filter strings are correctly
// parsed into the expected SkImageFilter chain (blur, brightness,
// contrast, grayscale, sepia, saturate, hue-rotate, invert, opacity,
// drop-shadow).

#include <gtest/gtest.h>
#include <litehtml.h>

// Stub Skia headers (minimal, no real Skia dependency)
#include <core/SkRefCnt.h>
#include <core/SkColor.h>
#include <core/SkBlendMode.h>
#include <effects/SkColorMatrix.h>
#include <core/SkColorFilter.h>
#include <core/SkImageFilter.h>
#include <effects/SkImageFilters.h>

#include <string>
#include <vector>
#include <cmath>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================
static constexpr float kSkScalarPI = 3.14159265f;

// ============================================================================
// Minimal litehtml::parse_color stub for drop-shadow color parsing.
// Only handles basic named colors used in tests.
// ============================================================================
namespace litehtml {

bool parse_color(const css_token& token, web_color& color, document_container*) {
    if (token.type == IDENT) {
        const auto& s = token.str;
        if (s == "red")            { color = web_color(255, 0, 0); return true; }
        if (s == "green")          { color = web_color(0, 128, 0); return true; }
        if (s == "blue")           { color = web_color(0, 0, 255); return true; }
        if (s == "black")          { color = web_color(0, 0, 0); return true; }
        if (s == "white")          { color = web_color(255, 255, 255); return true; }
        if (s == "transparent")    { color = web_color(0, 0, 0, 0); return true; }
    }
    // Fallback: try hex/rgb etc. — not implemented for stub
    return false;
}

} // namespace litehtml

// ============================================================================
// CSS componentizer (raw tokens → componentized CV_FUNCTION tokens)
// Based on test_clip_path.cpp SimpleComponentizer
// ============================================================================
namespace {

class SimpleComponentizer {
    const litehtml::css_token_vector& m_input;
    size_t m_pos = 0;

    litehtml::css_token peek() {
        return m_pos < m_input.size() ? m_input[m_pos] : litehtml::css_token(litehtml::_EOF);
    }

    litehtml::css_token consume() {
        return m_pos < m_input.size() ? m_input[m_pos++] : litehtml::css_token(litehtml::_EOF);
    }

    litehtml::css_token_vector consume_block_content(char closing) {
        litehtml::css_token_vector content;
        int depth = 1;
        while (m_pos < m_input.size()) {
            auto tok = peek();
            if (tok.type == closing) {
                if (depth == 1) { consume(); return content; }
                depth--;
            } else if (tok.type == '(' || tok.type == litehtml::FUNCTION ||
                       tok.type == '{' || tok.type == '[') {
                if (tok.type == '(' || tok.type == litehtml::FUNCTION) depth++;
                else if (tok.type == '{') depth++;
                else if (tok.type == '[') depth++;
            } else if (tok.type == ')' || tok.type == '}' || tok.type == ']') {
                depth--;
                if (depth == 0) { return content; }
            }
            content.push_back(consume_component_value());
        }
        return content;
    }

public:
    SimpleComponentizer(const litehtml::css_token_vector& input) : m_input(input) {}

    litehtml::css_token consume_component_value() {
        auto tok = consume();
        if (tok.type == litehtml::_EOF) return tok;

        if (tok.type == litehtml::FUNCTION) {
            litehtml::css_token cv(litehtml::CV_FUNCTION, tok.name);
            cv.value = consume_block_content(')');
            return cv;
        }

        if (tok.type == '(') {
            litehtml::css_token block(litehtml::ROUND_BLOCK);
            block.value = consume_block_content(')');
            return block;
        }

        if (tok.type == '{') {
            litehtml::css_token block(litehtml::CURLY_BLOCK);
            block.value = consume_block_content('}');
            return block;
        }

        if (tok.type == '[') {
            litehtml::css_token block(litehtml::SQUARE_BLOCK);
            block.value = consume_block_content(']');
            return block;
        }

        return tok;
    }
};

litehtml::css_token_vector normalize_componentized(const litehtml::css_token_vector& input) {
    SimpleComponentizer comp(input);
    litehtml::css_token_vector result;
    while (true) {
        auto tok = comp.consume_component_value();
        if (tok.type == litehtml::_EOF) break;
        result.push_back(tok);
    }
    return result;
}

std::vector<litehtml::css_token_vector> split_by_comma(const litehtml::css_token_vector& tokens) {
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

// ============================================================================
// Isomorphic copy of backdrop filter chain building
// (from container_skia.cpp push_backdrop_filter, lines 148-337)
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
                            angle_deg = angle_deg * 180.0f / kSkScalarPI;
                        } else if (unit == "turn") {
                            angle_deg = angle_deg * 360.0f;
                        } else if (unit == "grad") {
                            angle_deg = angle_deg * 0.9f;
                        }
                    }
                    float c = cosf(angle_deg * kSkScalarPI / 180.0f);
                    float s = sinf(angle_deg * kSkScalarPI / 180.0f);
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

// ============================================================================
// Isomorphic copy of filter chain building (push_filter version)
// (from container_skia.cpp push_filter, lines 2130-2179)
// Handles: blur, drop-shadow
// ============================================================================

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
                    litehtml::web_color color(0, 0, 0); // default black
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

// ============================================================================
// Test helper: tokenize a CSS filter string and componentize it
// ============================================================================

litehtml::css_token_vector parse_filter_string(const std::string& css) {
    auto raw = litehtml::tokenize(css);
    return normalize_componentized(raw);
}

} // anonymous namespace

// ============================================================================
// Tests: push_backdrop_filter chain (9 filter types)
// ============================================================================

// ── Blur ──────────────────────────────────────

TEST(BackdropFilterBlur, FivePx) {
    auto tokens = parse_filter_string("blur(5px)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->fType, SkImageFilter::kBlur);
    EXPECT_FLOAT_EQ(filter->fSigmaX, 5.0f);
    EXPECT_FLOAT_EQ(filter->fSigmaY, 5.0f);
    EXPECT_EQ(filter->fInput, nullptr);
}

TEST(BackdropFilterBlur, ZeroBlur) {
    // blur(0) should produce no filter (sigma <= 0 → skipped)
    auto tokens = parse_filter_string("blur(0px)");
    auto filter = build_backdrop_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

// ── Brightness ────────────────────────────────

TEST(BackdropFilterBrightness, Number) {
    auto tokens = parse_filter_string("brightness(1.5)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fColorFilter, nullptr);
    EXPECT_EQ(filter->fColorFilter->fType, SkColorFilter::kMatrix);

    // amount = 1.5, mat has amount on diagonal
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 1.5f);
    EXPECT_FLOAT_EQ(m.fMat[6], 1.5f);
    EXPECT_FLOAT_EQ(m.fMat[12], 1.5f);
    EXPECT_FLOAT_EQ(m.fMat[18], 1.0f);
}

TEST(BackdropFilterBrightness, Percentage) {
    auto tokens = parse_filter_string("brightness(50%)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // 50% → amount = 0.5
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 0.5f);
    EXPECT_FLOAT_EQ(m.fMat[6], 0.5f);
    EXPECT_FLOAT_EQ(m.fMat[12], 0.5f);
}

// ── Contrast ──────────────────────────────────

TEST(BackdropFilterContrast, Number) {
    auto tokens = parse_filter_string("contrast(2.0)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fColorFilter, nullptr);
    EXPECT_EQ(filter->fColorFilter->fType, SkColorFilter::kMatrix);

    // amount = 2.0, intercept = -(0.5*2.0)+0.5 = -0.5
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 2.0f);
    EXPECT_FLOAT_EQ(m.fMat[4], -0.5f);  // intercept R
    EXPECT_FLOAT_EQ(m.fMat[6], 2.0f);
    EXPECT_FLOAT_EQ(m.fMat[9], -0.5f);  // intercept G
    EXPECT_FLOAT_EQ(m.fMat[12], 2.0f);
    EXPECT_FLOAT_EQ(m.fMat[14], -0.5f); // intercept B
    EXPECT_FLOAT_EQ(m.fMat[18], 1.0f);
}

TEST(BackdropFilterContrast, Percentage) {
    auto tokens = parse_filter_string("contrast(200%)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // 200% → amount = 2.0
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 2.0f);
    EXPECT_FLOAT_EQ(m.fMat[4], -0.5f);
}

// ── Grayscale ─────────────────────────────────

TEST(BackdropFilterGrayscale, Number) {
    auto tokens = parse_filter_string("grayscale(1)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // amount = 1.0 → setSaturation(0.0) → full grayscale
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 0.2126f);  // R-lum when saturation=0
    EXPECT_FLOAT_EQ(m.fMat[1], 0.2126f);
    EXPECT_FLOAT_EQ(m.fMat[2], 0.2126f);
    EXPECT_FLOAT_EQ(m.fMat[5], 0.7152f);  // G-lum
    EXPECT_FLOAT_EQ(m.fMat[6], 0.7152f);
    EXPECT_FLOAT_EQ(m.fMat[7], 0.7152f);
    EXPECT_FLOAT_EQ(m.fMat[10], 0.0722f); // B-lum
    EXPECT_FLOAT_EQ(m.fMat[11], 0.0722f);
    EXPECT_FLOAT_EQ(m.fMat[12], 0.0722f);
}

TEST(BackdropFilterGrayscale, Percentage) {
    auto tokens = parse_filter_string("grayscale(100%)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // 100% → amount = 1.0
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 0.2126f);
    EXPECT_FLOAT_EQ(m.fMat[6], 0.7152f);
    EXPECT_FLOAT_EQ(m.fMat[12], 0.0722f);
}

TEST(BackdropFilterGrayscale, Half) {
    auto tokens = parse_filter_string("grayscale(0.5)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // amount = 0.5 → setSaturation(0.5)
    // saturation=0.5: i_amount=0.5
    // R: 0.5*0.2126 + 0.5 = 0.1063 + 0.5 = 0.6063
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_NEAR(m.fMat[0], 0.6063f, 0.001f);
    EXPECT_NEAR(m.fMat[6], 0.8576f, 0.001f);  // 0.5*0.7152 + 0.5
}

// ── Sepia ─────────────────────────────────────

TEST(BackdropFilterSepia, Half) {
    auto tokens = parse_filter_string("sepia(0.5)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // amount = 0.5, 1-amount = 0.5
    const auto& m = filter->fColorFilter->fMatrix;
    // mat[0] = 0.393 + 0.607 * 0.5 = 0.6965
    EXPECT_NEAR(m.fMat[0], 0.6965f, 0.001f);
    // mat[1] = 0.769 - 0.769 * 0.5 = 0.3845
    EXPECT_NEAR(m.fMat[1], 0.3845f, 0.001f);
    // mat[2] = 0.189 - 0.189 * 0.5 = 0.0945
    EXPECT_NEAR(m.fMat[2], 0.0945f, 0.001f);
    EXPECT_FLOAT_EQ(m.fMat[18], 1.0f);
}

TEST(BackdropFilterSepia, Percentage) {
    auto tokens = parse_filter_string("sepia(50%)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_NEAR(m.fMat[0], 0.6965f, 0.001f);
}

TEST(BackdropFilterSepia, Full) {
    auto tokens = parse_filter_string("sepia(1)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    const auto& m = filter->fColorFilter->fMatrix;
    // amount=1: 1-amount=0, so mat = sepia matrix
    EXPECT_NEAR(m.fMat[0], 0.393f, 0.001f);
    EXPECT_NEAR(m.fMat[1], 0.769f, 0.001f);
    EXPECT_NEAR(m.fMat[2], 0.189f, 0.001f);
}

// ── Saturate ──────────────────────────────────

TEST(BackdropFilterSaturate, Double) {
    auto tokens = parse_filter_string("saturate(2.0)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fColorFilter, nullptr);
    EXPECT_EQ(filter->fColorFilter->fType, SkColorFilter::kMatrix);

    // amount = 2.0 → setSaturation(2.0)
    const auto& m = filter->fColorFilter->fMatrix;
    // i_amount = 1 - 2.0 = -1.0
    // fMat[0] = i_amount * r_lum + amount = -0.2126 + 2 = 1.7874
    EXPECT_NEAR(m.fMat[0], 1.7874f, 0.001f);
    // fMat[1] = i_amount * r_lum = -0.2126
    EXPECT_NEAR(m.fMat[1], -0.2126f, 0.001f);
    // fMat[5] = i_amount * g_lum = -0.7152
    EXPECT_NEAR(m.fMat[5], -0.7152f, 0.001f);
    // fMat[6] = i_amount * g_lum + amount = -0.7152 + 2 = 1.2848
    EXPECT_NEAR(m.fMat[6], 1.2848f, 0.001f);
    // fMat[12] = i_amount * b_lum + amount = -0.0722 + 2 = 1.9278
    EXPECT_NEAR(m.fMat[12], 1.9278f, 0.001f);
}

TEST(BackdropFilterSaturate, Percentage) {
    auto tokens = parse_filter_string("saturate(200%)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // 200% → amount = 2.0
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_NEAR(m.fMat[0], 1.7874f, 0.001f);
}

// ── Hue-rotate ────────────────────────────────

TEST(BackdropFilterHueRotate, NinetyDeg) {
    auto tokens = parse_filter_string("hue-rotate(90deg)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fColorFilter, nullptr);
    EXPECT_EQ(filter->fColorFilter->fType, SkColorFilter::kMatrix);

    // 90 degrees → cos=0, sin=1
    const auto& m = filter->fColorFilter->fMatrix;
    // mat[0] = 0.213 + 0*0.787 - 1*0.213 = 0.0
    EXPECT_NEAR(m.fMat[0], 0.0f, 0.001f);
    // mat[1] = 0.715 - 0*0.715 - 1*0.715 = 0.0
    EXPECT_NEAR(m.fMat[1], 0.0f, 0.001f);
    // mat[2] = 0.072 - 0*0.072 + 1*0.928 = 1.0
    EXPECT_NEAR(m.fMat[2], 1.0f, 0.001f);
    // alpha stays
    EXPECT_FLOAT_EQ(m.fMat[18], 1.0f);
}

TEST(BackdropFilterHueRotate, ZeroDeg) {
    auto tokens = parse_filter_string("hue-rotate(0deg)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // 0 degrees → cos=1, sin=0 → identity matrix
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_NEAR(m.fMat[0], 1.0f, 0.001f);  // 0.213 + 1*0.787 - 0 = 1.0
    EXPECT_NEAR(m.fMat[6], 1.0f, 0.001f);  // 0.715 + 1*0.285 + 0 = 1.0
    EXPECT_NEAR(m.fMat[12], 1.0f, 0.001f); // 0.072 + 1*0.928 + 0 = 1.0
}

TEST(BackdropFilterHueRotate, QuarterTurn) {
    // 0.25turn → 90° → cos=0, sin=1
    auto tokens = parse_filter_string("hue-rotate(0.25turn)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_NEAR(m.fMat[0], 0.0f, 0.001f);
    EXPECT_NEAR(m.fMat[1], 0.0f, 0.001f);
    EXPECT_NEAR(m.fMat[2], 1.0f, 0.001f);
    EXPECT_FLOAT_EQ(m.fMat[18], 1.0f);
}

TEST(BackdropFilterHueRotate, OneTurn) {
    // 1turn → 360° → cos=1, sin=0 → identity matrix
    auto tokens = parse_filter_string("hue-rotate(1turn)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_NEAR(m.fMat[0], 1.0f, 0.001f);
    EXPECT_NEAR(m.fMat[6], 1.0f, 0.001f);
    EXPECT_NEAR(m.fMat[12], 1.0f, 0.001f);
}

TEST(BackdropFilterHueRotate, Rad) {
    // ~1.57rad → 90° → cos=0, sin=1
    auto tokens = parse_filter_string("hue-rotate(1.57rad)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    const auto& m = filter->fColorFilter->fMatrix;
    // 1.57 rad = 89.95°, cos≈0.0, sin≈1.0
    EXPECT_NEAR(m.fMat[0], 0.0f, 0.005f);
    EXPECT_NEAR(m.fMat[2], 1.0f, 0.005f);
    EXPECT_FLOAT_EQ(m.fMat[18], 1.0f);
}

TEST(BackdropFilterHueRotate, Grad) {
    // 100grad → 90° → cos=0, sin=1
    auto tokens = parse_filter_string("hue-rotate(100grad)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_NEAR(m.fMat[0], 0.0f, 0.001f);
    EXPECT_NEAR(m.fMat[2], 1.0f, 0.001f);
}

// ── Invert ────────────────────────────────────

TEST(BackdropFilterInvert, Full) {
    auto tokens = parse_filter_string("invert(1)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fColorFilter, nullptr);
    EXPECT_EQ(filter->fColorFilter->fType, SkColorFilter::kMatrix);

    // amount = 1.0
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], -1.0f);  // 1 - 2*1 = -1
    EXPECT_FLOAT_EQ(m.fMat[4], 1.0f);   // amount
    EXPECT_FLOAT_EQ(m.fMat[6], -1.0f);
    EXPECT_FLOAT_EQ(m.fMat[9], 1.0f);
    EXPECT_FLOAT_EQ(m.fMat[12], -1.0f);
    EXPECT_FLOAT_EQ(m.fMat[14], 1.0f);
    EXPECT_FLOAT_EQ(m.fMat[18], 1.0f);
}

TEST(BackdropFilterInvert, Half) {
    auto tokens = parse_filter_string("invert(0.5)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // amount = 0.5
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 0.0f);   // 1 - 2*0.5 = 0
    EXPECT_FLOAT_EQ(m.fMat[4], 0.5f);
    EXPECT_FLOAT_EQ(m.fMat[6], 0.0f);
    EXPECT_FLOAT_EQ(m.fMat[9], 0.5f);
    EXPECT_FLOAT_EQ(m.fMat[12], 0.0f);
    EXPECT_FLOAT_EQ(m.fMat[14], 0.5f);
}

TEST(BackdropFilterInvert, Percentage) {
    auto tokens = parse_filter_string("invert(100%)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);

    // 100% → amount = 1.0
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], -1.0f);
}

// ── Opacity ───────────────────────────────────

TEST(BackdropFilterOpacity, Half) {
    auto tokens = parse_filter_string("opacity(0.5)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fColorFilter, nullptr);
    EXPECT_EQ(filter->fColorFilter->fType, SkColorFilter::kBlend);

    EXPECT_EQ(filter->fColorFilter->fMode, SkBlendMode::kDstIn);
    // amount = 0.5 → (uint8_t)(0.5 * 255) = 127
    SkColor expected = SkColorSetARGB(127, 255, 255, 255);
    EXPECT_EQ(filter->fColorFilter->fColor, expected);
}

TEST(BackdropFilterOpacity, Percentage) {
    auto tokens = parse_filter_string("opacity(50%)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fColorFilter, nullptr);
    EXPECT_EQ(filter->fColorFilter->fType, SkColorFilter::kBlend);
    EXPECT_EQ(filter->fColorFilter->fMode, SkBlendMode::kDstIn);

    // 50% → amount = 0.5 → same as above
    SkColor expected = SkColorSetARGB(127, 255, 255, 255);
    EXPECT_EQ(filter->fColorFilter->fColor, expected);
}

// ── Compound filters ──────────────────────────

TEST(BackdropFilterCompound, BlurAndBrightness) {
    auto tokens = parse_filter_string("blur(5px) brightness(1.5)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);

    // The chain: Blur is outermost, Brightness is input (last_filter chain)
    // First blur is applied, then brightness wraps it
    // So outer filter should be ColorFilter (brightness), input is Blur
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    ASSERT_NE(filter->fInput, nullptr);
    EXPECT_EQ(filter->fInput->fType, SkImageFilter::kBlur);
    EXPECT_FLOAT_EQ(filter->fInput->fSigmaX, 5.0f);
    EXPECT_FLOAT_EQ(filter->fInput->fSigmaY, 5.0f);
}

TEST(BackdropFilterCompound, ThreeFilters) {
    auto tokens = parse_filter_string("grayscale(0.5) brightness(2.0) contrast(1.5)");
    auto filter = build_backdrop_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);

    // Chain: contrast wraps brightness wraps grayscale
    // Outer: contrast (kColorFilter)
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    // Middle: brightness
    ASSERT_NE(filter->fInput, nullptr);
    ASSERT_EQ(filter->fInput->fType, SkImageFilter::kColorFilter);
    // Inner: grayscale
    ASSERT_NE(filter->fInput->fInput, nullptr);
    ASSERT_EQ(filter->fInput->fInput->fType, SkImageFilter::kColorFilter);
    EXPECT_EQ(filter->fInput->fInput->fInput, nullptr);
}

// ── Empty filter list ─────────────────────────

TEST(BackdropFilterEmpty, NoTokens) {
    litehtml::css_token_vector empty;
    auto filter = build_backdrop_filter_chain(empty);
    EXPECT_EQ(filter, nullptr);
}

TEST(BackdropFilterEmpty, WhitespaceOnly) {
    // A filter string with only whitespace produces no CV_FUNCTION tokens
    auto tokens = parse_filter_string("   ");
    auto filter = build_backdrop_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

// ── Invalid syntax ────────────────────────────

TEST(BackdropFilterInvalid, UnknownFunction) {
    auto tokens = parse_filter_string("unknown(10px)");
    auto filter = build_backdrop_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

TEST(BackdropFilterInvalid, MissingArgs) {
    auto tokens = parse_filter_string("blur()");
    auto filter = build_backdrop_filter_chain(tokens);

    // blur() with no args → nothing built
    EXPECT_EQ(filter, nullptr);
}

TEST(BackdropFilterInvalid, NonNumericArg) {
    auto tokens = parse_filter_string("brightness(invalid)");
    auto filter = build_backdrop_filter_chain(tokens);

    // Non-numeric arg → amount stays at default (1.0 for brightness)
    // So a ColorFilter with identity-like matrix is built
    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(filter->fType, SkImageFilter::kColorFilter);
    const auto& m = filter->fColorFilter->fMatrix;
    EXPECT_FLOAT_EQ(m.fMat[0], 1.0f);  // default amount
}

TEST(BackdropFilterInvalid, NegativeBlur) {
    // blur with negative value is rejected by f_positive flag
    // from_token returns false, sigma stays at 0 → no filter
    auto tokens = parse_filter_string("blur(-5px)");
    auto filter = build_backdrop_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

TEST(BackdropFilterInvalid, MalformedToken) {
    // "blur(5px" without closing paren — tokenizer may produce different results
    auto raw = litehtml::tokenize("blur(5px");
    auto tokens = normalize_componentized(raw);
    auto filter = build_backdrop_filter_chain(tokens);
    // Should not crash; either produces filter or nullptr
    (void)filter;
}

// ============================================================================
// Tests: push_filter chain (blur, drop-shadow)
// ============================================================================

// ── Blur ──────────────────────────────────────

TEST(FilterBlur, FivePx) {
    auto tokens = parse_filter_string("blur(5px)");
    auto filter = build_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->fType, SkImageFilter::kBlur);
    EXPECT_FLOAT_EQ(filter->fSigmaX, 5.0f);
    EXPECT_FLOAT_EQ(filter->fSigmaY, 5.0f);
    EXPECT_EQ(filter->fInput, nullptr);
}

// ── Drop-shadow ───────────────────────────────

TEST(FilterDropShadow, OffsetAndBlur) {
    auto tokens = parse_filter_string("drop-shadow(5px 5px 10px)");
    auto filter = build_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->fType, SkImageFilter::kDropShadow);
    EXPECT_FLOAT_EQ(filter->fDx, 5.0f);
    EXPECT_FLOAT_EQ(filter->fDy, 5.0f);
    // blur * 0.5 = 10 * 0.5 = 5.0
    EXPECT_FLOAT_EQ(filter->fSigmaX, 5.0f);
    EXPECT_FLOAT_EQ(filter->fSigmaY, 5.0f);
    // default color: black (A=255, R=0, G=0, B=0)
    EXPECT_EQ(filter->fColor, SkColorSetARGB(255, 0, 0, 0));
}

TEST(FilterDropShadow, OffsetOnly) {
    auto tokens = parse_filter_string("drop-shadow(5px 5px)");
    auto filter = build_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->fType, SkImageFilter::kDropShadow);
    EXPECT_FLOAT_EQ(filter->fDx, 5.0f);
    EXPECT_FLOAT_EQ(filter->fDy, 5.0f);
    // no blur → sigma = 0
    EXPECT_FLOAT_EQ(filter->fSigmaX, 0.0f);
    EXPECT_FLOAT_EQ(filter->fSigmaY, 0.0f);
}

TEST(FilterDropShadow, WithColor) {
    auto tokens = parse_filter_string("drop-shadow(5px 5px 10px red)");
    auto filter = build_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->fType, SkImageFilter::kDropShadow);
    EXPECT_FLOAT_EQ(filter->fDx, 5.0f);
    EXPECT_FLOAT_EQ(filter->fDy, 5.0f);
    EXPECT_FLOAT_EQ(filter->fSigmaX, 5.0f);
    EXPECT_FLOAT_EQ(filter->fSigmaY, 5.0f);
    // red = A=255, R=255, G=0, B=0
    EXPECT_EQ(filter->fColor, SkColorSetARGB(255, 255, 0, 0));
}

TEST(FilterDropShadow, ZeroOffset) {
    // drop-shadow(0 0 0) → dx=0, dy=0, blur=0 → no filter (condition fails)
    auto tokens = parse_filter_string("drop-shadow(0px 0px 0px)");
    auto filter = build_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

TEST(FilterDropShadow, BlueColor) {
    auto tokens = parse_filter_string("drop-shadow(2px 4px 6px blue)");
    auto filter = build_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->fColor, SkColorSetARGB(255, 0, 0, 255));
}

// ── Compound filters ──────────────────────────

TEST(FilterCompound, BlurAndDropShadow) {
    auto tokens = parse_filter_string("blur(3px) drop-shadow(2px 2px 4px)");
    auto filter = build_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    // Outer: drop-shadow
    ASSERT_EQ(filter->fType, SkImageFilter::kDropShadow);
    EXPECT_FLOAT_EQ(filter->fDx, 2.0f);
    EXPECT_FLOAT_EQ(filter->fDy, 2.0f);
    // Inner: blur
    ASSERT_NE(filter->fInput, nullptr);
    EXPECT_EQ(filter->fInput->fType, SkImageFilter::kBlur);
    EXPECT_FLOAT_EQ(filter->fInput->fSigmaX, 3.0f);
    EXPECT_FLOAT_EQ(filter->fInput->fSigmaY, 3.0f);
}

TEST(FilterCompound, DropShadowAndBlur) {
    // Order matters: first function is innermost
    auto tokens = parse_filter_string("drop-shadow(1px 1px 2px red) blur(4px)");
    auto filter = build_filter_chain(tokens);

    ASSERT_NE(filter, nullptr);
    // Outer: blur
    ASSERT_EQ(filter->fType, SkImageFilter::kBlur);
    EXPECT_FLOAT_EQ(filter->fSigmaX, 4.0f);
    // Inner: drop-shadow
    ASSERT_NE(filter->fInput, nullptr);
    EXPECT_EQ(filter->fInput->fType, SkImageFilter::kDropShadow);
    EXPECT_FLOAT_EQ(filter->fInput->fDx, 1.0f);
    EXPECT_FLOAT_EQ(filter->fInput->fDy, 1.0f);
}

// ── Empty filter ──────────────────────────────

TEST(FilterEmpty, NoTokens) {
    litehtml::css_token_vector empty;
    auto filter = build_filter_chain(empty);
    EXPECT_EQ(filter, nullptr);
}

TEST(FilterEmpty, WhitespaceOnly) {
    auto tokens = parse_filter_string("   ");
    auto filter = build_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

// ── Edge cases ────────────────────────────────

TEST(FilterEdge, UnknownFunction) {
    auto tokens = parse_filter_string("unknown(10px)");
    auto filter = build_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

TEST(FilterEdge, NegativeBlur) {
    auto tokens = parse_filter_string("blur(-5px)");
    auto filter = build_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}

TEST(FilterEdge, ZeroBlur) {
    auto tokens = parse_filter_string("blur(0px)");
    auto filter = build_filter_chain(tokens);
    EXPECT_EQ(filter, nullptr);
}
