#pragma once
// Minimal SkImageFilters factory stub for native tests (no Skia dependency)
#include "core/SkImageFilter.h"
#include "core/SkColorFilter.h"

namespace SkImageFilters {

inline sk_sp<SkImageFilter> Blur(float sigmaX, float sigmaY, sk_sp<SkImageFilter> input) {
    auto f = sk_make_sp<SkImageFilter>(SkImageFilter::kBlur);
    f->fSigmaX = sigmaX;
    f->fSigmaY = sigmaY;
    f->fInput = std::move(input);
    return f;
}

inline sk_sp<SkImageFilter> ColorFilter(sk_sp<SkColorFilter> cf, sk_sp<SkImageFilter> input) {
    auto f = sk_make_sp<SkImageFilter>(SkImageFilter::kColorFilter);
    f->fColorFilter = std::move(cf);
    f->fInput = std::move(input);
    return f;
}

inline sk_sp<SkImageFilter> DropShadow(float dx, float dy, float sigmaX, float sigmaY,
                                        SkColor color, sk_sp<SkImageFilter> input) {
    auto f = sk_make_sp<SkImageFilter>(SkImageFilter::kDropShadow);
    f->fDx = dx;
    f->fDy = dy;
    f->fSigmaX = sigmaX;
    f->fSigmaY = sigmaY;
    f->fColor = color;
    f->fInput = std::move(input);
    return f;
}

} // namespace SkImageFilters

namespace SkColorFilters {

inline sk_sp<SkColorFilter> Matrix(const SkColorMatrix& cm) {
    auto cf = sk_make_sp<SkColorFilter>(SkColorFilter::kMatrix);
    cf->fMatrix = cm;
    return cf;
}

inline sk_sp<SkColorFilter> Blend(SkColor color, SkBlendMode mode) {
    auto cf = sk_make_sp<SkColorFilter>(SkColorFilter::kBlend);
    cf->fColor = color;
    cf->fMode = mode;
    return cf;
}

} // namespace SkColorFilters
