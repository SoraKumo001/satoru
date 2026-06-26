#pragma once
// Minimal SkFont stub for native tests (no Skia dependency)
#include "SkFontStyle.h"
#include "SkRefCnt.h"
#include "SkTypeface.h"
#include <cstdint>

using SkGlyphID = uint32_t;

enum class SkFontHinting : uint8_t {
    kNone = 0,
    kSlight = 1,
    kNormal = 2,
    kFull = 3,
};

class SkFont {
public:
    enum class Edging {
        kAlias = 0,
        kAntiAlias = 1,
        kSubpixelAntiAlias = 2,
    };

    SkFont() = default;
    SkFont(sk_sp<SkTypeface> tf, float size = 12.0f) : fTypeface(tf), fSize(size) {}
    void setSize(float size) { fSize = size; }
    float getSize() const { return fSize; }
    void setHinting(SkFontHinting) {}
    void setSubpixel(bool) {}
    void setEdging(Edging) {}
    void setEmbeddedBitmaps(bool) {}
    void setEmbolden(bool) {}
    void setSkewX(float) {}
    void setLinearMetrics(bool) {}
    sk_sp<SkTypeface> getTypeface() const { return fTypeface; }
private:
    sk_sp<SkTypeface> fTypeface;
    float fSize = 12.0f;
};
