#pragma once
struct SkPoint {
    SkScalar fX = 0;
    SkScalar fY = 0;
    static SkPoint Make(SkScalar x, SkScalar y) { return {x, y}; }
};
using SkScalar = float;
