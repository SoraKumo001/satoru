#include <gtest/gtest.h>
#include "bridge/magic_tags.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "libs/litehtml/include/litehtml/types.h"

#include <vector>

// ============================================================================
// Standalone helper: isomorphic to container_skia::to_skia_blend_mode
// ============================================================================

static SkBlendMode to_skia_blend_mode(litehtml::blend_mode bm) {
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
// LayerTestCanvas — minimal RecordingCanvas for push_layer tests
// Records drawRect (with paint), saveLayer (with paint), and save() calls.
// ============================================================================

struct RecordedDrawRect {
    SkRect rect;
    SkPaint paint;
};

struct RecordedLayer {
    SkRect bounds;
    SkPaint paint;
    bool has_bounds = false;
    bool has_paint = false;
};

/// Bounded recording canvas using fixed-size arrays to work around
/// a TDM-GCC 9.2 std::vector bug at -O3 with certain element sizes.
static constexpr int kMaxRecordedDraws = 64;
static constexpr int kMaxRecordedLayers = 64;

class LayerTestCanvas : public SkCanvas {
public:
    RecordedDrawRect drawRects[kMaxRecordedDraws];
    RecordedLayer saveLayers[kMaxRecordedLayers];
    int drawRectCount = 0;
    int saveLayerCount = 0;
    int saveCount = 0;

    LayerTestCanvas() : drawRects{}, saveLayers{}, drawRectCount(0), saveLayerCount(0), saveCount(0) {}

    void drawRect(const SkRect& rect, const SkPaint& paint) override {
        if (drawRectCount < kMaxRecordedDraws) {
            drawRects[drawRectCount++] = {rect, paint};
        }
    }

    void saveLayer(const SkRect* bounds, const SkPaint* paint) override {
        if (saveLayerCount < kMaxRecordedLayers) {
            saveLayers[saveLayerCount++] = {
                bounds ? *bounds : SkRect(),
                paint ? *paint : SkPaint(),
                bounds != nullptr,
                paint != nullptr
            };
        }
    }

    void save() override { ++saveCount; }

    void clear() {
        drawRectCount = 0;
        saveLayerCount = 0;
        saveCount = 0;
    }
};

// ============================================================================
// Standalone helper: replicates container_skia::push_layer logic exactly
// (minus m_opacity_stack / flush side effects).
// ============================================================================

static void push_layer_to_canvas(
    LayerTestCanvas& canvas,
    float opacity,
    litehtml::blend_mode bm,
    bool tagging = false,
    const std::vector<litehtml::position>& clips = {},
    int width = 800,
    int height = 600)
{
    SkBlendMode sk_bm = to_skia_blend_mode(bm);

    if (tagging) {
        SkPaint p;
        if (bm == litehtml::blend_mode_normal) {
            p.setColor(make_magic_color(satoru::MagicTag::LayerPush,
                                        (int)(opacity * 255.0f)));
        } else {
            int packed = ((int)(opacity * 255.0f) << 4) | ((int)bm & 0x0F);
            p.setColor(make_magic_color(satoru::MagicTag::LayerPushBlend, packed));
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
    } else if (opacity < 1.0f || sk_bm != SkBlendMode::kSrcOver) {
        SkPaint paint;
        paint.setAlphaf(opacity);
        paint.setBlendMode(sk_bm);
        canvas.saveLayer(nullptr, &paint);
    } else {
        canvas.save();
    }
}

// ============================================================================
// Test Fixture
// ============================================================================

class ContainerLayerTest : public ::testing::Test {
protected:
    LayerTestCanvas canvas;
};

// ============================================================================
// 1. blend_mode → SkBlendMode mapping (15 blend modes + default)
// ============================================================================

TEST_F(ContainerLayerTest, BlendModeNormal) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_normal),
              SkBlendMode::kSrcOver);
}

TEST_F(ContainerLayerTest, BlendModeMultiply) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_multiply),
              SkBlendMode::kMultiply);
}

TEST_F(ContainerLayerTest, BlendModeScreen) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_screen),
              SkBlendMode::kScreen);
}

TEST_F(ContainerLayerTest, BlendModeOverlay) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_overlay),
              SkBlendMode::kOverlay);
}

TEST_F(ContainerLayerTest, BlendModeDarken) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_darken),
              SkBlendMode::kDarken);
}

TEST_F(ContainerLayerTest, BlendModeLighten) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_lighten),
              SkBlendMode::kLighten);
}

TEST_F(ContainerLayerTest, BlendModeColorDodge) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_color_dodge),
              SkBlendMode::kColorDodge);
}

TEST_F(ContainerLayerTest, BlendModeColorBurn) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_color_burn),
              SkBlendMode::kColorBurn);
}

TEST_F(ContainerLayerTest, BlendModeHardLight) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_hard_light),
              SkBlendMode::kHardLight);
}

TEST_F(ContainerLayerTest, BlendModeSoftLight) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_soft_light),
              SkBlendMode::kSoftLight);
}

TEST_F(ContainerLayerTest, BlendModeDifference) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_difference),
              SkBlendMode::kDifference);
}

TEST_F(ContainerLayerTest, BlendModeExclusion) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_exclusion),
              SkBlendMode::kExclusion);
}

TEST_F(ContainerLayerTest, BlendModeHue) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_hue),
              SkBlendMode::kHue);
}

TEST_F(ContainerLayerTest, BlendModeSaturation) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_saturation),
              SkBlendMode::kSaturation);
}

TEST_F(ContainerLayerTest, BlendModeColor) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_color),
              SkBlendMode::kColor);
}

TEST_F(ContainerLayerTest, BlendModeLuminosity) {
    EXPECT_EQ(to_skia_blend_mode(litehtml::blend_mode_luminosity),
              SkBlendMode::kLuminosity);
}

TEST_F(ContainerLayerTest, BlendModeDefault) {
    // Invalid / out-of-range values fall back to kSrcOver
    EXPECT_EQ(to_skia_blend_mode(static_cast<litehtml::blend_mode>(999)),
              SkBlendMode::kSrcOver);
}

// ============================================================================
// 2. Opacity tests (non-tagging)
// ============================================================================

TEST_F(ContainerLayerTest, OpacityFullNormal) {
    // opacity=1.0 + normal blend → save() only
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal);

    EXPECT_EQ(canvas.saveCount, 1);
    EXPECT_EQ(canvas.saveLayerCount, 0u);
    EXPECT_EQ(canvas.drawRectCount, 0u);
}

TEST_F(ContainerLayerTest, OpacityHalf) {
    // opacity=0.5 + normal blend → saveLayer with alphaf=0.5
    push_layer_to_canvas(canvas, 0.5f, litehtml::blend_mode_normal);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
    EXPECT_FALSE(canvas.saveLayers[0].has_bounds);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.5f);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kSrcOver);
}

TEST_F(ContainerLayerTest, OpacityZero) {
    // opacity=0.0 + normal blend → saveLayer with alphaf=0.0
    push_layer_to_canvas(canvas, 0.0f, litehtml::blend_mode_normal);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.0f);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kSrcOver);
}

TEST_F(ContainerLayerTest, OpacityAboveOne) {
    // opacity > 1.0 — since opacity >= 1.0f AND blend mode is kSrcOver,
    // the code takes the save() path (no saveLayer).
    // The code does NOT clamp opacity or create a layer for opacity > 1.0
    // with normal blend mode.
    push_layer_to_canvas(canvas, 2.0f, litehtml::blend_mode_normal);

    EXPECT_EQ(canvas.saveCount, 1);
    EXPECT_EQ(canvas.saveLayerCount, 0u);
}

TEST_F(ContainerLayerTest, OpacityBelowZero) {
    // opacity < 0.0 — code does NOT clamp
    push_layer_to_canvas(canvas, -0.5f, litehtml::blend_mode_normal);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), -0.5f);
}

TEST_F(ContainerLayerTest, OpacityJustBelowOne) {
    // opacity=0.999 → saveLayer (since < 1.0)
    push_layer_to_canvas(canvas, 0.999f, litehtml::blend_mode_normal);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
}

TEST_F(ContainerLayerTest, OpacityExactlyOne) {
    // opacity=1.0 → save() (no saveLayer needed)
    canvas.clear();
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal);

    EXPECT_EQ(canvas.saveCount, 1);
    EXPECT_EQ(canvas.saveLayerCount, 0u);
}

// ============================================================================
// 3. Blend mode tests (non-tagging, opacity=0.5 ensures saveLayer is used)
// ============================================================================

TEST_F(ContainerLayerTest, BlendModeNonNormalTriggersSaveLayer) {
    // Even with opacity=1.0, non-normal blend mode triggers saveLayer
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_multiply);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kMultiply);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 1.0f);
}

TEST_F(ContainerLayerTest, BlendModeMultiplyWithOpacity) {
    push_layer_to_canvas(canvas, 0.3f, litehtml::blend_mode_multiply);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kMultiply);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.3f);
}

TEST_F(ContainerLayerTest, BlendModeScreenWithOpacity) {
    push_layer_to_canvas(canvas, 0.7f, litehtml::blend_mode_screen);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kScreen);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.7f);
}

TEST_F(ContainerLayerTest, BlendModeOverlayWithOpacity) {
    push_layer_to_canvas(canvas, 0.4f, litehtml::blend_mode_overlay);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kOverlay);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.4f);
}

TEST_F(ContainerLayerTest, BlendModeDarkenWithOpacity) {
    push_layer_to_canvas(canvas, 0.6f, litehtml::blend_mode_darken);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kDarken);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.6f);
}

TEST_F(ContainerLayerTest, BlendModeLightenWithOpacity) {
    push_layer_to_canvas(canvas, 0.8f, litehtml::blend_mode_lighten);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kLighten);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.8f);
}

TEST_F(ContainerLayerTest, BlendModeColorDodgeWithOpacity) {
    push_layer_to_canvas(canvas, 0.25f, litehtml::blend_mode_color_dodge);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kColorDodge);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.25f);
}

TEST_F(ContainerLayerTest, BlendModeColorBurnWithOpacity) {
    push_layer_to_canvas(canvas, 0.75f, litehtml::blend_mode_color_burn);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kColorBurn);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.75f);
}

TEST_F(ContainerLayerTest, BlendModeHardLightWithOpacity) {
    push_layer_to_canvas(canvas, 0.5f, litehtml::blend_mode_hard_light);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kHardLight);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.5f);
}

TEST_F(ContainerLayerTest, BlendModeSoftLightWithOpacity) {
    push_layer_to_canvas(canvas, 0.1f, litehtml::blend_mode_soft_light);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kSoftLight);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.1f);
}

TEST_F(ContainerLayerTest, BlendModeDifferenceWithOpacity) {
    push_layer_to_canvas(canvas, 0.9f, litehtml::blend_mode_difference);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kDifference);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.9f);
}

TEST_F(ContainerLayerTest, BlendModeExclusionWithOpacity) {
    push_layer_to_canvas(canvas, 0.2f, litehtml::blend_mode_exclusion);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kExclusion);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.2f);
}

TEST_F(ContainerLayerTest, BlendModeHueWithOpacity) {
    push_layer_to_canvas(canvas, 0.35f, litehtml::blend_mode_hue);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kHue);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.35f);
}

TEST_F(ContainerLayerTest, BlendModeSaturationWithOpacity) {
    push_layer_to_canvas(canvas, 0.65f, litehtml::blend_mode_saturation);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kSaturation);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.65f);
}

TEST_F(ContainerLayerTest, BlendModeColorWithOpacity) {
    push_layer_to_canvas(canvas, 0.15f, litehtml::blend_mode_color);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kColor);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.15f);
}

TEST_F(ContainerLayerTest, BlendModeLuminosityWithOpacity) {
    push_layer_to_canvas(canvas, 0.85f, litehtml::blend_mode_luminosity);

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kLuminosity);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.85f);
}



// ============================================================================
// 4. Tagging mode tests
// ============================================================================

TEST_F(ContainerLayerTest, TaggingNormalBlendNoClip) {
    // tagging=true, normal blend → drawRect with LayerPush magic color
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal,
                         true /* tagging */);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    // Expected magic color: MagicTag::LayerPush (6), index = (int)(1.0 * 255) = 255
    // r = ((255>>8)&0x3F)<<2 = 0, g = 6, b = 255
    SkColor expected = make_magic_color(satoru::MagicTag::LayerPush, 255);
    EXPECT_EQ(canvas.drawRects[0].paint.getColor(), expected);
    // Full canvas rect
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 0);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 0);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 800);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 600);
}

TEST_F(ContainerLayerTest, TaggingBlendModeNoClip) {
    // tagging=true, non-normal blend → drawRect with LayerPushBlend magic color
    push_layer_to_canvas(canvas, 0.5f, litehtml::blend_mode_multiply,
                         true /* tagging */);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    // packed = (int)(0.5*255)<<4 | blend_mode_multiply(1) = (127<<4) | 1 = 2033
    int packed = (127 << 4) | 1;  // 2033
    SkColor expected = make_magic_color(satoru::MagicTag::LayerPushBlend, packed);
    EXPECT_EQ(canvas.drawRects[0].paint.getColor(), expected);
}

TEST_F(ContainerLayerTest, TaggingWithClip) {
    // tagging=true, with clips → drawRect uses clip bounds
    std::vector<litehtml::position> clips = {
        litehtml::position(10, 20, 300, 150)
    };
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal,
                         true /* tagging */, clips);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 10);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 20);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 310);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 170);
}

TEST_F(ContainerLayerTest, TaggingUsesLastClip) {
    // When multiple clips exist, uses the last one
    std::vector<litehtml::position> clips = {
        litehtml::position(0, 0, 100, 100),
        litehtml::position(50, 60, 200, 150)
    };
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal,
                         true /* tagging */, clips);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 50);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 60);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 250);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 210);
}

TEST_F(ContainerLayerTest, TaggingOpacityIndexEncoding) {
    // Verify opacity encoding in the magic color index
    // opacity=0.0 → index = 0
    push_layer_to_canvas(canvas, 0.0f, litehtml::blend_mode_normal,
                         true /* tagging */);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    SkColor expected0 = make_magic_color(satoru::MagicTag::LayerPush, 0);
    EXPECT_EQ(canvas.drawRects[0].paint.getColor(), expected0);

    // opacity=1.0 → index = 255
    canvas.clear();
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal,
                         true /* tagging */);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    SkColor expected1 = make_magic_color(satoru::MagicTag::LayerPush, 255);
    EXPECT_EQ(canvas.drawRects[0].paint.getColor(), expected1);
}

TEST_F(ContainerLayerTest, TaggingBlendPackedEncoding) {
    // Verify packed encoding of opacity + blend mode in LayerPushBlend
    // opacity=0.5 (127), blend_mode=screen(2)
    // packed = (127 << 4) | 2 = 2034
    push_layer_to_canvas(canvas, 0.5f, litehtml::blend_mode_screen,
                         true /* tagging */);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    int packed = (127 << 4) | 2;
    SkColor expected = make_magic_color(satoru::MagicTag::LayerPushBlend, packed);
    EXPECT_EQ(canvas.drawRects[0].paint.getColor(), expected);
}

TEST_F(ContainerLayerTest, TaggingCustomCanvasSize) {
    // Custom canvas dimensions
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal,
                         true /* tagging */, {}, 1024, 768);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 1024);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 768);
}

// ============================================================================
// 5. Save path — no layer needed
// ============================================================================

TEST_F(ContainerLayerTest, SavePathNoLayer) {
    // opacity=1.0, normal blend, no tagging → just save()
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal);

    EXPECT_EQ(canvas.saveCount, 1);
    EXPECT_EQ(canvas.saveLayerCount, 0u);
    EXPECT_EQ(canvas.drawRectCount, 0u);
}

TEST_F(ContainerLayerTest, MultipleCallsIndependent) {
    // Multiple push_layer calls should not interfere
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_normal);
    push_layer_to_canvas(canvas, 0.5f, litehtml::blend_mode_multiply);
    push_layer_to_canvas(canvas, 0.0f, litehtml::blend_mode_screen);

    EXPECT_EQ(canvas.saveCount, 1);   // first call → save()
    ASSERT_EQ(canvas.saveLayerCount, 2u);  // second + third → saveLayer

    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kMultiply);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.5f);

    EXPECT_EQ(canvas.saveLayers[1].paint.getBlendMode(), SkBlendMode::kScreen);
    EXPECT_FLOAT_EQ(canvas.saveLayers[1].paint.getAlphaf(), 0.0f);
}

// ============================================================================
// 6. Edge cases
// ============================================================================

TEST_F(ContainerLayerTest, InvalidBlendModeUsesSrcOver) {
    // An unrecognized blend_mode value should map to kSrcOver
    // With opacity=0.5 this triggers saveLayer with kSrcOver (just opacity)
    push_layer_to_canvas(canvas, 0.5f,
                         static_cast<litehtml::blend_mode>(999));

    ASSERT_EQ(canvas.saveLayerCount, 1u);
    EXPECT_EQ(canvas.saveLayers[0].paint.getBlendMode(), SkBlendMode::kSrcOver);
    EXPECT_FLOAT_EQ(canvas.saveLayers[0].paint.getAlphaf(), 0.5f);
}

TEST_F(ContainerLayerTest, InvalidBlendModeOpacityOne) {
    // opacity=1.0 + invalid blend → still maps to kSrcOver → save() only
    push_layer_to_canvas(canvas, 1.0f,
                         static_cast<litehtml::blend_mode>(999));

    EXPECT_EQ(canvas.saveCount, 1);
    EXPECT_EQ(canvas.saveLayerCount, 0u);
}

TEST_F(ContainerLayerTest, TaggingNoSideEffects) {
    // Tagging mode should NOT produce saveLayer or save calls
    push_layer_to_canvas(canvas, 1.0f, litehtml::blend_mode_multiply,
                         true /* tagging */);

    EXPECT_EQ(canvas.saveCount, 0);
    EXPECT_EQ(canvas.saveLayerCount, 0u);
    ASSERT_EQ(canvas.drawRectCount, 1u);
}

TEST_F(ContainerLayerTest, TaggingWithInvalidBlendMode) {
    // Tagging + invalid blend mode → uses LayerPushBlend with packed value
    // where bm bits are taken from the raw enum value
    push_layer_to_canvas(canvas, 0.5f,
                         static_cast<litehtml::blend_mode>(999),
                         true /* tagging */);

    ASSERT_EQ(canvas.drawRectCount, 1u);
    int packed = (127 << 4) | (999 & 0x0F);  // 0x0F masks to 7 (since 999 & 15 = 7)
    SkColor expected = make_magic_color(satoru::MagicTag::LayerPushBlend, packed);
    EXPECT_EQ(canvas.drawRects[0].paint.getColor(), expected);
}
