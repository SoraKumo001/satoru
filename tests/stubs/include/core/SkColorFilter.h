#pragma once
// Minimal SkColorFilter stub for native tests (no Skia dependency)
#include "core/SkRefCnt.h"
#include "core/SkColor.h"
#include "core/SkBlendMode.h"
#include "effects/SkColorMatrix.h"

class SkColorFilter : public SkRefCnt {
public:
    enum Type { kMatrix, kBlend };

    Type fType;
    SkColorMatrix fMatrix;  // valid when fType == kMatrix
    SkColor fColor = 0;     // valid when fType == kBlend
    SkBlendMode fMode = SkBlendMode::kSrcOver; // valid when fType == kBlend

    explicit SkColorFilter(Type t) : fType(t) {}
};
