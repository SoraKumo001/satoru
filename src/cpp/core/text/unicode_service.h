#ifndef SATORU_UNICODE_SERVICE_H
#define SATORU_UNICODE_SERVICE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "include/core/SkRefCnt.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "utils/lru_cache.h"

namespace satoru {

class UnicodeService {
   public:
    UnicodeService();
    ~UnicodeService() = default;

    // UTF-8 Decoding (Advances the pointer)
    char32_t decodeUtf8(const char** ptr) const;

    // Normalization (NFC)
    std::string normalize(const char* text) const;

    // BiDi Level Calculation
    int getBidiLevel(const char* text, int baseLevel, int* lastLevel = nullptr) const;

    // Unicode Properties
    bool isMark(char32_t u) const;
    bool isSpace(char32_t u) const;
    bool isEmoji(char32_t u) const;
    bool shouldBreakGrapheme(char32_t u1, char32_t u2, int* state) const;

    // Line Breaking (wrapper for libunibreak/SkUnicode)
    void getLineBreaks(const char* text, size_t len, const char* lang,
                       std::vector<char>& breaks) const;

    // Cache Management
    void clearCache();

    // SkUnicode access
    SkUnicode* getSkUnicode() const { return m_unicode.get(); }
    sk_sp<SkUnicode> getSkUnicodeSp() const { return m_unicode; }

   private:
    sk_sp<SkUnicode> m_unicode;
    mutable LruCache<std::string, std::vector<char>> m_lineBreakCache;
};

}  // namespace satoru

#endif  // SATORU_UNICODE_SERVICE_H
