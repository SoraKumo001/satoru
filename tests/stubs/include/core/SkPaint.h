#pragma once
// Minimal SkPaint stub for native tests (no Skia dependency)

#include "core/SkColor.h"
#include "core/SkBlendMode.h"
#include "core/SkRefCnt.h"
#include "core/SkImageFilter.h"
#include "core/SkColorFilter.h"
#include "core/SkMaskFilter.h"
#include "core/SkPathEffect.h"
#include "core/SkShader.h"

struct SkPaint {
    SkColor fColor = 0;
    float fAlphaf = 1.0f;
    SkBlendMode fBlendMode = SkBlendMode::kSrcOver;
    bool fAntiAlias = false;
    float fStrokeWidth = 0;
    int fStyle = 0; // kFill_Style
    int fStrokeCap = 0;
    bool fFakeBoldText = false;
    sk_sp<SkImageFilter> fImageFilter;
    sk_sp<SkMaskFilter> fMaskFilter;
    sk_sp<SkPathEffect> fPathEffect;
    sk_sp<SkShader> fShader;

    void setColor(SkColor c) { fColor = c; }
    SkColor getColor() const { return fColor; }

    void setAlphaf(float a) { fAlphaf = a; }
    float getAlphaf() const { return fAlphaf; }

    void setBlendMode(SkBlendMode mode) { fBlendMode = mode; }
    SkBlendMode getBlendMode() const { return fBlendMode; }

    void setAntiAlias(bool aa) { fAntiAlias = aa; }
    bool isAntiAlias() const { return fAntiAlias; }

    void setStrokeWidth(float w) { fStrokeWidth = w; }
    float getStrokeWidth() const { return fStrokeWidth; }

    void setStyle(int s) { fStyle = s; }
    int getStyle() const { return fStyle; }

    void setStrokeCap(int cap) { fStrokeCap = cap; }
    int getStrokeCap() const { return fStrokeCap; }

    void setFakeBoldText(bool b) { fFakeBoldText = b; }
    bool isFakeBoldText() const { return fFakeBoldText; }

    void setImageFilter(sk_sp<SkImageFilter> filter) { fImageFilter = std::move(filter); }
    sk_sp<SkImageFilter> getImageFilter() const { return fImageFilter; }

    void setMaskFilter(sk_sp<SkMaskFilter> filter) { fMaskFilter = std::move(filter); }
    sk_sp<SkMaskFilter> getMaskFilter() const { return fMaskFilter; }

    void setPathEffect(sk_sp<SkPathEffect> effect) { fPathEffect = std::move(effect); }
    sk_sp<SkPathEffect> getPathEffect() const { return fPathEffect; }

    void setShader(sk_sp<SkShader> shader) { fShader = std::move(shader); }
    sk_sp<SkShader> getShader() const { return fShader; }
};
