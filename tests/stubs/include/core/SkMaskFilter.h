#pragma once
#include "core/SkRefCnt.h"
class SkMaskFilter : public SkRefCnt {
public:
    ~SkMaskFilter() override = default;
    static sk_sp<SkMaskFilter> MakeBlur(int, float) { return nullptr; }
};
