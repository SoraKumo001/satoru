#pragma once
// Minimal SkUnicode stub for native tests (no Skia dependency)
#include "include/core/SkRefCnt.h"

class SkUnicode : public SkRefCnt {
public:
    virtual ~SkUnicode() = default;
};
