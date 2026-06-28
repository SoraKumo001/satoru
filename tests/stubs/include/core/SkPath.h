#pragma once
// Minimal SkPath / SkPathBuilder stub for native tests (no Skia dependency)
// Records all operations for test inspection.

#include "SkRect.h"
#include <vector>
#include <cstddef>

// Forward declaration of SkRRect (from SkRRect.h, included as needed)
class SkRRect;

// Record of a single path-building operation
struct SkPathOp {
    enum Type {
        kCircle,  // params = [cx, cy, r]
        kOval,    // params = [left, top, right, bottom]
        kRect,    // params = [left, top, right, bottom]
        kRRect,   // params = [radius] (rounded rect, detail not captured)
        kMove,    // params = [x, y]
        kLine,    // params = [x, y]
        kClose    // params = unused
    };
    Type type;
    float params[5]{};
};

// SkPath – holds a list of operations
struct SkPath {
    std::vector<SkPathOp> fOps;

    bool isEmpty() const { return fOps.empty(); }
};

// SkPathBuilder – accumulates operations, detach() produces an SkPath
class SkPathBuilder {
public:
    std::vector<SkPathOp> fOps;

    void addCircle(float cx, float cy, float r) {
        fOps.push_back({SkPathOp::kCircle, {cx, cy, r}});
    }

    void addOval(const SkRect& rect) {
        fOps.push_back({SkPathOp::kOval, {rect.fLeft, rect.fTop, rect.fRight, rect.fBottom}});
    }

    void addRect(const SkRect& rect) {
        fOps.push_back({SkPathOp::kRect, {rect.fLeft, rect.fTop, rect.fRight, rect.fBottom}});
    }

    void addRRect(const SkRRect&) {
        fOps.push_back({SkPathOp::kRRect, {}});
    }

    void moveTo(float x, float y) {
        fOps.push_back({SkPathOp::kMove, {x, y}});
    }

    void lineTo(float x, float y) {
        fOps.push_back({SkPathOp::kLine, {x, y}});
    }

    void close() {
        fOps.push_back({SkPathOp::kClose, {}});
    }

    SkPath detach() {
        SkPath p;
        p.fOps = std::move(fOps);
        fOps.clear();
        return p;
    }
};
