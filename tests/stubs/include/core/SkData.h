#pragma once
// Minimal SkData stub for native tests (no Skia dependency)
#include "SkRefCnt.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

class SkData : public SkRefCnt {
public:
    static sk_sp<SkData> MakeWithCopy(const void* data, size_t size) {
        if (!data || size == 0) return nullptr;
        auto* d = new SkData();
        d->fPtr = new uint8_t[size];
        d->fSize = size;
        memcpy(d->fPtr, data, size);
        return sk_sp<SkData>(d);
    }

    static sk_sp<SkData> MakeEmpty() {
        return sk_sp<SkData>(new SkData());
    }

    const uint8_t* bytes() const { return fPtr; }
    size_t size() const { return fSize; }
    bool isEmpty() const { return fSize == 0; }

    ~SkData() override { delete[] fPtr; }

private:
    SkData() = default;
    uint8_t* fPtr = nullptr;
    size_t fSize = 0;
};
