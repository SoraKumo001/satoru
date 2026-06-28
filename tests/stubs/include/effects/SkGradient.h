#pragma once
#include "core/SkColor.h"
#include "core/SkPoint.h"
#include "core/SkRefCnt.h"
#include "core/SkShader.h"
#include "core/SkTileMode.h"

struct SkGradientShader {
    static sk_sp<SkShader> MakeLinear(const SkPoint pts[2],
                                       const SkColor colors[], const SkScalar pos[],
                                       int count, SkTileMode mode) {
        return nullptr;
    }
    static sk_sp<SkShader> MakeRadial(const SkPoint& center, SkScalar radius,
                                       const SkColor colors[], const SkScalar pos[],
                                       int count, SkTileMode mode) {
        return nullptr;
    }
    static sk_sp<SkShader> MakeSweep(SkScalar cx, SkScalar cy,
                                      const SkColor colors[], const SkScalar pos[],
                                      int count) {
        return nullptr;
    }
};
