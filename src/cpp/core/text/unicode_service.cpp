#include "unicode_service.h"

#include <linebreak.h>
#include <utf8proc.h>

#include <algorithm>

#include "core/satoru_cache_manager.h"
#include "utils/skunicode_satoru.h"

namespace satoru {

UnicodeService::UnicodeService() { m_unicode = satoru::MakeUnicode(); }

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

void UnicodeService::encodeUtf8(char32_t u, std::string& out) const {
    utf8proc_uint8_t buf[4];
    utf8proc_ssize_t len = utf8proc_encode_char((utf8proc_int32_t)u, buf);
    if (len > 0) {
        out.append((const char*)buf, len);
    }
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
    return (u >= 0x1F000 && u <= 0x1FADF) || (u >= 0x1F300 && u <= 0x1F9FF) ||
           (u >= 0x2600 && u <= 0x26FF) || (u >= 0x2700 && u <= 0x27BF) ||
           (u >= 0x1F000 && u <= 0x1F02F) || (u >= 0x1F0A0 && u <= 0x1F0FF) ||
           (u >= 0x1F100 && u <= 0x1F64F) || (u >= 0x1F680 && u <= 0x1F6FF) ||
           (u >= 0x1F900 && u <= 0x1F9FF);
}

bool UnicodeService::shouldBreakGrapheme(char32_t u1, char32_t u2, int* state) const {
    return utf8proc_grapheme_break_stateful(u1, u2, state);
}

void UnicodeService::getLineBreaks(const char* text, size_t len, const char* lang,
                                   std::vector<char>& breaks,
                                   SatoruCacheManager* cacheManager) const {
    if (!text || len == 0) {
        breaks.clear();
        return;
    }

    std::string key;
    if (cacheManager) {
        if (lang && *lang) {
            key = std::string(lang) + ":" + std::string(text, len);
        } else {
            key = std::string(text, len);
        }

        if (std::vector<char>* cached = cacheManager->lineBreakCache.get(key)) {
            breaks = *cached;
            return;
        }
    }

    breaks.resize(len);
    set_linebreaks_utf8((const unsigned char*)text, len, lang, breaks.data());

    if (cacheManager) {
        cacheManager->lineBreakCache.put(key, breaks);
    }
}

void UnicodeService::clearCache() {}

char32_t UnicodeService::getVerticalSubstitution(char32_t u) const {
    switch (u) {
        case 0x2015:
            return 0x2014; // Horizontal bar -> Em dash (more common)
        case 0x2014:
            return 0xFE31;
        case 0x3008:
            return 0xFE3F;  // 〈
        case 0x3009:
            return 0xFE40;  // 〉
        case 0x300A:
            return 0xFE41;  // 《
        case 0x300B:
            return 0xFE42;  // 》
        case 0x300C:
            return 0xFE43;  // 「
        case 0x300D:
            return 0xFE44;  // 」
        case 0x300E:
            return 0xFE45;  // 『
        case 0x300F:
            return 0xFE46;  // 』
        case 0x3010:
            return 0xFE3B;  // 【
        case 0x3011:
            return 0xFE3C;  // 】
        case 0xFF08:
            return 0xFE35;  // （
        case 0xFF09:
            return 0xFE36;  // ）
        case 0xFF3B:
            return 0xFE47;  // ［
        case 0xFF3D:
            return 0xFE48;  // ］
        case 0xFF5B:
            return 0xFE37;  // ｛
        case 0xFF5D:
            return 0xFE38;  // ｝
        case 0x30FC:
            return 0xFE31;  // ー
        default:
            return u;
    }
}

bool UnicodeService::isVerticalUpright(char32_t u) const {
    if (u == 0x2014 || u == 0x2015) return false; // Dashes must be rotated if not substituted
    if (u >= 0x2E80 && u <= 0x2EFF) return true;
    if (u >= 0x2F00 && u <= 0x2FDF) return true;
    if (u >= 0x2FF0 && u <= 0x2FFF) return true;
    if (u >= 0x3000 && u <= 0x303F) return true;
    if (u >= 0x3040 && u <= 0x309F) return true;
    if (u >= 0x30A0 && u <= 0x30FF) return true;
    if (u >= 0x3100 && u <= 0x312F) return true;
    if (u >= 0x3130 && u <= 0x318F) return true;
    if (u >= 0x3190 && u <= 0x319F) return true;
    if (u >= 0x31A0 && u <= 0x31BF) return true;
    if (u >= 0x31C0 && u <= 0x31EF) return true;
    if (u >= 0x31F0 && u <= 0x31FF) return true;
    if (u >= 0x3200 && u <= 0x32FF) return true;
    if (u >= 0x3300 && u <= 0x33FF) return true;
    if (u >= 0x3400 && u <= 0x4DBF) return true;
    if (u >= 0x4E00 && u <= 0x9FFF) return true;
    if (u >= 0xAC00 && u <= 0xD7AF) return true;
    if (u >= 0xF900 && u <= 0xFAFF) return true;
    if (u >= 0xFE30 && u <= 0xFE4F) return true;
    if (u >= 0xFF00 && u <= 0xFF60) return true;
    if (u >= 0xFFE0 && u <= 0xFFEE) return true;
    if (u >= 0x20000 && u <= 0x3134F) return true;
    if (isEmoji(u)) return true;
    return false;
}

bool UnicodeService::isVerticalPunctuation(char32_t u) const {
    if (u == 0x3001 || u == 0x3002 || u == 0xFF0C || u == 0xFF0E) return true;
    if (u == 0x2015) return true; // Horizontal bar
    // Original Japanese brackets and punctuation (3008-3011, 3014-3015 etc)
    if (u >= 0x3008 && u <= 0x3011) return true;
    if (u >= 0x3014 && u <= 0x301B) return true;
    if (u == 0x30FC) return true; // Prolonged sound mark
    // Full-width brackets
    if (u == 0xFF08 || u == 0xFF09 || u == 0xFF3B || u == 0xFF3D || u == 0xFF5B || u == 0xFF5D) return true;
    // Vertical presentation forms for brackets and punctuation (FE30-FE4F)
    if (u >= 0xFE30 && u <= 0xFE4F) return true;
    return false;
}

}  // namespace satoru
