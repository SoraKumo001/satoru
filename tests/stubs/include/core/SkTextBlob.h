#pragma once
// Minimal SkTextBlob stub for native tests (no Skia dependency)
#include "SkRefCnt.h"

class SkTextBlob : public SkRefCnt {
public:
    ~SkTextBlob() override = default;
};
