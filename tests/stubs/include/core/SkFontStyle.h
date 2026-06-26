#pragma once
// Minimal SkFontStyle stub for native tests (no Skia dependency)
#include <cstdint>

class SkFontStyle {
   public:
    enum Slant { kUpright_Slant = 0, kItalic_Slant = 1 };
    static constexpr int kNormal_Width = 5;

    SkFontStyle() = default;
    SkFontStyle(int weight, int width, Slant slant)
        : fWeight(weight), fWidth(width), fSlant(slant) {}

    int weight() const { return fWeight; }
    int width() const { return fWidth; }
    Slant slant() const { return fSlant; }

    bool operator==(const SkFontStyle& o) const {
        return fWeight == o.fWeight && fWidth == o.fWidth && fSlant == o.fSlant;
    }
    bool operator!=(const SkFontStyle& o) const { return !(*this == o); }

   private:
    int fWeight = 400;
    int fWidth = kNormal_Width;
    Slant fSlant = kUpright_Slant;
};
