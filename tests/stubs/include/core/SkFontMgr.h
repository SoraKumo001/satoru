#pragma once
// Minimal SkFontMgr stub for native tests
#include "SkRefCnt.h"
#include "SkTypeface.h"
#include "SkData.h"
#include <vector>
#include <string>

class SkFontMgr : public SkRefCnt {
public:
    SkFontMgr() = default;
    virtual ~SkFontMgr() = default;

    int countFamilies() const { return 0; }

    sk_sp<SkTypeface> matchSiblings(const sk_sp<SkTypeface> [], int) const {
        return nullptr;
    }

    sk_sp<SkTypeface> makeFromData(sk_sp<SkData>, int index = 0) const {
        if (index != 0) return nullptr;
        return sk_sp<SkTypeface>(new SkTypeface());
    }

    bool equal(const sk_sp<SkTypeface>& a, const sk_sp<SkTypeface>& b) const {
        return a.get() == b.get();
    }

    std::string getFamilyName(int) const { return ""; }
};

inline sk_sp<SkFontMgr> SkFontMgr_New_Custom_Empty() {
    return sk_sp<SkFontMgr>(new SkFontMgr());
}
