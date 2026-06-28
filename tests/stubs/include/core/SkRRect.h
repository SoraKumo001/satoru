#pragma once
// Minimal SkRRect stub for native tests (no Skia dependency)
#include "SkRect.h"

struct SkPoint {
    float fX{}, fY{};
    static SkPoint Make(float x, float y) { return {x, y}; }
};

class SkRRect {
public:
    SkRRect() = default;
    static SkRRect MakeRect(const SkRect& rect) { SkRRect r; r.fRect = rect; return r; }
    void setRect(const SkRect& rect) { fRect = rect; }
    const SkRect& rect() const { return fRect; }
    bool isEmpty() const { return fRect.isEmpty(); }
    static SkRRect MakeEmpty() { return SkRRect(); }
private:
    SkRect fRect;
};
