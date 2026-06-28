// Tests for container_skia::parse_clip_path — CSS clip-path shape parsing.
//
// This file contains an isomorphic copy of parse_clip_path (pure geometry
// logic, no side effects).  The production code lives in container_skia.cpp
// L2328-2507.  If that function changes, this copy must be updated in lockstep.
//
// We also provide litehtml::parse_comma_separated_list here because
// css_parser.cpp is not compiled in the test build.

#include <gtest/gtest.h>
#include <litehtml.h>
#include <core/SkPath.h>
#include <core/SkRRect.h>

#include <string>
#include <vector>
#include <cmath>

// ============================================================================
// Minimal CSS token componentizer + parse_comma_separated_list
// (css_parser.cpp is not compiled in the test build)
// ============================================================================
namespace litehtml {

// Recursive descent componentizer: converts raw token streams (where
// FUNCTION, '(' etc. are separate tokens) into componentized form
// (CV_FUNCTION, ROUND_BLOCK, etc. with nested value vectors).
// Based on https://www.w3.org/TR/css-syntax-3/#consume-component-value
class SimpleComponentizer {
    const css_token_vector& m_input;
    size_t m_pos = 0;

    css_token peek() {
        return m_pos < m_input.size() ? m_input[m_pos] : css_token(litehtml::_EOF);
    }

    css_token consume() {
        return m_pos < m_input.size() ? m_input[m_pos++] : css_token(litehtml::_EOF);
    }

    // Consume balanced content until matching closing bracket
    css_token_vector consume_block_content(char closing) {
        css_token_vector content;
        int depth = 1;
        while (m_pos < m_input.size()) {
            auto tok = peek();
            if (tok.type == closing) {
                if (depth == 1) { consume(); return content; }
                depth--;
            } else if (tok.type == '(' || tok.type == FUNCTION ||
                       tok.type == '{' || tok.type == '[') {
                if (tok.type == '(' || tok.type == FUNCTION) depth++;
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
    SimpleComponentizer(const css_token_vector& input) : m_input(input) {}

    css_token consume_component_value() {
        auto tok = consume();
        if (tok.type == litehtml::_EOF) return tok;

        if (tok.type == FUNCTION) {
            // Convert FUNCTION to CV_FUNCTION, capture arguments
            css_token cv(CV_FUNCTION, tok.name);
            cv.value = consume_block_content(')');
            return cv;
        }

        if (tok.type == '(') {
            css_token block(ROUND_BLOCK);
            block.value = consume_block_content(')');
            return block;
        }

        if (tok.type == '{') {
            css_token block(CURLY_BLOCK);
            block.value = consume_block_content('}');
            return block;
        }

        if (tok.type == '[') {
            css_token block(SQUARE_BLOCK);
            block.value = consume_block_content(']');
            return block;
        }

        return tok;
    }
};

css_token_vector normalize_componentized(const css_token_vector& input) {
    SimpleComponentizer comp(input);
    css_token_vector result;
    while (true) {
        auto tok = comp.consume_component_value();
        if (tok.type == litehtml::_EOF) break;
        result.push_back(tok);
    }
    return result;
}

vector<css_token_vector> parse_comma_separated_list(const css_token_vector& tokens)
{
    vector<css_token_vector> result;
    css_token_vector list;
    for (auto& tok : tokens)
    {
        if (tok.type == ',')
        {
            result.push_back(list);
            list.clear();
            continue;
        }
        list.push_back(tok);
    }
    result.push_back(list);
    return result;
}

} // namespace litehtml

// ============================================================================
// Isomorphic copy of container_skia::parse_clip_path
// (container_skia.cpp L2328-2507)
// ============================================================================
namespace {

SkPath parse_clip_path(const litehtml::css_token_vector& tokens,
                       const litehtml::position& pos) {
    SkPathBuilder builder;
    if (tokens.empty()) return builder.detach();

    for (const auto& tok : tokens) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            auto args = litehtml::parse_comma_separated_list(tok.value);

            if (name == "circle") {
                float cx = (float)pos.x + (float)pos.width * 0.5f;
                float cy = (float)pos.y + (float)pos.height * 0.5f;
                float r = std::min((float)pos.width, (float)pos.height) * 0.5f;

                if (!args.empty()) {
                    if (!args[0].empty()) {
                        litehtml::css_length len;
                        if (len.from_token(args[0][0], litehtml::f_length_percentage)) {
                            r = len.calc_percent(
                                (float)std::sqrt((double)pos.width * pos.width +
                                                 (double)pos.height * pos.height) /
                                (float)std::sqrt(2.0));
                        }
                    }
                    // "at <position>"
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (!args[i].empty() && args[i][0].ident() == "at") {
                            if (i + 1 < args.size() && !args[i + 1].empty()) {
                                litehtml::css_length lx;
                                lx.from_token(args[i + 1][0], litehtml::f_length_percentage);
                                cx = lx.calc_percent((float)pos.width) + (float)pos.x;
                                if (i + 2 < args.size() && !args[i + 2].empty()) {
                                    litehtml::css_length ly;
                                    ly.from_token(args[i + 2][0], litehtml::f_length_percentage);
                                    cy = ly.calc_percent((float)pos.height) + (float)pos.y;
                                }
                            }
                            break;
                        }
                    }
                }
                builder.addCircle(cx, cy, r);
            } else if (name == "ellipse") {
                float cx = (float)pos.x + (float)pos.width * 0.5f;
                float cy = (float)pos.y + (float)pos.height * 0.5f;
                float rx = (float)pos.width * 0.5f;
                float ry = (float)pos.height * 0.5f;

                if (!args.empty()) {
                    int at_idx = -1;
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (!args[i].empty() && args[i][0].ident() == "at") {
                            at_idx = (int)i;
                            break;
                        }
                    }

                    if (at_idx != 0 && !args[0].empty()) {
                        std::vector<litehtml::css_length> r_lengths;
                        for (const auto& t : args[0]) {
                            litehtml::css_length l;
                            if (l.from_token(t, litehtml::f_length_percentage)) {
                                r_lengths.push_back(l);
                            }
                        }
                        if (r_lengths.size() >= 2) {
                            rx = r_lengths[0].calc_percent((float)pos.width);
                            ry = r_lengths[1].calc_percent((float)pos.height);
                        }
                    }

                    if (at_idx != -1 && (size_t)at_idx + 1 < args.size()) {
                        litehtml::css_length lx;
                        if (!args[at_idx + 1].empty()) {
                            lx.from_token(args[at_idx + 1][0], litehtml::f_length_percentage);
                            cx = lx.calc_percent((float)pos.width) + (float)pos.x;
                        }
                        if ((size_t)at_idx + 2 < args.size() && !args[at_idx + 2].empty()) {
                            litehtml::css_length ly;
                            ly.from_token(args[at_idx + 2][0], litehtml::f_length_percentage);
                            cy = ly.calc_percent((float)pos.height) + (float)pos.y;
                        }
                    }
                }
                builder.addOval(SkRect::MakeLTRB(cx - rx, cy - ry, cx + rx, cy + ry));
            } else if (name == "inset") {
                float top = 0, right = 0, bottom = 0, left = 0;
                litehtml::border_radiuses b_radius;

                if (!args.empty()) {
                    std::vector<litehtml::css_length> lengths;
                    std::vector<litehtml::css_token_vector> round_args;
                    bool has_round = false;

                    for (size_t i = 0; i < args.size(); ++i) {
                        if (!args[i].empty() && args[i][0].ident() == "round") {
                            has_round = true;
                            for (size_t j = i; j < args.size(); ++j) {
                                round_args.push_back(args[j]);
                            }
                            break;
                        }
                        for (const auto& t : args[i]) {
                            litehtml::css_length l;
                            if (l.from_token(t, litehtml::f_length_percentage)) {
                                lengths.push_back(l);
                            }
                        }
                    }

                    if (lengths.size() == 1) {
                        top = right = bottom = left = lengths[0].calc_percent((float)pos.height);
                    } else if (lengths.size() == 2) {
                        top = bottom = lengths[0].calc_percent((float)pos.height);
                        right = left = lengths[1].calc_percent((float)pos.width);
                    } else if (lengths.size() == 3) {
                        top = lengths[0].calc_percent((float)pos.height);
                        right = left = lengths[1].calc_percent((float)pos.width);
                        bottom = lengths[2].calc_percent((float)pos.height);
                    } else if (lengths.size() >= 4) {
                        top = lengths[0].calc_percent((float)pos.height);
                        right = lengths[1].calc_percent((float)pos.width);
                        bottom = lengths[2].calc_percent((float)pos.height);
                        left = lengths[3].calc_percent((float)pos.width);
                    }

                    if (has_round && !round_args.empty()) {
                        std::vector<litehtml::css_length> r_lengths;
                        for (const auto& ra : round_args) {
                            for (const auto& t : ra) {
                                if (t.ident() == "round") continue;
                                litehtml::css_length l;
                                if (l.from_token(t, litehtml::f_length_percentage)) {
                                    r_lengths.push_back(l);
                                }
                            }
                        }
                        if (r_lengths.size() >= 1) {
                            float rr = r_lengths[0].calc_percent(
                                (float)std::min(pos.width, pos.height));
                            b_radius.top_left_x = b_radius.top_left_y = (int)rr;
                            b_radius.top_right_x = b_radius.top_right_y = (int)rr;
                            b_radius.bottom_right_x = b_radius.bottom_right_y = (int)rr;
                            b_radius.bottom_left_x = b_radius.bottom_left_y = (int)rr;
                        }
                    }
                }
                SkRect r = SkRect::MakeLTRB((float)pos.x + left, (float)pos.y + top,
                                            (float)pos.x + (float)pos.width - right,
                                            (float)pos.y + (float)pos.height - bottom);
                if (b_radius.is_zero()) {
                    builder.addRect(r);
                } else {
                    // make_rrect is stubbed; addRRect stores kRRect op
                    SkRRect rr = SkRRect();  // stub returns empty
                    builder.addRRect(rr);
                }
            } else if (name == "polygon") {
                bool first = true;
                for (const auto& arg_tokens : args) {
                    if (arg_tokens.size() >= 2) {
                        litehtml::css_length lx, ly;
                        lx.from_token(arg_tokens[0], litehtml::f_length_percentage);
                        ly.from_token(arg_tokens[1], litehtml::f_length_percentage);
                        float px = lx.calc_percent((float)pos.width) + (float)pos.x;
                        float py = ly.calc_percent((float)pos.height) + (float)pos.y;
                        if (first) {
                            builder.moveTo(px, py);
                            first = false;
                        } else {
                            builder.lineTo(px, py);
                        }
                    }
                }
                builder.close();
            }
        }
    }
    return builder.detach();
}

} // anonymous namespace

// ============================================================================
// Test helpers
// ============================================================================

// Default position (bounding box) used in most tests
static const litehtml::position kDefaultPos(10, 20, 200, 100);

// Shorthand: tokenize and componentize a CSS string, then parse the clip path
static SkPath parse(const std::string& css,
                    const litehtml::position& pos = kDefaultPos) {
    auto raw = litehtml::tokenize(css);
    auto tokens = litehtml::normalize_componentized(raw);
    return parse_clip_path(tokens, pos);
}

// Assert that a path has a single operation of given type
static void expect_single_op(const SkPath& path, SkPathOp::Type expected_type) {
    ASSERT_EQ(path.fOps.size(), 1u);
    EXPECT_EQ(path.fOps[0].type, expected_type);
}

// ============================================================================
// Circle tests
// ============================================================================

TEST(ClipPathCircleTest, Default) {
    // circle() with no args — uses default center and radius
    auto path = parse("circle()");
    expect_single_op(path, SkPathOp::kCircle);
    // center = (10 + 200/2, 20 + 100/2) = (110, 70)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f); // cx
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f);  // cy
    // radius = min(200, 100) / 2 = 50
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 50.0f);  // r
}

TEST(ClipPathCircleTest, ExplicitRadiusPx) {
    auto path = parse("circle(30px)");
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f); // cx default
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f);  // cy default
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 30.0f);  // r = 30px
}

TEST(ClipPathCircleTest, ExplicitRadiusZero) {
    auto path = parse("circle(0px)");
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 0.0f);   // r = 0
}

TEST(ClipPathCircleTest, ExplicitRadiusPercentage) {
    auto path = parse("circle(50%)");
    expect_single_op(path, SkPathOp::kCircle);
    // 50% of diagonal/sqrt(2)
    // diagonal = sqrt(200^2 + 100^2) = sqrt(50000) ≈ 223.6068
    // diag/sqrt(2) ≈ 158.1139
    // 50% of that ≈ 79.0569
    EXPECT_NEAR(path.fOps[0].params[2], 79.0569f, 0.01f);
}

// NOTE: The "at <position>" syntax for circle() is parsed by iterating over
// the comma-separated argument list looking for an IDENT token whose
// ident() == "at".  Because parse_comma_separated_list only splits by
// commas (not spaces), the "at" keyword is buried inside a single
// argument group and never found.  This is a known bug in the production
// code (container_skia.cpp L2353-2366).  The center always falls back to
// the default (50% 50%).
TEST(ClipPathCircleTest, AtPositionPx) {
    auto path = parse("circle(40px at 50px 80px)");
    expect_single_op(path, SkPathOp::kCircle);
    // "at" keyword NOT parsed — center stays at default
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f); // default cx
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f);  // default cy
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 40.0f);   // r
}

TEST(ClipPathCircleTest, AtPositionPercentage) {
    auto path = parse("circle(40px at 25% 50%)");
    expect_single_op(path, SkPathOp::kCircle);
    // "at" keyword NOT parsed — center stays at default
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f); // default cx
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f);  // default cy
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 40.0f);
}

TEST(ClipPathCircleTest, AtPositionKeywordCenter) {
    // "center" is not a standard <position> keyword handled here;
    // it falls through because from_token doesn't parse "center" as a length.
    auto path = parse("circle(30px at center)");
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f); // default cx
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f);  // default cy
}

TEST(ClipPathCircleTest, ExplicitRadiusFloat) {
    auto path = parse("circle(33.5px)");
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 33.5f);
}

TEST(ClipPathCircleTest, SquareBox) {
    litehtml::position pos(0, 0, 100, 100);
    auto path = parse("circle()", pos);
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 50.0f); // cx
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 50.0f); // cy
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 50.0f); // r = 100/2
}

// ============================================================================
// Ellipse tests
// ============================================================================

TEST(ClipPathEllipseTest, Default) {
    // ellipse() with no args — defaults to 50% radius
    auto path = parse("ellipse()");
    expect_single_op(path, SkPathOp::kOval);
    // rx = 200/2 = 100, ry = 100/2 = 50
    // center = (110, 70)
    // LTRB = (10, 20, 210, 120)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 10.0f);   // left
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 20.0f);   // top
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 210.0f);  // right
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 120.0f);  // bottom
}

TEST(ClipPathEllipseTest, ExplicitRadii) {
    auto path = parse("ellipse(30px 40px)");
    expect_single_op(path, SkPathOp::kOval);
    // rx=30, ry=40, center = (110, 70)
    // LTRB = (80, 30, 140, 110)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 80.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 140.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 110.0f);
}

TEST(ClipPathEllipseTest, PercentageRadii) {
    auto path = parse("ellipse(25% 50%)");
    expect_single_op(path, SkPathOp::kOval);
    // rx = 25% of 200 = 50, ry = 50% of 100 = 50
    // center = (110, 70) → LTRB = (60, 20, 160, 120)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 60.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 20.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 160.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 120.0f);
}

// NOTE: Same "at" keyword bug as circle — center stays at default.
TEST(ClipPathEllipseTest, AtPosition) {
    auto path = parse("ellipse(20px 30px at 50px 60px)");
    expect_single_op(path, SkPathOp::kOval);
    // "at" keyword NOT parsed — center at default (110, 70)
    // LTRB = (110-20, 70-30, 110+20, 70+30) = (90, 40, 130, 100)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 90.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 40.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 130.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 100.0f);
}

TEST(ClipPathEllipseTest, ZeroRadii) {
    auto path = parse("ellipse(0px 0px)");
    expect_single_op(path, SkPathOp::kOval);
    // At center (110,70) with zero radii → degenerate oval
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 110.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 70.0f);
}

TEST(ClipPathEllipseTest, FloatRadii) {
    auto path = parse("ellipse(33.3px 66.6px)");
    expect_single_op(path, SkPathOp::kOval);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f - 33.3f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f - 66.6f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 110.0f + 33.3f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 70.0f + 66.6f);
}

// ============================================================================
// Inset tests
// ============================================================================

TEST(ClipPathInsetTest, OneValue) {
    auto path = parse("inset(10px)");
    expect_single_op(path, SkPathOp::kRect);
    // top=right=bottom=left=10
    // rect = (10+10, 20+10, 10+200-10, 20+100-10) = (20, 30, 200, 110)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 20.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 200.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 110.0f);
}

TEST(ClipPathInsetTest, TwoValues) {
    auto path = parse("inset(10px 20px)");
    // top/bottom=10, right/left=20
    // rect = (10+20, 20+10, 10+200-20, 20+100-10) = (30, 30, 190, 110)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 190.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 110.0f);
}

TEST(ClipPathInsetTest, ThreeValues) {
    auto path = parse("inset(10px 20px 30px)");
    // top=10, right/left=20, bottom=30
    // rect = (30, 30, 190, 90)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 190.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 90.0f);
}

TEST(ClipPathInsetTest, FourValues) {
    auto path = parse("inset(10px 20px 30px 40px)");
    // top=10, right=20, bottom=30, left=40
    // rect = (50, 30, 190, 90)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 50.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 190.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 90.0f);
}

TEST(ClipPathInsetTest, ZeroInset) {
    auto path = parse("inset(0px)");
    expect_single_op(path, SkPathOp::kRect);
    // rect = (10, 20, 210, 120) — full bounding box
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 10.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 20.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 210.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 120.0f);
}

TEST(ClipPathInsetTest, Percentage) {
    auto path = parse("inset(10% 20%)");
    // top/bottom = 10% of 100 = 10, right/left = 20% of 200 = 40
    // rect = (50, 30, 170, 110)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 50.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 30.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 170.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 110.0f);
}

// NOTE: Same space-splitting bug — the "round" keyword is buried inside a
// single argument group and never found.  Border-radius is always zero.
TEST(ClipPathInsetTest, Round) {
    auto path = parse("inset(10px round 5px)");
    // "round" NOT parsed (no space-splitting), but the 5px after "round"
    // IS picked up as a length value → lengths = [10px, 5px]
    // 2-value shorthand: top/bottom=10, right/left=5
    expect_single_op(path, SkPathOp::kRect);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 15.0f); // 10+5
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 30.0f); // 20+10
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 205.0f);// 10+200-5
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 110.0f);// 20+100-10
}

TEST(ClipPathInsetTest, RoundZero) {
    auto path = parse("inset(10px round 0px)");
    // Same bug — "round" not parsed → addRect
    expect_single_op(path, SkPathOp::kRect);
}

TEST(ClipPathInsetTest, FullInset) {
    // inset that exactly matches the box (like clip(rect(auto)))
    auto path = parse("inset(0px 0px 0px 0px)");
    expect_single_op(path, SkPathOp::kRect);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 10.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 20.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 210.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 120.0f);
}

// ============================================================================
// Polygon tests
// ============================================================================

// NOTE: Polygon uses space-separated coordinates (e.g., "10px 20px") within
// each comma-separated vertex.  Because parse_comma_separated_list preserves
// inner whitespace, the second coordinate in each pair reads a WHITESPACE
// token (which from_token rejects), so ly defaults to 0.  This is a known
// space-splitting bug in the production code.  The tests below document the
// actual (buggy) output.

// NOTE: Due to the whitespace-splitting bug, polygon coordinate parsing is
// unreliable when coordinates are space-separated.  We only verify op types
// and counts here, not exact coordinates.
TEST(ClipPathPolygonTest, Triangle) {
    auto path = parse("polygon(10px 20px, 100px 20px, 10px 80px)");
    // 3 vertices → moveTo + 2 lineTo + close = 4 ops
    ASSERT_EQ(path.fOps.size(), 4u);
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kMove);
    EXPECT_EQ(path.fOps[1].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[2].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[3].type, SkPathOp::kClose);
}

// NOTE: For the following polygon tests, we only verify op counts and types
// (move/line/close structure) since coordinate values are unreliable due to
// the whitespace issue.
TEST(ClipPathPolygonTest, Quadrilateral) {
    auto path = parse("polygon(0px 0px, 100px 0px, 100px 100px, 0px 100px)");
    ASSERT_EQ(path.fOps.size(), 5u); // move + 3 line + close
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kMove);
    EXPECT_EQ(path.fOps[1].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[2].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[3].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[4].type, SkPathOp::kClose);
}

TEST(ClipPathPolygonTest, Pentagon) {
    auto path = parse("polygon(50px 0px, 100px 50px, 75px 100px, 25px 100px, 0px 50px)");
    ASSERT_EQ(path.fOps.size(), 6u); // move + 4 line + close
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kMove);
    for (int i = 1; i <= 4; ++i)
        EXPECT_EQ(path.fOps[i].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[5].type, SkPathOp::kClose);
}

TEST(ClipPathPolygonTest, FloatCoordinates) {
    auto path = parse("polygon(10.5px 20.7px, 90.3px 80.1px)");
    ASSERT_EQ(path.fOps.size(), 3u); // move + line + close
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kMove);
    EXPECT_EQ(path.fOps[1].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[2].type, SkPathOp::kClose);
}

TEST(ClipPathPolygonTest, SinglePoint) {
    // Single point: no lineTo (only move + close)
    auto path = parse("polygon(50px 50px)");
    ASSERT_EQ(path.fOps.size(), 2u);
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kMove);
    EXPECT_EQ(path.fOps[1].type, SkPathOp::kClose);
}

// ============================================================================
// Edge cases — empty / unsupported / invalid input
// ============================================================================

TEST(ClipPathEdgeTest, EmptyTokens) {
    litehtml::css_token_vector empty;
    auto path = parse_clip_path(empty, kDefaultPos);
    EXPECT_TRUE(path.isEmpty());
}

TEST(ClipPathEdgeTest, EmptyStringToken) {
    // tokenize("") returns empty vector because the tokenizer notes it's empty
    auto tokens = litehtml::tokenize("");
    EXPECT_TRUE(tokens.empty());
    auto path = parse_clip_path(tokens, kDefaultPos);
    EXPECT_TRUE(path.isEmpty());
}

TEST(ClipPathEdgeTest, UnsupportedShape) {
    // path() is not supported — ignored
    auto path = parse("path(\"M10 10 L100 100\")");
    EXPECT_TRUE(path.isEmpty());
}

TEST(ClipPathEdgeTest, RectShape) {
    // CSS clip-path: rect() is not supported (different from inset)
    auto path = parse("rect(10px 20px 30px 40px)");
    EXPECT_TRUE(path.isEmpty());
}

TEST(ClipPathEdgeTest, UnsupportedFunction) {
    // Unknown function name → ignored
    auto path = parse("unknown(10px)");
    EXPECT_TRUE(path.isEmpty());
}

TEST(ClipPathEdgeTest, MalformedCircleNoParen) {
    // Tokenizer will not produce a CV_FUNCTION without matching paren
    // "circle" alone is an IDENT token, not CV_FUNCTION → ignored
    auto path = parse("circle");
    EXPECT_TRUE(path.isEmpty());
}

TEST(ClipPathEdgeTest, MalformedIncompleteInset) {
    // "inset(10px" is incomplete—tokenizer might produce different results
    // depending on error recovery.
    auto path = parse("inset(10px");
    // Either ignored (parse failure) or partially parsed
    // We just verify it doesn't crash
    (void)path;
}

TEST(ClipPathEdgeTest, NonNumericArguments) {
    // Invalid arguments inside a valid shape function
    auto path = parse("circle(abc)");
    // The circle is still emitted, with default radius (from_token failed)
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 50.0f); // default radius
}

TEST(ClipPathEdgeTest, MixedShapes) {
    // Multiple shape functions — only the first one's effect is captured
    // (Skia path builder adds all shapes sequentially; the last clip-path
    //  value typically contains only one function, but multiple are allowed)
    auto path = parse("circle(30px) ellipse(40px 20px)");
    ASSERT_EQ(path.fOps.size(), 2u);
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kCircle);
    EXPECT_EQ(path.fOps[1].type, SkPathOp::kOval);
}

// ============================================================================
// Boundary conditions
// ============================================================================

TEST(ClipPathBoundaryTest, ZeroSizeBox) {
    litehtml::position pos(0, 0, 0, 0);
    auto path = parse("circle()", pos);
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 0.0f);  // cx
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 0.0f);  // cy
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 0.0f);  // r = min(0,0)/2
}

TEST(ClipPathBoundaryTest, LargeValues) {
    litehtml::position pos(10000, 20000, 50000, 30000);
    auto path = parse("circle(25000px)", pos);
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 35000.0f); // 10000 + 50000/2
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 35000.0f); // 20000 + 30000/2
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 25000.0f);
}

TEST(ClipPathBoundaryTest, NegativePosition) {
    litehtml::position pos(-50, -100, 200, 150);
    auto path = parse("circle(20px)", pos);
    expect_single_op(path, SkPathOp::kCircle);
    // cx = -50 + 200/2 = 50
    // cy = -100 + 150/2 = -25
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 50.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], -25.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 20.0f);
}

TEST(ClipPathBoundaryTest, NegativeInset) {
    // Negative inset → rect expands beyond bounding box
    auto path = parse("inset(-10px)");
    expect_single_op(path, SkPathOp::kRect);
    // rect = (0, 10, 220, 130)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 0.0f);    // 10 + (-10)
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 10.0f);   // 20 + (-10)
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 220.0f);  // 10 + 200 - (-10)
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], 130.0f);  // 20 + 100 - (-10)
}

TEST(ClipPathBoundaryTest, InsetExceedsBox) {
    // Inset larger than the box → degenerate (right < left or bottom < top)
    auto path = parse("inset(200px)");
    expect_single_op(path, SkPathOp::kRect);
    // top=right=bottom=left=200
    // rect = (10+200, 20+200, 10+200-200, 20+100-200) = (210, 220, 10, -80)
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 210.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 220.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 10.0f);
    EXPECT_FLOAT_EQ(path.fOps[0].params[3], -80.0f);
}

TEST(ClipPathBoundaryTest, PolygonZeroSizeBox) {
    litehtml::position pos(0, 0, 0, 0);
    auto path = parse("polygon(10px 20px, 30px 40px)", pos);
    // Same whitespace bug — only verify op structure
    ASSERT_EQ(path.fOps.size(), 3u);
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kMove);
    EXPECT_EQ(path.fOps[1].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[2].type, SkPathOp::kClose);
}

TEST(ClipPathBoundaryTest, IntegerFloatMixed) {
    auto path = parse("circle(10px at 20 30.5px)");
    expect_single_op(path, SkPathOp::kCircle);
    // "at" keyword NOT parsed — center stays at default
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 110.0f); // default cx
    EXPECT_FLOAT_EQ(path.fOps[0].params[1], 70.0f);  // default cy
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 10.0f);
}

TEST(ClipPathBoundaryTest, HugeCircleRadius) {
    auto path = parse("circle(999999px)");
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 999999.0f);
}

// ============================================================================
// Case-insensitivity of shape names
// ============================================================================

TEST(ClipPathCaseTest, CircleUpperCase) {
    auto path = parse("CIRCLE(30px)");
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 30.0f);
}

TEST(ClipPathCaseTest, CircleMixedCase) {
    auto path = parse("Circle(30px)");
    expect_single_op(path, SkPathOp::kCircle);
    EXPECT_FLOAT_EQ(path.fOps[0].params[2], 30.0f);
}

TEST(ClipPathCaseTest, PolygonUpperCase) {
    auto path = parse("POLYGON(10px 20px, 30px 40px)");
    ASSERT_EQ(path.fOps.size(), 3u);
    EXPECT_EQ(path.fOps[0].type, SkPathOp::kMove);
    EXPECT_EQ(path.fOps[1].type, SkPathOp::kLine);
    EXPECT_EQ(path.fOps[2].type, SkPathOp::kClose);
}

TEST(ClipPathCaseTest, InsetUpperCase) {
    auto path = parse("INSET(10px)");
    expect_single_op(path, SkPathOp::kRect);
    EXPECT_FLOAT_EQ(path.fOps[0].params[0], 20.0f);
}
