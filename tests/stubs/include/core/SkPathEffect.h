#pragma once
#include "core/SkRefCnt.h"
#include "core/SkSpan.h"
class SkPathEffect : public SkRefCnt {
public:
    ~SkPathEffect() override = default;
};
namespace SkDashPathEffect {
inline sk_sp<SkPathEffect> Make(SkSpan<const float>, float) { return nullptr; }
}
