#pragma once
// Minimal SkPaint stub for native tests (no Skia dependency)

#include "core/SkBlendMode.h"
#include "core/SkColor.h"

#include <cstdint>

struct SkPaint {
    float fAlphaf = 1.0f;
    SkBlendMode fBlendMode = SkBlendMode::kSrcOver;
    SkColor fColor = 0;
    uint32_t fPadding = 0;  // ensure 8-byte alignment for SkColor

    void setAlphaf(float alpha) { fAlphaf = alpha; }
    float getAlphaf() const { return fAlphaf; }

    void setBlendMode(SkBlendMode mode) { fBlendMode = mode; }
    SkBlendMode getBlendMode() const { return fBlendMode; }

    void setColor(SkColor c) { fColor = c; }
    SkColor getColor() const { return fColor; }
};
