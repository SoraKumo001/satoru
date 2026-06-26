#pragma once
// Minimal SkImage stub for native tests
#include "SkRefCnt.h"
#include "SkColor.h"
#include <cstdint>
#include <memory>

class SkImage : public SkRefCnt {
public:
    virtual ~SkImage() = default;
    int width() const { return fWidth; }
    int height() const { return fHeight; }

    // Stub: always returns nullptr
    const uint8_t* readPixels(int, int, int, int) const { return nullptr; }

protected:
    int fWidth = 0;
    int fHeight = 0;
};
