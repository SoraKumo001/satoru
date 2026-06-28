#pragma once
// Minimal SkImageFilter stub for native tests (no Skia dependency)
#include "core/SkRefCnt.h"
#include "core/SkColor.h"

class SkColorFilter;

class SkImageFilter : public SkRefCnt {
public:
    enum Type { kBlur, kColorFilter, kDropShadow, kUnknown };

    Type fType = kUnknown;
    float fSigmaX = 0;
    float fSigmaY = 0;          // kBlur: blur sigmas
    sk_sp<SkColorFilter> fColorFilter; // kColorFilter
    float fDx = 0;
    float fDy = 0;              // kDropShadow: offset
    SkColor fColor = 0;         // kDropShadow: shadow color
    sk_sp<SkImageFilter> fInput;

    explicit SkImageFilter(Type t) : fType(t) {}

    SkImageFilter(Type t, sk_sp<SkImageFilter> input)
        : fType(t), fInput(std::move(input)) {}
};
