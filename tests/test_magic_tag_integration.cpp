// ============================================================================
// Integration tests for container_skia tagging mode
//
// Approach A: lightweight stubbing
//   - Standalone helper functions replicate the exact tagging-mode logic
//     from container_skia.cpp methods (draw_box_shadow, draw_linear_gradient,
//     draw_radial_gradient, draw_conic_gradient, draw_image, draw_border_image,
//     draw_list_marker, set_clip/del_clip, push_layer/pop_layer,
//     push_filter/pop_filter, push_backdrop_filter/pop_backdrop_filter,
//     push_clip_path/pop_clip_path, push_mask/pop_mask).
//   - RecordingCanvas captures drawRect/drawRRect calls with magic colors.
//   - Tests verify that the correct MagicTag/MagicTagExtended values,
//     indices, and bounding boxes are recorded.
//
// This approach avoids the full Skia and litehtml::document dependencies
// while thoroughly exercising the tagging-mode draw-call encoding logic.
// ============================================================================

#include <gtest/gtest.h>
#include "bridge/magic_tags.h"
#include "bridge/bridge_types.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRRect.h"

// LiteHTML types — use <litehtml.h> which includes document_container.h
// which transitively includes types.h (position, web_color, css_length),
// background.h (shadow, shadow_vector, background_layer),
// borders.h (border_radiuses), and document_container.h (list_marker).
#include <litehtml.h>

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

// ============================================================================
// RecordingCanvas — captures drawRect and drawRRect calls with paint
// ============================================================================

struct RecordedDrawRect {
    SkRect rect;
    SkPaint paint;
};

struct RecordedDrawRRect {
    SkRRect rrect;
    SkPaint paint;
};

static constexpr int kMaxDraws = 256;

class RecordCanvas : public SkCanvas {
public:
    RecordedDrawRect drawRects[kMaxDraws];
    RecordedDrawRRect drawRRects[kMaxDraws];
    int drawRectCount = 0;
    int drawRRectCount = 0;
    int saveCount = 0;
    int restoreCount = 0;

    RecordCanvas() : drawRects{}, drawRRects{}, drawRectCount(0), drawRRectCount(0),
                     saveCount(0), restoreCount(0) {}

    void drawRect(const SkRect& rect, const SkPaint& paint) override {
        if (drawRectCount < kMaxDraws) {
            drawRects[drawRectCount++] = {rect, paint};
        }
    }

    void drawRRect(const SkRRect& rrect, const SkPaint& paint) override {
        if (drawRRectCount < kMaxDraws) {
            drawRRects[drawRRectCount++] = {rrect, paint};
        }
    }

    void save() override { ++saveCount; }
    void restore() override { ++restoreCount; }

    void clear() {
        drawRectCount = 0;
        drawRRectCount = 0;
        saveCount = 0;
        restoreCount = 0;
    }
};

// ============================================================================
// Helper: replicate get_background_rrect from container_skia.cpp (lines 409-431)
// ============================================================================

static SkRRect helper_get_background_rrect(const litehtml::background_layer& layer) {
    litehtml::position intersect_box = layer.border_box.intersect(layer.clip_box);
    if (intersect_box.width <= 0 || intersect_box.height <= 0) {
        return SkRRect::MakeEmpty();
    }

    litehtml::border_radiuses rad = layer.border_radius;
    float offset_l = std::max(0.0f, (float)(intersect_box.x - layer.border_box.x));
    float offset_t = std::max(0.0f, (float)(intersect_box.y - layer.border_box.y));
    float offset_r = std::max(0.0f, (float)(layer.border_box.right() - intersect_box.right()));
    float offset_b = std::max(0.0f, (float)(layer.border_box.bottom() - intersect_box.bottom()));

    rad.top_left_x     = std::max(0.0f, rad.top_left_x - offset_l);
    rad.top_left_y     = std::max(0.0f, rad.top_left_y - offset_t);
    rad.top_right_x    = std::max(0.0f, rad.top_right_x - offset_r);
    rad.top_right_y    = std::max(0.0f, rad.top_right_y - offset_t);
    rad.bottom_right_x = std::max(0.0f, rad.bottom_right_x - offset_r);
    rad.bottom_right_y = std::max(0.0f, rad.bottom_right_y - offset_b);
    rad.bottom_left_x  = std::max(0.0f, rad.bottom_left_x - offset_l);
    rad.bottom_left_y  = std::max(0.0f, rad.bottom_left_y - offset_b);

    return SkRRect::MakeRect(
        SkRect::MakeXYWH((float)intersect_box.x, (float)intersect_box.y,
                         (float)intersect_box.width, (float)intersect_box.height));
}

// ============================================================================
// Helper: replicate make_rrect (container_skia.cpp pattern)
// ============================================================================

static SkRRect helper_make_rrect(const litehtml::position& pos,
                                  const litehtml::border_radiuses& /*radius*/) {
    return SkRRect::MakeRect(
        SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height));
}

// ============================================================================
// Helper: replicate container_skia::draw_box_shadow tagging mode (lines 791-811)
// ============================================================================

static void helper_draw_box_shadow_tagging(
    RecordCanvas& canvas,
    const litehtml::shadow_vector& shadows,
    const litehtml::position& pos,
    const litehtml::border_radiuses& radius,
    bool inset,
    int& shadowIndex)  // in/out: tracks m_usedShadows.size()
{
    for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
        const auto& s = *it;
        if (s.inset != inset) continue;
        shadowIndex++;
        int index = shadowIndex;
        SkPaint p;
        p.setColor(satoru::make_magic_color(satoru::MagicTag::Shadow, index));
        canvas.drawRRect(helper_make_rrect(pos, radius), p);
    }
}

// ============================================================================
// Helper: replicate container_skia::draw_linear_gradient tagging mode (lines 977-990)
// ============================================================================

static void helper_draw_linear_gradient_tagging(
    RecordCanvas& canvas,
    const litehtml::background_layer& layer,
    int& gradientIndex)
{
    gradientIndex++;
    int index = gradientIndex;
    SkPaint p;
    if (layer.is_text_clip) {
        p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::TextClipLinearGradient, index));
    } else {
        p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::LinearGradient, index));
    }
    canvas.drawRRect(helper_get_background_rrect(layer), p);
}

// ============================================================================
// Helper: replicate container_skia::draw_radial_gradient tagging mode (lines 1060-1069)
// ============================================================================

static void helper_draw_radial_gradient_tagging(
    RecordCanvas& canvas,
    const litehtml::background_layer& layer,
    int& gradientIndex)
{
    gradientIndex++;
    int index = gradientIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::RadialGradient, index));
    canvas.drawRRect(helper_get_background_rrect(layer), p);
}

// ============================================================================
// Helper: replicate container_skia::draw_conic_gradient tagging mode (lines 1133-1142)
// ============================================================================

static void helper_draw_conic_gradient_tagging(
    RecordCanvas& canvas,
    const litehtml::background_layer& layer,
    int& gradientIndex)
{
    gradientIndex++;
    int index = gradientIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::ConicGradient, index));
    canvas.drawRRect(helper_get_background_rrect(layer), p);
}

// ============================================================================
// Helper: replicate container_skia::draw_image tagging mode (lines 859-900)
// ============================================================================

static void helper_draw_image_tagging(
    RecordCanvas& canvas,
    const litehtml::background_layer& layer,
    const std::string& url,
    int& imageIndex)
{
    imageIndex++;
    int index = imageIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::ImageDraw, index));
    canvas.drawRect(
        SkRect::MakeXYWH((float)layer.clip_box.x, (float)layer.clip_box.y,
                         (float)layer.clip_box.width, (float)layer.clip_box.height),
        p);
}

// ============================================================================
// Helper: replicate container_skia::draw_list_marker tagging mode (lines 1702-1718)
// (image path only)
// ============================================================================

static void helper_draw_list_marker_image_tagging(
    RecordCanvas& canvas,
    const litehtml::list_marker& marker,
    int& imageIndex)
{
    // Only image-based markers get the tagging treatment
    imageIndex++;
    int index = imageIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::ImageDraw, index));
    SkRect dst = SkRect::MakeXYWH((float)marker.pos.x, (float)marker.pos.y,
                                   (float)marker.pos.width, (float)marker.pos.height);
    canvas.drawRect(dst, p);
}

// ============================================================================
// Helper: replicate container_skia::draw_border_image tagging mode (lines 1219-1233)
// ============================================================================

static void helper_draw_border_image_tagging(
    RecordCanvas& canvas,
    const litehtml::position& draw_pos,
    int& borderImageIndex)
{
    borderImageIndex++;
    int index = borderImageIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::BorderImage, index));
    canvas.drawRect(
        SkRect::MakeXYWH((float)draw_pos.x, (float)draw_pos.y,
                         (float)draw_pos.width, (float)draw_pos.height),
        p);
}

// ============================================================================
// Helper: replicate container_skia::set_clip tagging mode (lines 1799-1808)
// ============================================================================

static void helper_set_clip_tagging(
    RecordCanvas& canvas,
    int& clipIndex)
{
    clipIndex++;
    int index = clipIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::ClipPush, index));
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
}

// ============================================================================
// Helper: replicate container_skia::del_clip tagging mode (lines 1822-1825)
// ============================================================================

static void helper_del_clip_tagging(
    RecordCanvas& canvas)
{
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::ClipPop));
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
}

// ============================================================================
// Helper: replicate container_skia::push_layer tagging mode (lines 1941-1958)
// ============================================================================

static void helper_push_layer_tagging(
    RecordCanvas& canvas,
    float opacity,
    litehtml::blend_mode bm,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    SkPaint p;
    if (bm == litehtml::blend_mode_normal) {
        p.setColor(satoru::make_magic_color(satoru::MagicTag::LayerPush,
                                            (int)(opacity * 255.0f)));
    } else {
        int packed = ((int)(opacity * 255.0f) << 4) | ((int)bm & 0x0F);
        p.setColor(satoru::make_magic_color(satoru::MagicTag::LayerPushBlend, packed));
    }
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Helper: replicate container_skia::pop_layer tagging mode (lines 1977-1988)
// ============================================================================

static void helper_pop_layer_tagging(
    RecordCanvas& canvas,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::LayerPop));
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Helper: replicate container_skia::push_filter tagging mode (lines 2103-2124)
// ============================================================================

static void helper_push_filter_tagging(
    RecordCanvas& canvas,
    int& filterIndex,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    filterIndex++;
    int index = filterIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::FilterPush, index));
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Helper: replicate container_skia::pop_filter tagging mode (lines 2195-2206)
// ============================================================================

static void helper_pop_filter_tagging(
    RecordCanvas& canvas,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::FilterPop));
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Helper: replicate container_skia::push_backdrop_filter tagging mode (lines 121-146)
// ============================================================================

static void helper_push_backdrop_filter_tagging(
    RecordCanvas& canvas,
    int& backdropFilterIndex)
{
    backdropFilterIndex++;
    int index = backdropFilterIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::BackdropFilterPush, index));
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
}

// ============================================================================
// Helper: replicate container_skia::pop_backdrop_filter tagging mode (lines 376-380)
// ============================================================================

static void helper_pop_backdrop_filter_tagging(
    RecordCanvas& canvas)
{
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::BackdropFilterPop));
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
}

// ============================================================================
// Helper: replicate container_skia::push_clip_path tagging mode (lines 2571-2592)
// ============================================================================

static void helper_push_clip_path_tagging(
    RecordCanvas& canvas,
    int& clipPathIndex,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    clipPathIndex++;
    int index = clipPathIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::ClipPathPush, index));
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Helper: replicate container_skia::pop_clip_path tagging mode (lines 2612-2623)
// ============================================================================

static void helper_pop_clip_path_tagging(
    RecordCanvas& canvas,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::ClipPathPop));
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Helper: replicate container_skia::push_mask tagging mode (lines 2218-2238)
// ============================================================================

static void helper_push_mask_tagging(
    RecordCanvas& canvas,
    int& maskIndex,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    maskIndex++;
    int index = maskIndex;
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::MaskPush, index));
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Helper: replicate container_skia::pop_mask tagging mode (lines 2254-2265)
// ============================================================================

static void helper_pop_mask_tagging(
    RecordCanvas& canvas,
    const std::vector<litehtml::position>& clips,
    int width,
    int height)
{
    SkPaint p;
    p.setColor(satoru::make_magic_color(satoru::MagicTag::MaskPop));
    SkRect rect;
    if (!clips.empty()) {
        auto& pos = clips.back();
        rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y,
                                (float)pos.width, (float)pos.height);
    } else {
        rect = SkRect::MakeWH((float)width, (float)height);
    }
    canvas.drawRect(rect, p);
}

// ============================================================================
// Test Fixture
// ============================================================================

class MagicTagIntegrationTest : public ::testing::Test {
protected:
    RecordCanvas canvas;

    void SetUp() override {
        canvas.clear();
    }

    // Helper: decode paint color from a recorded drawRect
    satoru::DecodedMagicTag decodeDrawRectColor(int drawIndex) const {
        if (drawIndex >= canvas.drawRectCount) return {};
        SkColor c = canvas.drawRects[drawIndex].paint.getColor();
        return satoru::decode_magic_color(
            (uint8_t)((c >> 16) & 0xFF),
            (uint8_t)((c >> 8) & 0xFF),
            (uint8_t)(c & 0xFF));
    }

    // Helper: decode paint color from a recorded drawRRect
    satoru::DecodedMagicTag decodeDrawRRectColor(int drawIndex) const {
        if (drawIndex >= canvas.drawRRectCount) return {};
        SkColor c = canvas.drawRRects[drawIndex].paint.getColor();
        return satoru::decode_magic_color(
            (uint8_t)((c >> 16) & 0xFF),
            (uint8_t)((c >> 8) & 0xFF),
            (uint8_t)(c & 0xFF));
    }
};

// ============================================================================
// 1. Box Shadow
// ============================================================================

TEST_F(MagicTagIntegrationTest, BoxShadowTagging) {
    litehtml::position pos(10, 20, 200, 100);
    litehtml::border_radiuses radius;

    litehtml::shadow shadow;
    shadow.color = litehtml::web_color(0, 0, 0, 128);
    shadow.blur = litehtml::css_length::predef_value(5);
    shadow.x = litehtml::css_length::predef_value(3);
    shadow.y = litehtml::css_length::predef_value(4);
    shadow.spread = litehtml::css_length::predef_value(1);
    shadow.inset = false;

    litehtml::shadow_vector shadows = {shadow};
    int shadowIndex = 0;

    helper_draw_box_shadow_tagging(canvas, shadows, pos, radius, false, shadowIndex);

    ASSERT_EQ(canvas.drawRRectCount, 1);
    ASSERT_EQ(canvas.drawRectCount, 0);

    auto decoded = decodeDrawRRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_FALSE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTag::Shadow));
    EXPECT_EQ(decoded.index, 1);
}

TEST_F(MagicTagIntegrationTest, BoxShadowMultipleTagging) {
    litehtml::position pos(0, 0, 100, 100);
    litehtml::border_radiuses radius;

    litehtml::shadow s1, s2;
    s1.inset = false; s1.blur = s1.x = s1.y = s1.spread = litehtml::css_length::predef_value(2);
    s1.color = litehtml::web_color(255, 0, 0);
    s2.inset = false; s2.blur = s2.x = s2.y = s2.spread = litehtml::css_length::predef_value(4);
    s2.color = litehtml::web_color(0, 255, 0);

    litehtml::shadow_vector shadows = {s1, s2};
    int shadowIndex = 0;
    helper_draw_box_shadow_tagging(canvas, shadows, pos, radius, false, shadowIndex);

    ASSERT_EQ(canvas.drawRRectCount, 2);

    auto d1 = decodeDrawRRectColor(0);
    EXPECT_EQ(d1.tag_value, static_cast<int>(satoru::MagicTag::Shadow));
    EXPECT_EQ(d1.index, 1);

    auto d2 = decodeDrawRRectColor(1);
    EXPECT_EQ(d2.tag_value, static_cast<int>(satoru::MagicTag::Shadow));
    EXPECT_EQ(d2.index, 2);
}

// ============================================================================
// 2. Linear Gradient
// ============================================================================

TEST_F(MagicTagIntegrationTest, LinearGradientTagging) {
    litehtml::background_layer layer;
    layer.border_box = litehtml::position(10, 10, 200, 100);
    layer.clip_box = layer.border_box;
    layer.origin_box = layer.border_box;
    layer.is_text_clip = false;

    int gradientIndex = 0;
    helper_draw_linear_gradient_tagging(canvas, layer, gradientIndex);

    ASSERT_EQ(canvas.drawRRectCount, 1);
    auto decoded = decodeDrawRRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_TRUE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTagExtended::LinearGradient));
    EXPECT_EQ(decoded.index, 1);
}

TEST_F(MagicTagIntegrationTest, LinearGradientTextClipTagging) {
    litehtml::background_layer layer;
    layer.border_box = litehtml::position(0, 0, 100, 50);
    layer.clip_box = layer.border_box;
    layer.origin_box = layer.border_box;
    layer.is_text_clip = true;

    int gradientIndex = 0;
    helper_draw_linear_gradient_tagging(canvas, layer, gradientIndex);

    ASSERT_EQ(canvas.drawRRectCount, 1);
    auto decoded = decodeDrawRRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_TRUE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTagExtended::TextClipLinearGradient));
    EXPECT_EQ(decoded.index, 1);
}

// ============================================================================
// 3. Radial Gradient
// ============================================================================

TEST_F(MagicTagIntegrationTest, RadialGradientTagging) {
    litehtml::background_layer layer;
    layer.border_box = litehtml::position(5, 5, 150, 80);
    layer.clip_box = layer.border_box;
    layer.origin_box = layer.border_box;

    int gradientIndex = 0;
    helper_draw_radial_gradient_tagging(canvas, layer, gradientIndex);

    ASSERT_EQ(canvas.drawRRectCount, 1);
    auto decoded = decodeDrawRRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_TRUE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTagExtended::RadialGradient));
    EXPECT_EQ(decoded.index, 1);
}

// ============================================================================
// 4. Conic Gradient
// ============================================================================

TEST_F(MagicTagIntegrationTest, ConicGradientTagging) {
    litehtml::background_layer layer;
    layer.border_box = litehtml::position(0, 0, 300, 200);
    layer.clip_box = layer.border_box;
    layer.origin_box = layer.border_box;

    int gradientIndex = 0;
    helper_draw_conic_gradient_tagging(canvas, layer, gradientIndex);

    ASSERT_EQ(canvas.drawRRectCount, 1);
    auto decoded = decodeDrawRRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_TRUE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTagExtended::ConicGradient));
    EXPECT_EQ(decoded.index, 1);
}

// ============================================================================
// 5. Image Draw
// ============================================================================

TEST_F(MagicTagIntegrationTest, ImageDrawTagging) {
    litehtml::background_layer layer;
    layer.border_box = litehtml::position(0, 0, 100, 100);
    layer.clip_box = litehtml::position(5, 5, 90, 90);
    layer.origin_box = litehtml::position(0, 0, 100, 100);

    int imageIndex = 0;
    helper_draw_image_tagging(canvas, layer, "test.png", imageIndex);

    ASSERT_EQ(canvas.drawRectCount, 1);
    auto decoded = decodeDrawRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_TRUE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTagExtended::ImageDraw));
    EXPECT_EQ(decoded.index, 1);

    // Verify rect matches clip_box
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 5);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 5);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 95);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 95);
}

// ============================================================================
// 6. List Marker (image path)
// ============================================================================

TEST_F(MagicTagIntegrationTest, ListMarkerImageTagging) {
    litehtml::list_marker marker;
    marker.image = "bullet.png";
    marker.pos = litehtml::position(10, 20, 16, 16);
    marker.color = litehtml::web_color(0, 0, 0);

    int imageIndex = 0;
    helper_draw_list_marker_image_tagging(canvas, marker, imageIndex);

    ASSERT_EQ(canvas.drawRectCount, 1);
    auto decoded = decodeDrawRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_TRUE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTagExtended::ImageDraw));
    EXPECT_EQ(decoded.index, 1);

    // Verify rect matches marker position
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 10);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 20);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 26);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 36);
}

// ============================================================================
// 7. Border Image
// ============================================================================

TEST_F(MagicTagIntegrationTest, BorderImageTagging) {
    litehtml::position draw_pos(10, 10, 200, 100);
    int borderImageIndex = 0;
    helper_draw_border_image_tagging(canvas, draw_pos, borderImageIndex);

    ASSERT_EQ(canvas.drawRectCount, 1);
    auto decoded = decodeDrawRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_TRUE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTagExtended::BorderImage));
    EXPECT_EQ(decoded.index, 1);

    // Verify rect matches draw_pos
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 10);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 10);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 210);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 110);
}

// ============================================================================
// 8. Clip push/pop
// ============================================================================

TEST_F(MagicTagIntegrationTest, ClipPushPopTagging) {
    int clipIndex = 0;
    helper_set_clip_tagging(canvas, clipIndex);
    helper_del_clip_tagging(canvas);

    ASSERT_EQ(canvas.drawRectCount, 2);

    // Push
    auto pushDecoded = decodeDrawRectColor(0);
    EXPECT_TRUE(pushDecoded.is_magic);
    EXPECT_FALSE(pushDecoded.is_extended);
    EXPECT_EQ(pushDecoded.tag_value, static_cast<int>(satoru::MagicTag::ClipPush));
    EXPECT_EQ(pushDecoded.index, 1);

    // Verify tiny rect
    EXPECT_FLOAT_EQ(canvas.drawRects[0].rect.fRight - canvas.drawRects[0].rect.fLeft, 0.001f);
    EXPECT_FLOAT_EQ(canvas.drawRects[0].rect.fBottom - canvas.drawRects[0].rect.fTop, 0.001f);

    // Pop
    auto popDecoded = decodeDrawRectColor(1);
    EXPECT_TRUE(popDecoded.is_magic);
    EXPECT_FALSE(popDecoded.is_extended);
    EXPECT_EQ(popDecoded.tag_value, static_cast<int>(satoru::MagicTag::ClipPop));
    EXPECT_EQ(popDecoded.index, 0);  // ClipPop has no index
}

// ============================================================================
// 9. Layer push/pop
// ============================================================================

TEST_F(MagicTagIntegrationTest, LayerPushPopTagging) {
    std::vector<litehtml::position> clips;
    helper_push_layer_tagging(canvas, 1.0f, litehtml::blend_mode_normal, clips, 800, 600);
    helper_pop_layer_tagging(canvas, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 2);

    // Push: LayerPush magic color with opacity encoded as index
    auto pushDecoded = decodeDrawRectColor(0);
    EXPECT_TRUE(pushDecoded.is_magic);
    EXPECT_FALSE(pushDecoded.is_extended);
    EXPECT_EQ(pushDecoded.tag_value, static_cast<int>(satoru::MagicTag::LayerPush));
    EXPECT_EQ(pushDecoded.index, 255);  // (int)(1.0 * 255) = 255

    // Full canvas rect
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 0);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 0);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 800);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 600);

    // Pop: LayerPop magic color
    auto popDecoded = decodeDrawRectColor(1);
    EXPECT_TRUE(popDecoded.is_magic);
    EXPECT_FALSE(popDecoded.is_extended);
    EXPECT_EQ(popDecoded.tag_value, static_cast<int>(satoru::MagicTag::LayerPop));
}

TEST_F(MagicTagIntegrationTest, LayerPushBlendModeTagging) {
    std::vector<litehtml::position> clips;
    helper_push_layer_tagging(canvas, 0.5f, litehtml::blend_mode_multiply, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 1);
    auto decoded = decodeDrawRectColor(0);
    EXPECT_TRUE(decoded.is_magic);
    EXPECT_FALSE(decoded.is_extended);
    EXPECT_EQ(decoded.tag_value, static_cast<int>(satoru::MagicTag::LayerPushBlend));

    // packed = (int)(0.5 * 255) << 4 | blend_mode_multiply(1) = (127 << 4) | 1 = 2033
    EXPECT_EQ(decoded.index, 2033);
}

TEST_F(MagicTagIntegrationTest, LayerPushWithClipsTagging) {
    std::vector<litehtml::position> clips = {
        litehtml::position(10, 20, 300, 150)
    };
    helper_push_layer_tagging(canvas, 1.0f, litehtml::blend_mode_normal, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 1);
    // Rect should match clip bounds
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 10);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 20);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 310);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 170);
}

// ============================================================================
// 10. Filter push/pop
// ============================================================================

TEST_F(MagicTagIntegrationTest, FilterPushPopTagging) {
    std::vector<litehtml::position> clips;
    int filterIndex = 0;
    helper_push_filter_tagging(canvas, filterIndex, clips, 800, 600);
    helper_pop_filter_tagging(canvas, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 2);

    auto pushDecoded = decodeDrawRectColor(0);
    EXPECT_TRUE(pushDecoded.is_magic);
    EXPECT_FALSE(pushDecoded.is_extended);
    EXPECT_EQ(pushDecoded.tag_value, static_cast<int>(satoru::MagicTag::FilterPush));
    EXPECT_EQ(pushDecoded.index, 1);

    auto popDecoded = decodeDrawRectColor(1);
    EXPECT_TRUE(popDecoded.is_magic);
    EXPECT_FALSE(popDecoded.is_extended);
    EXPECT_EQ(popDecoded.tag_value, static_cast<int>(satoru::MagicTag::FilterPop));
}

TEST_F(MagicTagIntegrationTest, FilterPushWithClipsTagging) {
    std::vector<litehtml::position> clips = {
        litehtml::position(30, 40, 250, 180)
    };
    int filterIndex = 0;
    helper_push_filter_tagging(canvas, filterIndex, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 1);
    // Rect should match clip bounds
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 30);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 40);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 280);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 220);
}

TEST_F(MagicTagIntegrationTest, FilterPushMultipleTagging) {
    std::vector<litehtml::position> clips;
    int filterIndex = 0;
    helper_push_filter_tagging(canvas, filterIndex, clips, 800, 600);
    helper_push_filter_tagging(canvas, filterIndex, clips, 800, 600);
    helper_push_filter_tagging(canvas, filterIndex, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 3);
    EXPECT_EQ(decodeDrawRectColor(0).index, 1);
    EXPECT_EQ(decodeDrawRectColor(1).index, 2);
    EXPECT_EQ(decodeDrawRectColor(2).index, 3);
}

// ============================================================================
// 11. Backdrop Filter push/pop
// ============================================================================

TEST_F(MagicTagIntegrationTest, BackdropFilterPushPopTagging) {
    int backdropFilterIndex = 0;
    helper_push_backdrop_filter_tagging(canvas, backdropFilterIndex);
    helper_pop_backdrop_filter_tagging(canvas);

    ASSERT_EQ(canvas.drawRectCount, 2);

    auto pushDecoded = decodeDrawRectColor(0);
    EXPECT_TRUE(pushDecoded.is_magic);
    EXPECT_FALSE(pushDecoded.is_extended);
    EXPECT_EQ(pushDecoded.tag_value, static_cast<int>(satoru::MagicTag::BackdropFilterPush));
    EXPECT_EQ(pushDecoded.index, 1);

    auto popDecoded = decodeDrawRectColor(1);
    EXPECT_TRUE(popDecoded.is_magic);
    EXPECT_FALSE(popDecoded.is_extended);
    EXPECT_EQ(popDecoded.tag_value, static_cast<int>(satoru::MagicTag::BackdropFilterPop));
}

// ============================================================================
// 12. Clip-path push/pop
// ============================================================================

TEST_F(MagicTagIntegrationTest, ClipPathPushPopTagging) {
    std::vector<litehtml::position> clips;
    int clipPathIndex = 0;
    helper_push_clip_path_tagging(canvas, clipPathIndex, clips, 800, 600);
    helper_pop_clip_path_tagging(canvas, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 2);

    auto pushDecoded = decodeDrawRectColor(0);
    EXPECT_TRUE(pushDecoded.is_magic);
    EXPECT_FALSE(pushDecoded.is_extended);
    EXPECT_EQ(pushDecoded.tag_value, static_cast<int>(satoru::MagicTag::ClipPathPush));
    EXPECT_EQ(pushDecoded.index, 1);

    auto popDecoded = decodeDrawRectColor(1);
    EXPECT_TRUE(popDecoded.is_magic);
    EXPECT_FALSE(popDecoded.is_extended);
    EXPECT_EQ(popDecoded.tag_value, static_cast<int>(satoru::MagicTag::ClipPathPop));
}

TEST_F(MagicTagIntegrationTest, ClipPathPushWithClipsTagging) {
    std::vector<litehtml::position> clips = {
        litehtml::position(50, 60, 150, 200)
    };
    int clipPathIndex = 0;
    helper_push_clip_path_tagging(canvas, clipPathIndex, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 1);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 50);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 60);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 200);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 260);
}

// ============================================================================
// 13. Mask push/pop
// ============================================================================

TEST_F(MagicTagIntegrationTest, MaskPushPopTagging) {
    std::vector<litehtml::position> clips;
    int maskIndex = 0;
    helper_push_mask_tagging(canvas, maskIndex, clips, 800, 600);
    helper_pop_mask_tagging(canvas, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 2);

    auto pushDecoded = decodeDrawRectColor(0);
    EXPECT_TRUE(pushDecoded.is_magic);
    EXPECT_FALSE(pushDecoded.is_extended);
    EXPECT_EQ(pushDecoded.tag_value, static_cast<int>(satoru::MagicTag::MaskPush));
    EXPECT_EQ(pushDecoded.index, 1);

    auto popDecoded = decodeDrawRectColor(1);
    EXPECT_TRUE(popDecoded.is_magic);
    EXPECT_FALSE(popDecoded.is_extended);
    EXPECT_EQ(popDecoded.tag_value, static_cast<int>(satoru::MagicTag::MaskPop));
}

// ============================================================================
// 14. Sequential mixed operations — simulate a real render flow
// ============================================================================

TEST_F(MagicTagIntegrationTest, SequentialMixedOperations) {
    // Simulate a realistic sequence of tagging calls as they would happen
    // during rendering of a complex HTML element.
    std::vector<litehtml::position> clips;
    int shadowIndex = 0;
    int gradientIndex = 0;
    int imageIndex = 0;
    int borderImageIndex = 0;
    int filterIndex = 0;
    int clipPathIndex = 0;
    int maskIndex = 0;
    int backdropFilterIndex = 0;
    int clipIndex = 0;

    // 1. Push a layer (opacity)
    helper_push_layer_tagging(canvas, 0.8f, litehtml::blend_mode_normal, clips, 800, 600);

    // 2. Set clip
    clips.push_back(litehtml::position(10, 10, 200, 100));
    helper_set_clip_tagging(canvas, clipIndex);

    // 3. Draw a box shadow
    litehtml::shadow shadow;
    shadow.color = litehtml::web_color(0, 0, 0, 100);
    shadow.blur = litehtml::css_length::predef_value(5);
    shadow.x = litehtml::css_length::predef_value(2);
    shadow.y = litehtml::css_length::predef_value(2);
    shadow.spread = litehtml::css_length::predef_value(0);
    shadow.inset = false;
    litehtml::shadow_vector shadows = {shadow};
    litehtml::border_radiuses radius;
    helper_draw_box_shadow_tagging(canvas, shadows,
                                    litehtml::position(10, 10, 200, 100),
                                    radius, false, shadowIndex);

    // 4. Draw a linear gradient background
    litehtml::background_layer layer;
    layer.border_box = litehtml::position(10, 10, 200, 100);
    layer.clip_box = layer.border_box;
    layer.origin_box = layer.border_box;
    layer.is_text_clip = false;
    helper_draw_linear_gradient_tagging(canvas, layer, gradientIndex);

    // 5. Draw an image
    helper_draw_image_tagging(canvas, layer, "bg.png", imageIndex);

    // 6. Pop clip
    helper_del_clip_tagging(canvas);
    clips.pop_back();

    // 7. Pop layer
    helper_pop_layer_tagging(canvas, clips, 800, 600);

    // Total: 7 draw calls (1 layer-push + 1 clip-push + 1 shadow + 1 gradient + 1 image + 1 clip-pop + 1 layer-pop)
    int totalDraws = canvas.drawRectCount + canvas.drawRRectCount;
    EXPECT_EQ(totalDraws, 7);

    // Verify drawRect calls (1 layer-push, 1 clip-push, 1 image, 1 clip-pop, 1 layer-pop = 5 drawRect)
    ASSERT_EQ(canvas.drawRectCount, 5);

    // drawRect[0]: LayerPush
    EXPECT_EQ(decodeDrawRectColor(0).tag_value, static_cast<int>(satoru::MagicTag::LayerPush));
    EXPECT_EQ(decodeDrawRectColor(0).index, (int)(0.8f * 255.0f));

    // drawRect[1]: ClipPush
    EXPECT_EQ(decodeDrawRectColor(1).tag_value, static_cast<int>(satoru::MagicTag::ClipPush));
    EXPECT_EQ(decodeDrawRectColor(1).index, 1);

    // drawRect[2]: ImageDraw
    EXPECT_EQ(decodeDrawRectColor(2).tag_value, static_cast<int>(satoru::MagicTagExtended::ImageDraw));
    EXPECT_EQ(decodeDrawRectColor(2).index, 1);

    // drawRect[3]: ClipPop
    EXPECT_EQ(decodeDrawRectColor(3).tag_value, static_cast<int>(satoru::MagicTag::ClipPop));

    // drawRect[4]: LayerPop
    EXPECT_EQ(decodeDrawRectColor(4).tag_value, static_cast<int>(satoru::MagicTag::LayerPop));

    // verify drawRRect calls (1 shadow, 1 gradient = 2 drawRRect)
    ASSERT_EQ(canvas.drawRRectCount, 2);
    EXPECT_EQ(decodeDrawRRectColor(0).tag_value, static_cast<int>(satoru::MagicTag::Shadow));
    EXPECT_EQ(decodeDrawRRectColor(0).index, 1);
    EXPECT_EQ(decodeDrawRRectColor(1).tag_value, static_cast<int>(satoru::MagicTagExtended::LinearGradient));
    EXPECT_EQ(decodeDrawRRectColor(1).index, 1);
}

// ============================================================================
// 15. Index continuity across calls
// ============================================================================

TEST_F(MagicTagIntegrationTest, IndexContinuityAcrossGradients) {
    int gradientIndex = 0;
    litehtml::background_layer layer;
    layer.border_box = litehtml::position(0, 0, 100, 50);
    layer.clip_box = layer.border_box;
    layer.origin_box = layer.border_box;

    // Three linear gradients in sequence
    helper_draw_linear_gradient_tagging(canvas, layer, gradientIndex);
    helper_draw_radial_gradient_tagging(canvas, layer, gradientIndex);
    helper_draw_conic_gradient_tagging(canvas, layer, gradientIndex);

    ASSERT_EQ(canvas.drawRRectCount, 3);
    // Linear gradient: index 1 (linear uses gradientIndex=1)
    EXPECT_EQ(decodeDrawRRectColor(0).tag_value, static_cast<int>(satoru::MagicTagExtended::LinearGradient));
    EXPECT_EQ(decodeDrawRRectColor(0).index, 1);
    // Radial: index 2 (radial uses gradientIndex=2)
    EXPECT_EQ(decodeDrawRRectColor(1).tag_value, static_cast<int>(satoru::MagicTagExtended::RadialGradient));
    EXPECT_EQ(decodeDrawRRectColor(1).index, 2);
    // Conic: index 3 (conic uses gradientIndex=3)
    EXPECT_EQ(decodeDrawRRectColor(2).tag_value, static_cast<int>(satoru::MagicTagExtended::ConicGradient));
    EXPECT_EQ(decodeDrawRRectColor(2).index, 3);
}

// ============================================================================
// 16. No side effects in tagging mode
// ============================================================================

TEST_F(MagicTagIntegrationTest, TaggingNoSaveRestore) {
    // Tagging mode should produce NO save/restore calls on the canvas
    std::vector<litehtml::position> clips;

    helper_push_layer_tagging(canvas, 1.0f, litehtml::blend_mode_normal, clips, 800, 600);
    helper_pop_layer_tagging(canvas, clips, 800, 600);
    int idx = 0;
    helper_set_clip_tagging(canvas, idx);
    helper_del_clip_tagging(canvas);

    EXPECT_EQ(canvas.saveCount, 0);
    EXPECT_EQ(canvas.restoreCount, 0);
    EXPECT_GT(canvas.drawRectCount, 0);
}

// ============================================================================
// 17. Clip-path and mask push/pop with clip tracking
// ============================================================================

TEST_F(MagicTagIntegrationTest, ClipPathAndMaskAlternating) {
    std::vector<litehtml::position> clips;
    int clipPathIndex = 0;
    int maskIndex = 0;

    // Clip-path push
    helper_push_clip_path_tagging(canvas, clipPathIndex, clips, 800, 600);
    // Mask push
    helper_push_mask_tagging(canvas, maskIndex, clips, 800, 600);
    // Mask pop
    helper_pop_mask_tagging(canvas, clips, 800, 600);
    // Clip-path pop
    helper_pop_clip_path_tagging(canvas, clips, 800, 600);

    ASSERT_EQ(canvas.drawRectCount, 4);

    EXPECT_EQ(decodeDrawRectColor(0).tag_value, static_cast<int>(satoru::MagicTag::ClipPathPush));
    EXPECT_EQ(decodeDrawRectColor(0).index, 1);

    EXPECT_EQ(decodeDrawRectColor(1).tag_value, static_cast<int>(satoru::MagicTag::MaskPush));
    EXPECT_EQ(decodeDrawRectColor(1).index, 1);

    EXPECT_EQ(decodeDrawRectColor(2).tag_value, static_cast<int>(satoru::MagicTag::MaskPop));

    EXPECT_EQ(decodeDrawRectColor(3).tag_value, static_cast<int>(satoru::MagicTag::ClipPathPop));
}
