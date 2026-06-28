#pragma once
// Minimal SkBlendMode stub for native tests (no Skia dependency)
enum class SkBlendMode {
    kSrc, kDst, kSrcOver, kDstOver, kSrcIn, kDstIn, kSrcOut, kDstOut,
    kSrcATop, kDstATop, kXor, kPlus, kModulate,
    kScreen, kOverlay, kDarken, kLighten, kColorDodge, kColorBurn,
    kHardLight, kSoftLight, kDifference, kExclusion, kMultiply,
    kHue, kSaturation, kColor, kLuminosity,
    kLastMode = kLuminosity
};
