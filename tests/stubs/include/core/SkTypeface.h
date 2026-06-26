#pragma once
// Minimal SkTypeface stub for native tests
#include "SkRefCnt.h"
#include "SkFontStyle.h"
#include "SkFontArguments.h"
#include "SkSpan.h"

using SkTypefaceID = uint32_t;

class SkTypeface : public SkRefCnt {
public:
    SkTypeface() : fID(++sNextID) {}
    SkTypeface(SkFontStyle style) : fID(++sNextID), fStyle(style) {}
    virtual ~SkTypeface() = default;

    SkTypefaceID uniqueID() const { return fID; }
    const SkFontStyle& fontStyle() const { return fStyle; }

    // Glyph mapping stub — always returns 0 (no glyph)
    uint16_t unicharToGlyph(int32_t) const { return 0; }

    // Variable font stubs
    int getVariationDesignPosition(SkSpan<SkFontArguments::VariationPosition::Coordinate>) const { return 0; }
    sk_sp<SkTypeface> makeClone(const SkFontArguments&) const {
        return sk_sp<SkTypeface>(new SkTypeface(fStyle));
    }

private:
    SkTypefaceID fID;
    SkFontStyle fStyle;
    static inline SkTypefaceID sNextID = 0;
};
