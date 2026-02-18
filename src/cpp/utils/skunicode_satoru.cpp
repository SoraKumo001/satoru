#include "skunicode_satoru.h"

#include <linebreak.h>
#include <utf8proc.h>

#include <algorithm>
#include <vector>

#include "bridge/bridge_types.h"

class SkBreakIterator_Satoru : public SkBreakIterator {
   public:
    SkBreakIterator_Satoru() : fText(nullptr), fLen(0), fPos(0) {}
    ~SkBreakIterator_Satoru() override = default;

    Position first() override { return fPos = 0; }
    Position current() override { return fPos; }
    Position next() override {
        if (fPos == kDone || fPos >= fLen) return fPos = kDone;

        const char* p = fText + fPos;
        const char* end = fText + fLen;
        utf8proc_int32_t cp;
        utf8proc_ssize_t result = utf8proc_iterate((const utf8proc_uint8_t*)p, end - p, &cp);
        if (result > 0) {
            fPos += (int32_t)result;
        } else {
            fPos++;
        }
        if (fPos > fLen) fPos = fLen;
        return fPos;
    }
    Status status() override { return 0; }
    bool isDone() override { return fPos == kDone; }
    bool setText(const char utftext8[], int utf8Units) override {
        fText = utftext8;
        fLen = utf8Units;
        fPos = 0;
        return true;
    }
    bool setText(const char16_t utftext16[], int utf16Units) override { return false; }

   private:
    const char* fText;
    int32_t fLen;
    int32_t fPos;
    static constexpr Position kDone = -1;
};

class SkUnicode_Satoru : public SkUnicode {
   public:
    SkUnicode_Satoru() {}
    ~SkUnicode_Satoru() override = default;

    SkString toUpper(const SkString& s) override { return s; }
    SkString toUpper(const SkString& s, const char*) override { return s; }

    bool isControl(SkUnichar utf8) override {
        return utf8proc_category(utf8) == UTF8PROC_CATEGORY_CC;
    }
    bool isWhitespace(SkUnichar utf8) override {
        auto cat = utf8proc_category(utf8);
        return cat == UTF8PROC_CATEGORY_ZS || cat == UTF8PROC_CATEGORY_ZL ||
               cat == UTF8PROC_CATEGORY_ZP || utf8 == '\t' || utf8 == '\n' || utf8 == '\r' ||
               utf8 == '\f' || utf8 == '\v';
    }
    bool isSpace(SkUnichar utf8) override { return utf8 == ' '; }
    bool isTabulation(SkUnichar utf8) override { return utf8 == '\t'; }
    bool isHardBreak(SkUnichar utf8) override { return utf8 == '\n' || utf8 == '\r'; }
    bool isEmoji(SkUnichar utf8) override { return false; }
    bool isEmojiComponent(SkUnichar utf8) override { return false; }
    bool isEmojiModifierBase(SkUnichar utf8) override { return false; }
    bool isEmojiModifier(SkUnichar utf8) override { return false; }
    bool isRegionalIndicator(SkUnichar utf8) override { return false; }
    bool isIdeographic(SkUnichar utf8) override {
        auto cat = utf8proc_category(utf8);
        return cat == UTF8PROC_CATEGORY_LO;  // Simplified
    }

    std::unique_ptr<SkBidiIterator> makeBidiIterator(const uint16_t text[], int count,
                                                     SkBidiIterator::Direction) override {
        return nullptr;
    }
    std::unique_ptr<SkBidiIterator> makeBidiIterator(const char text[], int count,
                                                     SkBidiIterator::Direction) override {
        return nullptr;
    }
    std::unique_ptr<SkBreakIterator> makeBreakIterator(const char locale[],
                                                       BreakType breakType) override {
        return std::make_unique<SkBreakIterator_Satoru>();
    }
    std::unique_ptr<SkBreakIterator> makeBreakIterator(BreakType type) override {
        return std::make_unique<SkBreakIterator_Satoru>();
    }

    bool getBidiRegions(const char utf8[], int utf8Units, TextDirection dir,
                        std::vector<BidiRegion>* results) override {
        results->clear();
        if (utf8Units <= 0) return true;

        const char* p = utf8;
        const char* end = utf8 + utf8Units;
        int last_pos = 0;
        BidiLevel last_level = (dir == TextDirection::kRTL) ? 1 : 0;

        while (p < end) {
            const char* prev_p = p;
            utf8proc_int32_t cp;
            utf8proc_ssize_t res = utf8proc_iterate((const utf8proc_uint8_t*)p, end - p, &cp);
            if (res <= 0) break;
            p += res;

            // Simple check for Arabic/Hebrew scripts
            BidiLevel level = 0;
            if ((cp >= 0x0590 && cp <= 0x08FF) || (cp >= 0xFB50 && cp <= 0xFDFF) ||
                (cp >= 0xFE70 && cp <= 0xFEFF)) {
                level = 1;  // RTL
            } else if (dir == TextDirection::kRTL) {
                level = 1;  // Base RTL
            }

            if (level != last_level) {
                if (prev_p - utf8 > last_pos) {
                    results->push_back(
                        {(size_t)last_pos, (size_t)(prev_p - utf8 - last_pos), last_level});
                }
                last_pos = prev_p - utf8;
                last_level = level;
            }
        }

        if (utf8 + utf8Units - utf8 > last_pos) {
            results->push_back({(size_t)last_pos, (size_t)(utf8Units - last_pos), last_level});
        }

        return true;
    }
    bool getWords(const char utf8[], int utf8Units, const char* locale,
                  std::vector<Position>* results) override {
        return false;
    }
    bool getUtf8Words(const char utf8[], int utf8Units, const char* locale,
                      std::vector<Position>* results) override {
        return false;
    }
    bool getSentences(const char utf8[], int utf8Units, const char* locale,
                      std::vector<Position>* results) override {
        return false;
    }

    bool computeCodeUnitFlags(
        char utf8[], int utf8Units, bool replaceTabs,
        skia_private::TArray<SkUnicode::CodeUnitFlags, true>* results) override {
        results->clear();
        for (int i = 0; i < utf8Units; ++i) {
            results->push_back(kNoCodeUnitFlag);
        }
        return true;
    }
    bool computeCodeUnitFlags(
        char16_t utf16[], int utf16Units, bool replaceTabs,
        skia_private::TArray<SkUnicode::CodeUnitFlags, true>* results) override {
        results->clear();
        for (int i = 0; i < utf16Units; ++i) {
            results->push_back(kNoCodeUnitFlag);
        }
        return true;
    }
    void reorderVisual(const BidiLevel runLevels[], int levelsCount,
                       int32_t logicalFromVisual[]) override {
        for (int i = 0; i < levelsCount; ++i) logicalFromVisual[i] = i;
    }
};

namespace satoru {
sk_sp<SkUnicode> MakeUnicode() {
    static sk_sp<SkUnicode> instance = sk_make_sp<SkUnicode_Satoru>();
    return instance;
}
}  // namespace satoru
