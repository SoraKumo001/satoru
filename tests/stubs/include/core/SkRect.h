#pragma once
// Minimal SkRect stub for native tests (no Skia dependency)

struct SkRect {
    float fLeft{0}, fTop{0}, fRight{0}, fBottom{0};

    static SkRect MakeWH(float w, float h) { return {0, 0, w, h}; }
    static SkRect MakeIWH(int w, int h) { return {(float)w, (float)h}; }
    static SkRect MakeXYWH(float x, float y, float w, float h) {
        return {x, y, x + w, y + h};
    }

    float width() const { return fRight - fLeft; }
    float height() const { return fBottom - fTop; }
    bool isEmpty() const { return fLeft >= fRight || fTop >= fBottom; }
    void setXYWH(float x, float y, float w, float h) {
        fLeft = x; fTop = y; fRight = x + w; fBottom = y + h;
    }
};
