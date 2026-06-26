#pragma once
// Minimal SkCanvas recording stub for native tests (no Skia dependency)
#include "core/SkRect.h"
#include "core/SkPaint.h"

enum class SkClipOp {
    kIntersect,
    kDifference
};

class SkCanvas {
public:
    virtual ~SkCanvas() = default;

    virtual void drawRect(const SkRect& rect, const SkPaint& paint) { (void)rect; (void)paint; }
    virtual void drawLine(float x1, float y1, float x2, float y2, const SkPaint& paint) {
        (void)x1; (void)y1; (void)x2; (void)y2; (void)paint;
    }
    virtual void clipRect(const SkRect& rect, SkClipOp op, bool antialias) {
        (void)rect; (void)op; (void)antialias;
    }
    virtual void save() {}
    virtual void restore() {}
};
