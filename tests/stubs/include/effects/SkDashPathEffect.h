#pragma once
#include "core/SkPathEffect.h"
#include "core/SkSpan.h"

struct SkDashPathEffect {
    static sk_sp<SkPathEffect> Make(SkSpan<const float> intervals, float phase) {
        return nullptr;
    }
};
