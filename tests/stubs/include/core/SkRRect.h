#pragma once
// Minimal SkRRect stub for native tests (no Skia dependency)
#include <cstdint>

struct SkScalar { float fValue{0}; };

struct SkPoint {
    SkScalar fX{}, fY{};
    static SkPoint Make(float x, float y) { return {SkScalar{x}, SkScalar{y}}; }
};

struct SkRect {
    SkScalar fLeft{}, fTop{}, fRight{}, fBottom{};
    static SkRect MakeWH(float w, float h) { return {SkScalar{0}, SkScalar{0}, SkScalar{w}, SkScalar{h}}; }
    static SkRect MakeIWH(int w, int h) { return MakeWH((float)w, (float)h); }
    float width() const { return fRight.fValue - fLeft.fValue; }
    float height() const { return fBottom.fValue - fTop.fValue; }
    bool isEmpty() const { return fLeft.fValue >= fRight.fValue || fTop.fValue >= fBottom.fValue; }
    void setXYWH(float x, float y, float w, float h) {
        fLeft.fValue = x; fTop.fValue = y; fRight.fValue = x + w; fBottom.fValue = y + h;
    }
};

class SkRRect {
public:
    SkRRect() = default;
    static SkRRect MakeRect(const SkRect& rect) { SkRRect r; r.fRect = rect; return r; }
    void setRect(const SkRect& rect) { fRect = rect; }
    const SkRect& rect() const { return fRect; }
    bool isEmpty() const { return fRect.isEmpty(); }
private:
    SkRect fRect;
};
