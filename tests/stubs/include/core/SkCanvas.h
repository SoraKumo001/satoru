#pragma once
// Minimal SkCanvas recording stub for native tests (no Skia dependency)
#include "core/SkRect.h"
#include "core/SkPaint.h"
#include "core/SkRRect.h"
#include "core/SkMatrix.h"

enum class SkClipOp {
    kIntersect,
    kDifference
};

class SkImageFilter;
class SkPath;

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

    // SaveLayer support
    struct SaveLayerRec {
        const SkRect* fBounds = nullptr;
        const SkPaint* fPaint = nullptr;
        const SkImageFilter* fBackdrop = nullptr;
        uint32_t fSaveLayerFlags = 0;

        SaveLayerRec() = default;
        SaveLayerRec(const SkRect* b, const SkPaint* p, const SkImageFilter* back = nullptr,
                     uint32_t flags = 0)
            : fBounds(b), fPaint(p), fBackdrop(back), fSaveLayerFlags(flags) {}
    };

    virtual void saveLayer(const SkRect* bounds, const SkPaint* paint) {
        (void)bounds; (void)paint;
    }
    virtual void saveLayer(const SaveLayerRec& rec) { (void)rec; }

    // RRect
    virtual void drawRRect(const SkRRect& rr, const SkPaint& p) { (void)rr; (void)p; }

    // Path
    virtual void drawPath(const SkPath& path, const SkPaint& paint) {
        (void)path; (void)paint;
    }
    virtual void clipPath(const SkPath& path, bool antialias) {
        (void)path; (void)antialias;
    }
    virtual void clipRRect(const SkRRect& rr, bool antialias) {
        (void)rr; (void)antialias;
    }

    // Matrix transforms
    virtual void concat(const SkMatrix& m) { (void)m; }
    virtual void setMatrix(const SkMatrix& m) { (void)m; }
    virtual void translate(float dx, float dy) { (void)dx; (void)dy; }
    virtual void scale(float sx, float sy) { (void)sx; (void)sy; }
    virtual void rotate(float degrees) { (void)degrees; }
    virtual void skew(float sx, float sy) { (void)sx; (void)sy; }
};
