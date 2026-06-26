#include <gtest/gtest.h>
#include "core/logical_canvas.h"
#include "core/logical_geometry.h"

// ──────────────────────────────────────────────
// Wave 6: LogicalCanvas Tests
//
// LogicalCanvas wraps SkCanvas with writing-mode-aware
// coordinate conversion. We use a RecordingCanvas to
// verify that logical positions are correctly translated
// to physical positions before being passed to SkCanvas.
// ──────────────────────────────────────────────

using namespace litehtml;
using namespace satoru;

// ── Recording SkCanvas test double ───────────

struct RecordedDrawRect {
    SkRect rect;
};

struct RecordedDrawLine {
    float x1, y1, x2, y2;
};

struct RecordedClipRect {
    SkRect rect;
    SkClipOp op;
    bool antialias;
};

class RecordingCanvas : public SkCanvas {
public:
    std::vector<RecordedDrawRect> drawRects;
    std::vector<RecordedDrawLine> drawLines;
    std::vector<RecordedClipRect> clipRects;
    int saveCount = 0;
    int restoreCount = 0;

    void drawRect(const SkRect& rect, const SkPaint&) override {
        drawRects.push_back({rect});
    }

    void drawLine(float x1, float y1, float x2, float y2, const SkPaint&) override {
        drawLines.push_back({x1, y1, x2, y2});
    }

    void clipRect(const SkRect& rect, SkClipOp op, bool antialias) override {
        clipRects.push_back({rect, op, antialias});
    }

    void save() override { ++saveCount; }
    void restore() override { ++restoreCount; }
};

// ── test fixtures ────────────────────────────

class LogicalCanvasTest : public ::testing::Test {
protected:
    RecordingCanvas canvas;
    SkPaint paint;  // unused in recording stub
};

// ── drawRect tests ───────────────────────────

TEST_F(LogicalCanvasTest, DrawRectHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    lc.drawRect(logical_pos(100, 200), logical_size(300, 150), paint);

    ASSERT_EQ(canvas.drawRects.size(), 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 100);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 200);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 400);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 350);
}

TEST_F(LogicalCanvasTest, DrawRectVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    // logical_pos(100, 200) → inline=100, block=200
    // logical_size(300, 150) → inline=300, block=150
    // vertical-rl: x = 800 - 200 - 150 = 450, y = 100, w = 150, h = 300
    lc.drawRect(logical_pos(100, 200), logical_size(300, 150), paint);

    ASSERT_EQ(canvas.drawRects.size(), 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 450);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 100);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 600);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 400);
}

TEST_F(LogicalCanvasTest, DrawRectVerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    // vertical-lr: x = 200, y = 100, w = 150, h = 300
    lc.drawRect(logical_pos(100, 200), logical_size(300, 150), paint);

    ASSERT_EQ(canvas.drawRects.size(), 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 200);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 100);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 350);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 400);
}

TEST_F(LogicalCanvasTest, DrawRectWithOffset) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm, 50, 30);

    lc.drawRect(logical_pos(100, 200), logical_size(300, 150), paint);

    ASSERT_EQ(canvas.drawRects.size(), 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 150);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 230);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 450);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 380);
}

// ── drawLine tests ───────────────────────────

TEST_F(LogicalCanvasTest, DrawLineHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    lc.drawLine(logical_pos(10, 20), logical_pos(100, 200), paint);

    ASSERT_EQ(canvas.drawLines.size(), 1u);
    EXPECT_EQ(canvas.drawLines[0].x1, 10);
    EXPECT_EQ(canvas.drawLines[0].y1, 20);
    EXPECT_EQ(canvas.drawLines[0].x2, 100);
    EXPECT_EQ(canvas.drawLines[0].y2, 200);
}

TEST_F(LogicalCanvasTest, DrawLineVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    // to_physical with size(0,0):
    //   pos(10,20) → x = 800 - 20 - 0 = 780, y = 10
    //   pos(100,200) → x = 800 - 200 - 0 = 600, y = 100
    lc.drawLine(logical_pos(10, 20), logical_pos(100, 200), paint);

    ASSERT_EQ(canvas.drawLines.size(), 1u);
    EXPECT_EQ(canvas.drawLines[0].x1, 780);
    EXPECT_EQ(canvas.drawLines[0].y1, 10);
    EXPECT_EQ(canvas.drawLines[0].x2, 600);
    EXPECT_EQ(canvas.drawLines[0].y2, 100);
}

// ── clipRect tests ───────────────────────────

TEST_F(LogicalCanvasTest, ClipRectHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    lc.clipRect(logical_pos(50, 60), logical_size(200, 100));

    ASSERT_EQ(canvas.clipRects.size(), 1u);
    EXPECT_EQ(canvas.clipRects[0].rect.fLeft, 50);
    EXPECT_EQ(canvas.clipRects[0].rect.fTop, 60);
    EXPECT_EQ(canvas.clipRects[0].rect.fRight, 250);
    EXPECT_EQ(canvas.clipRects[0].rect.fBottom, 160);
    EXPECT_EQ(canvas.clipRects[0].op, SkClipOp::kIntersect);
    EXPECT_EQ(canvas.clipRects[0].antialias, false);
}

TEST_F(LogicalCanvasTest, ClipRectWithAntialias) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    lc.clipRect(logical_pos(10, 20), logical_size(100, 50), true);

    ASSERT_EQ(canvas.clipRects.size(), 1u);
    EXPECT_EQ(canvas.clipRects[0].antialias, true);
}

// ── save / restore tests ─────────────────────

TEST_F(LogicalCanvasTest, SaveRestore) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    EXPECT_EQ(canvas.saveCount, 0);
    EXPECT_EQ(canvas.restoreCount, 0);

    lc.save();
    EXPECT_EQ(canvas.saveCount, 1);

    lc.save();
    EXPECT_EQ(canvas.saveCount, 2);

    lc.restore();
    EXPECT_EQ(canvas.restoreCount, 1);

    lc.restore();
    EXPECT_EQ(canvas.restoreCount, 2);
}

// ── translate (offset accumulation) tests ────

TEST_F(LogicalCanvasTest, TranslateAccumulates) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm, 10, 20);

    lc.translate(5, 8);
    lc.drawRect(logical_pos(100, 200), logical_size(50, 30), paint);

    ASSERT_EQ(canvas.drawRects.size(), 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 115);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 228);
}

TEST_F(LogicalCanvasTest, CanvasAccessor) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    EXPECT_EQ(lc.canvas(), &canvas);
}

// ── combined: offset + vertical-rl ───────────

TEST_F(LogicalCanvasTest, DrawRectOffsetVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    LogicalCanvas lc(&canvas, wm, 10, 20);

    // to_physical: x = 800 - 200 - 150 = 450, y = 100
    // + offset: x = 460, y = 120, w = 150, h = 300
    lc.drawRect(logical_pos(100, 200), logical_size(300, 150), paint);

    ASSERT_EQ(canvas.drawRects.size(), 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 460);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 120);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 610);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 420);
}

// ── zero-size / edge cases ───────────────────

TEST_F(LogicalCanvasTest, DrawRectZeroSize) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    LogicalCanvas lc(&canvas, wm);

    lc.drawRect(logical_pos(0, 0), logical_size(0, 0), paint);

    ASSERT_EQ(canvas.drawRects.size(), 1u);
    EXPECT_EQ(canvas.drawRects[0].rect.fLeft, 0);
    EXPECT_EQ(canvas.drawRects[0].rect.fTop, 0);
    EXPECT_EQ(canvas.drawRects[0].rect.fRight, 0);
    EXPECT_EQ(canvas.drawRects[0].rect.fBottom, 0);
}
