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

// ── New recording data types (Phase 2 extension) ──

struct RecordedLayer {
    SkRect bounds;
    SkPaint paint;
    bool has_bounds;
    bool has_paint;
};

struct RecordedRRect {
    SkRRect rrect;
    SkPaint paint;
};

struct RecordedMatrix {
    SkMatrix matrix;
    bool is_concat;  // true: concat(), false: setMatrix()
};

class RecordingCanvas : public SkCanvas {
public:
    // Original recording data
    std::vector<RecordedDrawRect> drawRects;
    std::vector<RecordedDrawLine> drawLines;
    std::vector<RecordedClipRect> clipRects;
    int saveCount = 0;
    int restoreCount = 0;

    // Phase 2 recording data
    std::vector<RecordedLayer> saveLayers;
    std::vector<RecordedRRect> drawRRects;
    std::vector<RecordedMatrix> concatMatrices;

    // ── Original overrides ──

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

    // ── Phase 2 overrides ──

    void saveLayer(const SkRect* bounds, const SkPaint* paint) override {
        saveLayers.push_back({
            bounds ? *bounds : SkRect(),
            paint ? *paint : SkPaint(),
            bounds != nullptr,
            paint != nullptr
        });
    }

    void saveLayer(const SaveLayerRec& rec) override {
        saveLayers.push_back({
            rec.fBounds ? *rec.fBounds : SkRect(),
            rec.fPaint ? *rec.fPaint : SkPaint(),
            rec.fBounds != nullptr,
            rec.fPaint != nullptr
        });
    }

    void drawRRect(const SkRRect& rr, const SkPaint& p) override {
        drawRRects.push_back({rr, p});
    }

    void concat(const SkMatrix& m) override {
        concatMatrices.push_back({m, true});
    }

    void setMatrix(const SkMatrix& m) override {
        concatMatrices.push_back({m, false});
    }

    // ── Getters ──

    const std::vector<RecordedLayer>& getSaveLayers() const { return saveLayers; }
    const std::vector<RecordedRRect>& getDrawRRects() const { return drawRRects; }
    const std::vector<RecordedMatrix>& getConcats() const { return concatMatrices; }

    // ── Reset all recordings ──

    void clear() {
        drawRects.clear();
        drawLines.clear();
        clipRects.clear();
        saveCount = 0;
        restoreCount = 0;
        saveLayers.clear();
        drawRRects.clear();
        concatMatrices.clear();
    }
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

// ══════════════════════════════════════════════
// Phase 2: RecordingCanvas Extension Tests
//
// These tests verify that the RecordingCanvas
// correctly records SkCanvas API calls needed
// for push_backdrop_filter / push_layer /
// push_transform / draw_borders testing.
// ══════════════════════════════════════════════

class RecordingCanvasTest : public ::testing::Test {
protected:
    RecordingCanvas canvas;
    SkPaint paint;
};

// ── saveLayer recording ──────────────────────

TEST_F(RecordingCanvasTest, SaveLayerRecordsCall) {
    SkRect bounds = SkRect::MakeXYWH(10, 20, 100, 50);
    canvas.saveLayer(&bounds, &paint);

    ASSERT_EQ(canvas.saveLayers.size(), 1u);
    EXPECT_TRUE(canvas.saveLayers[0].has_bounds);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fLeft, 10);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fTop, 20);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fRight, 110);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fBottom, 70);
}

TEST_F(RecordingCanvasTest, SaveLayerNullBounds) {
    canvas.saveLayer(nullptr, &paint);

    ASSERT_EQ(canvas.saveLayers.size(), 1u);
    EXPECT_FALSE(canvas.saveLayers[0].has_bounds);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
}

TEST_F(RecordingCanvasTest, SaveLayerNullPaint) {
    SkRect bounds = SkRect::MakeXYWH(0, 0, 200, 100);
    canvas.saveLayer(&bounds, nullptr);

    ASSERT_EQ(canvas.saveLayers.size(), 1u);
    EXPECT_TRUE(canvas.saveLayers[0].has_bounds);
    EXPECT_FALSE(canvas.saveLayers[0].has_paint);
}

TEST_F(RecordingCanvasTest, SaveLayerBothNull) {
    canvas.saveLayer(nullptr, nullptr);

    ASSERT_EQ(canvas.saveLayers.size(), 1u);
    EXPECT_FALSE(canvas.saveLayers[0].has_bounds);
    EXPECT_FALSE(canvas.saveLayers[0].has_paint);
}

TEST_F(RecordingCanvasTest, SaveLayerRecRecordsCall) {
    SkRect bounds = SkRect::MakeXYWH(5, 15, 80, 40);
    SkCanvas::SaveLayerRec rec(&bounds, &paint, nullptr, 0);
    canvas.saveLayer(rec);

    ASSERT_EQ(canvas.saveLayers.size(), 1u);
    EXPECT_TRUE(canvas.saveLayers[0].has_bounds);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fLeft, 5);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fTop, 15);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fRight, 85);
    EXPECT_EQ(canvas.saveLayers[0].bounds.fBottom, 55);
}

TEST_F(RecordingCanvasTest, SaveLayerRecNullFields) {
    SkCanvas::SaveLayerRec rec(nullptr, nullptr, nullptr, 0);
    canvas.saveLayer(rec);

    ASSERT_EQ(canvas.saveLayers.size(), 1u);
    EXPECT_FALSE(canvas.saveLayers[0].has_bounds);
    EXPECT_FALSE(canvas.saveLayers[0].has_paint);
}

TEST_F(RecordingCanvasTest, SaveLayerMultipleCalls) {
    SkRect b1 = SkRect::MakeXYWH(0, 0, 50, 50);
    SkRect b2 = SkRect::MakeXYWH(10, 10, 30, 30);
    canvas.saveLayer(&b1, &paint);
    canvas.saveLayer(&b2, nullptr);
    canvas.saveLayer(nullptr, &paint);

    ASSERT_EQ(canvas.saveLayers.size(), 3u);
    EXPECT_TRUE(canvas.saveLayers[0].has_bounds);
    EXPECT_TRUE(canvas.saveLayers[1].has_bounds);
    EXPECT_FALSE(canvas.saveLayers[2].has_bounds);
    EXPECT_TRUE(canvas.saveLayers[0].has_paint);
    EXPECT_FALSE(canvas.saveLayers[1].has_paint);
    EXPECT_TRUE(canvas.saveLayers[2].has_paint);
}

TEST_F(RecordingCanvasTest, SaveLayerDoesNotAffectSaveCount) {
    canvas.saveLayer(nullptr, &paint);
    canvas.save();
    canvas.saveLayer(nullptr, nullptr);

    EXPECT_EQ(canvas.saveCount, 1);
    EXPECT_EQ(canvas.saveLayers.size(), 2u);
}

// ── drawRRect recording ──────────────────────

TEST_F(RecordingCanvasTest, DrawRRectRecordsCall) {
    SkRect r = SkRect::MakeXYWH(100, 200, 300, 150);
    SkRRect rr = SkRRect::MakeRect(r);
    canvas.drawRRect(rr, paint);

    ASSERT_EQ(canvas.drawRRects.size(), 1u);
    EXPECT_EQ(canvas.drawRRects[0].rrect.rect().fLeft, 100);
    EXPECT_EQ(canvas.drawRRects[0].rrect.rect().fTop, 200);
    EXPECT_EQ(canvas.drawRRects[0].rrect.rect().fRight, 400);
    EXPECT_EQ(canvas.drawRRects[0].rrect.rect().fBottom, 350);
}

TEST_F(RecordingCanvasTest, DrawRRectMultipleCalls) {
    SkRRect rr1 = SkRRect::MakeRect(SkRect::MakeXYWH(0, 0, 10, 10));
    SkRRect rr2 = SkRRect::MakeRect(SkRect::MakeXYWH(5, 5, 20, 20));
    canvas.drawRRect(rr1, paint);
    canvas.drawRRect(rr2, paint);

    ASSERT_EQ(canvas.drawRRects.size(), 2u);
    EXPECT_EQ(canvas.drawRRects[0].rrect.rect().fRight, 10);
    EXPECT_EQ(canvas.drawRRects[1].rrect.rect().fRight, 25);
}

// ── concat / setMatrix recording ─────────────

TEST_F(RecordingCanvasTest, ConcatRecordsCall) {
    SkMatrix m;
    m.setTranslate(10.0f, 20.0f);
    canvas.concat(m);

    ASSERT_EQ(canvas.concatMatrices.size(), 1u);
    EXPECT_TRUE(canvas.concatMatrices[0].is_concat);
    EXPECT_EQ(canvas.concatMatrices[0].matrix, m);
}

TEST_F(RecordingCanvasTest, SetMatrixRecordsCall) {
    SkMatrix m;
    m.setScaleTranslate(2.0f, 3.0f, 100.0f, 200.0f);
    canvas.setMatrix(m);

    ASSERT_EQ(canvas.concatMatrices.size(), 1u);
    EXPECT_FALSE(canvas.concatMatrices[0].is_concat);
    EXPECT_EQ(canvas.concatMatrices[0].matrix, m);
}

TEST_F(RecordingCanvasTest, ConcatAndSetMatrixAreDistinct) {
    SkMatrix m1;
    m1.setTranslate(5.0f, 10.0f);
    SkMatrix m2;
    m2.setScaleTranslate(1.0f, 1.0f, 0.0f, 0.0f);

    canvas.concat(m1);
    canvas.setMatrix(m2);
    canvas.concat(m2);

    ASSERT_EQ(canvas.concatMatrices.size(), 3u);
    EXPECT_TRUE(canvas.concatMatrices[0].is_concat);
    EXPECT_FALSE(canvas.concatMatrices[1].is_concat);
    EXPECT_TRUE(canvas.concatMatrices[2].is_concat);
}

// ── clear() ─────────────────────────────────

TEST_F(RecordingCanvasTest, ClearResetsAllRecordings) {
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
    canvas.saveLayer(nullptr, &paint);
    canvas.drawRRect(SkRRect::MakeRect(SkRect::MakeXYWH(0, 0, 1, 1)), paint);
    canvas.concat(SkMatrix());
    canvas.save();
    canvas.restore();

    canvas.clear();

    EXPECT_EQ(canvas.drawRects.size(), 0u);
    EXPECT_EQ(canvas.saveLayers.size(), 0u);
    EXPECT_EQ(canvas.drawRRects.size(), 0u);
    EXPECT_EQ(canvas.concatMatrices.size(), 0u);
    EXPECT_EQ(canvas.saveCount, 0);
    EXPECT_EQ(canvas.restoreCount, 0);
}

TEST_F(RecordingCanvasTest, ClearAndReRecord) {
    canvas.saveLayer(nullptr, &paint);
    ASSERT_EQ(canvas.saveLayers.size(), 1u);

    canvas.clear();
    ASSERT_EQ(canvas.saveLayers.size(), 0u);

    canvas.saveLayer(nullptr, nullptr);
    ASSERT_EQ(canvas.saveLayers.size(), 1u);
    EXPECT_FALSE(canvas.saveLayers[0].has_paint);
}
