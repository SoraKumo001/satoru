#include <gtest/gtest.h>

#include "bridge/bridge_types.h"
#include "include/core/SkFontStyle.h"

// ============================================================================
// font_request::operator<
// ============================================================================

TEST(font_request_test, less_than_by_family) {
    font_request a{"Arial", 400, SkFontStyle::kUpright_Slant};
    font_request b{"Verdana", 400, SkFontStyle::kUpright_Slant};
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(font_request_test, less_than_by_weight) {
    font_request a{"Arial", 400, SkFontStyle::kUpright_Slant};
    font_request b{"Arial", 700, SkFontStyle::kUpright_Slant};
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(font_request_test, less_than_by_slant) {
    font_request a{"Arial", 400, SkFontStyle::kUpright_Slant};
    font_request b{"Arial", 400, SkFontStyle::kItalic_Slant};
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(font_request_test, equal_not_less) {
    font_request a{"Arial", 400, SkFontStyle::kUpright_Slant};
    font_request b{"Arial", 400, SkFontStyle::kUpright_Slant};
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(font_request_test, same_family_diff_weight_and_slant) {
    font_request light{"Arial", 300, SkFontStyle::kUpright_Slant};
    font_request bold_italic{"Arial", 700, SkFontStyle::kItalic_Slant};
    EXPECT_TRUE(light < bold_italic);
    EXPECT_FALSE(bold_italic < light);
}

// ============================================================================
// text_draw_info::operator==
// ============================================================================

TEST(text_draw_info_test, equal) {
    litehtml::web_color color{255, 0, 0, 255};
    text_draw_info a{400, false, color, 1.0f};
    text_draw_info b{400, false, color, 1.0f};
    EXPECT_TRUE(a == b);
}

TEST(text_draw_info_test, not_equal_weight) {
    litehtml::web_color color{255, 0, 0, 255};
    text_draw_info a{400, false, color, 1.0f};
    text_draw_info b{700, false, color, 1.0f};
    EXPECT_FALSE(a == b);
}

TEST(text_draw_info_test, not_equal_italic) {
    litehtml::web_color color{255, 0, 0, 255};
    text_draw_info a{400, false, color, 1.0f};
    text_draw_info b{400, true, color, 1.0f};
    EXPECT_FALSE(a == b);
}

TEST(text_draw_info_test, not_equal_color) {
    litehtml::web_color red{255, 0, 0, 255};
    litehtml::web_color blue{0, 0, 255, 255};
    text_draw_info a{400, false, red, 1.0f};
    text_draw_info b{400, false, blue, 1.0f};
    EXPECT_FALSE(a == b);
}

TEST(text_draw_info_test, not_equal_opacity) {
    litehtml::web_color color{255, 0, 0, 255};
    text_draw_info a{400, false, color, 1.0f};
    text_draw_info b{400, false, color, 0.5f};
    EXPECT_FALSE(a == b);
}

// ============================================================================
// shadow_info::operator< (via as_tuple)
// ============================================================================

TEST(shadow_info_test, less_than_by_color_red) {
    litehtml::web_color red{255, 0, 0, 255};
    litehtml::web_color green{0, 255, 0, 255};
    litehtml::position pos{0, 0, 100, 100};
    litehtml::border_radiuses br;
    br.top_left_x = br.top_right_x = br.bottom_left_x = br.bottom_right_x = 0;

    shadow_info a{red, 5.0f, 2.0f, 3.0f, 1.0f, false, pos, br, 1.0f};
    shadow_info b{green, 5.0f, 2.0f, 3.0f, 1.0f, false, pos, br, 1.0f};
    // red < green in tuple comparison (red.red 255 > green.red 0)
    EXPECT_TRUE(b < a);
    EXPECT_FALSE(a < b);
}

TEST(shadow_info_test, less_than_by_blur) {
    litehtml::web_color color{128, 128, 128, 255};
    litehtml::position pos{0, 0, 100, 100};
    litehtml::border_radiuses br{};

    shadow_info a{color, 5.0f, 2.0f, 3.0f, 1.0f, false, pos, br, 1.0f};
    shadow_info b{color, 10.0f, 2.0f, 3.0f, 1.0f, false, pos, br, 1.0f};
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(shadow_info_test, less_than_by_inset) {
    litehtml::web_color color{128, 128, 128, 255};
    litehtml::position pos{0, 0, 100, 100};
    litehtml::border_radiuses br{};

    shadow_info a{color, 5.0f, 2.0f, 3.0f, 1.0f, false, pos, br, 1.0f};
    shadow_info b{color, 5.0f, 2.0f, 3.0f, 1.0f, true, pos, br, 1.0f};
    // false < true in tuple comparison
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(shadow_info_test, equal_not_less) {
    litehtml::web_color color{128, 128, 128, 255};
    litehtml::position pos{0, 0, 100, 100};
    litehtml::border_radiuses br{};

    shadow_info a{color, 5.0f, 2.0f, 3.0f, 1.0f, false, pos, br, 1.0f};
    shadow_info b{color, 5.0f, 2.0f, 3.0f, 1.0f, false, pos, br, 1.0f};
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(b < a);
}

// ============================================================================
// text_shadow_info::operator==
// ============================================================================

TEST(text_shadow_info_test, equal_single_shadow) {
    litehtml::web_color shadow_color{0, 0, 0, 128};
    litehtml::web_color text_color{255, 0, 0, 255};
    litehtml::css_length bx, by, bblur, bspread;
    bx.set_value(2.0f, litehtml::css_units_px);
    by.set_value(3.0f, litehtml::css_units_px);
    bblur.set_value(5.0f, litehtml::css_units_px);
    bspread.set_value(1.0f, litehtml::css_units_px);

    litehtml::shadow s;
    s.color = shadow_color;
    s.x = bx;
    s.y = by;
    s.blur = bblur;
    s.spread = bspread;
    s.inset = false;

    litehtml::shadow_vector shadows = {s};
    text_shadow_info a{shadows, text_color, 1.0f};
    text_shadow_info b{shadows, text_color, 1.0f};
    EXPECT_TRUE(a == b);
}

TEST(text_shadow_info_test, not_equal_diff_shadow_count) {
    litehtml::web_color shadow_color{0, 0, 0, 128};
    litehtml::web_color text_color{255, 0, 0, 255};
    litehtml::css_length bx;
    bx.set_value(2.0f, litehtml::css_units_px);
    litehtml::css_length by;
    by.set_value(3.0f, litehtml::css_units_px);

    litehtml::shadow s;
    s.color = shadow_color;
    s.x = bx;
    s.y = by;
    s.inset = false;

    text_shadow_info a{{s}, text_color, 1.0f};
    text_shadow_info b{{}, text_color, 1.0f}; // empty shadows
    EXPECT_FALSE(a == b);
}

TEST(text_shadow_info_test, equal_no_shadows) {
    litehtml::web_color text_color{255, 0, 0, 255};
    text_shadow_info a{{}, text_color, 1.0f};
    text_shadow_info b{{}, text_color, 1.0f};
    EXPECT_TRUE(a == b);
}

TEST(text_shadow_info_test, not_equal_text_color) {
    litehtml::web_color shadow_color{0, 0, 0, 128};
    litehtml::web_color text_color_a{255, 0, 0, 255};
    litehtml::web_color text_color_b{0, 0, 255, 255};

    text_shadow_info a{{}, text_color_a, 1.0f};
    text_shadow_info b{{}, text_color_b, 1.0f};
    EXPECT_FALSE(a == b);
}

TEST(text_shadow_info_test, not_equal_opacity) {
    litehtml::web_color text_color{255, 0, 0, 255};
    text_shadow_info a{{}, text_color, 1.0f};
    text_shadow_info b{{}, text_color, 0.5f};
    EXPECT_FALSE(a == b);
}
