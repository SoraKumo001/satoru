#pragma once
// Minimal SkTextBlob stub for native tests (no Skia dependency)
#include "SkRefCnt.h"
#include "SkRect.h"
#include "SkFont.h"

class SkTextBlob : public SkRefCnt {
public:
    ~SkTextBlob() override = default;
};

class SkTextBlobBuilder {
public:
    struct RunBuffer {
        void* glyphs = nullptr;
        void* pos = nullptr;
        void* utf8text = nullptr;
        void* clusters = nullptr;
    };
    const RunBuffer& allocRunTextPos(const SkFont& font, int count,
                                     const char* utf8 = nullptr, size_t textLen = 0,
                                     const SkRect* bounds = nullptr) {
        static RunBuffer buf;
        return buf;
    }
    sk_sp<SkTextBlob> make() { return sk_sp<SkTextBlob>(new SkTextBlob()); }
};
