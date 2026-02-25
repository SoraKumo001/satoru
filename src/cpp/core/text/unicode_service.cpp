#include "unicode_service.h"

#include <linebreak.h>
#include <utf8proc.h>

#include <algorithm>

#include "utils/skunicode_satoru.h"

namespace satoru {

UnicodeService::UnicodeService() : m_lineBreakCache(1000) { m_unicode = satoru::MakeUnicode(); }

char32_t UnicodeService::decodeUtf8(const char** ptr) const {
    if (!ptr || !*ptr || !**ptr) return 0;

    utf8proc_int32_t cp;
    utf8proc_ssize_t result = utf8proc_iterate((const utf8proc_uint8_t*)*ptr, -1, &cp);

    if (result < 0) {
        (*ptr)++;  // Advance one byte on error
        return 0xFFFD;
    }

    *ptr += result;
    return (char32_t)cp;
}

std::string UnicodeService::normalize(const char* text) const {
    if (!text || !*text) return "";
    utf8proc_uint8_t* retval = nullptr;
    utf8proc_ssize_t len = utf8proc_map((const utf8proc_uint8_t*)text, 0, &retval,
                                        (utf8proc_option_t)UTF8PROC_COMPOSE);
    if (len < 0 || !retval) return std::string(text);
    std::string result((const char*)retval);
    free(retval);
    return result;
}

int UnicodeService::getBidiLevel(const char* text, int baseLevel, int* lastLevel) const {
    if (!text || !*text) return baseLevel;

    const char* p = text;
    bool all_neutral = true;
    while (*p) {
        char32_t u = decodeUtf8(&p);
        auto cat = utf8proc_category(u);

        // Skip punctuation, marks, and neutral characters
        if (cat == UTF8PROC_CATEGORY_PO || cat == UTF8PROC_CATEGORY_PD ||
            cat == UTF8PROC_CATEGORY_PS || cat == UTF8PROC_CATEGORY_PE ||
            cat == UTF8PROC_CATEGORY_PI || cat == UTF8PROC_CATEGORY_PF ||
            cat == UTF8PROC_CATEGORY_PC || cat == UTF8PROC_CATEGORY_MN ||
            cat == UTF8PROC_CATEGORY_MC || cat == UTF8PROC_CATEGORY_ME ||
            cat == UTF8PROC_CATEGORY_ZS || (u <= 0x2F) || (u >= 0x3A && u <= 0x40) ||
            (u >= 0x5B && u <= 0x60) || (u >= 0x7B && u <= 0x7F)) {
            continue;
        }

        all_neutral = false;

        // Check if character is RTL (Arabic, Hebrew, Syriac, Thaana, N'Ko, etc.)
        bool is_rtl =
            (u >= 0x0590 && u <= 0x08FF) ||    // Hebrew, Arabic, Syriac, Arabic Supplement, Thaana,
                                               // Samaritan, Mandaic, Arabic Extended-A
            (u >= 0xFB50 && u <= 0xFDFF) ||    // Arabic Presentation Forms-A
            (u >= 0xFE70 && u <= 0xFEFF) ||    // Arabic Presentation Forms-B
            (u >= 0x10800 && u <= 0x10FFF) ||  // Various ancient RTL scripts
            (u >= 0x1E800 && u <= 0x1EFFF);    // More RTL scripts

        int level;
        if (is_rtl) {
            level = 1;  // RTL Level 1
        } else {
            level = (baseLevel == 1) ? 2 : 0;  // LTR in RTL block is Level 2, otherwise 0
        }

        if (lastLevel) *lastLevel = level;
        return level;
    }

    if (all_neutral && lastLevel && *lastLevel != -1) {
        return *lastLevel;
    }

    return baseLevel;
}

bool UnicodeService::isMark(char32_t u) const {
    auto category = utf8proc_category(u);
    return (category == UTF8PROC_CATEGORY_MN || category == UTF8PROC_CATEGORY_MC ||
            category == UTF8PROC_CATEGORY_ME);
}

bool UnicodeService::isSpace(char32_t u) const {
    auto cat = utf8proc_category(u);
    return (cat == UTF8PROC_CATEGORY_ZS ||
            (u <= 0x20 && (u == 0x20 || u == 0x09 || u == 0x0A || u == 0x0D || u == 0x0C)));
}

bool UnicodeService::isEmoji(char32_t u) const {
    return (u >= 0x1F300 && u <= 0x1F9FF) || (u >= 0x2600 && u <= 0x26FF);
}

bool UnicodeService::shouldBreakGrapheme(char32_t u1, char32_t u2, int* state) const {
    return utf8proc_grapheme_break_stateful(u1, u2, state);
}

void UnicodeService::getLineBreaks(const char* text, size_t len, const char* lang,
                                   std::vector<char>& breaks) const {
    if (!text || len == 0) {
        breaks.clear();
        return;
    }

    std::string key;
    if (lang && *lang) {
        key = std::string(lang) + ":" + std::string(text, len);
    } else {
        key = std::string(text, len);
    }

    if (std::vector<char>* cached = m_lineBreakCache.get(key)) {
        breaks = *cached;
        return;
    }

    breaks.resize(len);
    set_linebreaks_utf8((const unsigned char*)text, len, lang, breaks.data());
    m_lineBreakCache.put(key, breaks);
}

void UnicodeService::clearCache() { m_lineBreakCache.clear(); }

}  // namespace satoru
