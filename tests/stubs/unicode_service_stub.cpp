// Stub implementation of UnicodeService for native tests
// Avoids dependencies on utf8proc, linebreak, and Skia/SkUnicode
#include "core/text/unicode_service.h"

namespace satoru {

UnicodeService::UnicodeService() {
    m_unicode = nullptr;  // No SkUnicode in tests
}

char32_t UnicodeService::decodeUtf8(const char** ptr) const {
    if (!ptr || !*ptr || !**ptr) return 0;
    const unsigned char first = static_cast<unsigned char>(**ptr);
    if (first < 0x80) {
        (*ptr)++;
        return first;
    }
    // Minimal multi-byte: just return replacement char
    (*ptr)++;
    return 0xFFFD;
}

void UnicodeService::encodeUtf8(char32_t u, std::string& out) const {
    if (u < 0x80) {
        out += static_cast<char>(u);
    } else {
        out += static_cast<char>(0xFFFD);
    }
}

std::string UnicodeService::normalize(const char* text) const {
    return text ? std::string(text) : "";
}

int UnicodeService::getBidiLevel(const char*, int baseLevel, int*) const {
    return baseLevel;
}

bool UnicodeService::isMark(char32_t u) const {
    // Unicode Mark ranges (simplified)
    // General combining marks: 0x0300-0x036F, 0x1DC0-0x1DFF, 0x20D0-0x20FF, 0xFE20-0xFE2F
    return (u >= 0x0300 && u <= 0x036F) ||
           (u >= 0x1DC0 && u <= 0x1DFF) ||
           (u >= 0x20D0 && u <= 0x20FF) ||
           (u >= 0xFE20 && u <= 0xFE2F);
}

bool UnicodeService::isSpace(char32_t u) const {
    return u == 0x20 || u == 0x09 || u == 0x0A || u == 0x0D || u == 0xA0 || u == 0x3000;
}

bool UnicodeService::isEmoji(char32_t u) const {
    // Simplified emoji detection
    return (u >= 0x1F600 && u <= 0x1F64F) ||
           (u >= 0x1F300 && u <= 0x1F5FF) ||
           (u >= 0x1F680 && u <= 0x1F6FF) ||
           (u >= 0x1F900 && u <= 0x1F9FF) ||
           (u >= 0x2600 && u <= 0x26FF) ||
           (u >= 0x2700 && u <= 0x27BF);
}

bool UnicodeService::shouldBreakGrapheme(char32_t, char32_t, int*) const {
    return true;
}

void UnicodeService::getLineBreaks(const char*, size_t, const char*, std::vector<char>&,
                                   SatoruCacheManager*) const {
    // No-op stub
}

void UnicodeService::clearCache() {}

char32_t UnicodeService::getVerticalSubstitution(char32_t u) const { return u; }
bool UnicodeService::isVerticalUpright(char32_t) const { return true; }
bool UnicodeService::isVerticalPunctuation(char32_t) const { return false; }

}  // namespace satoru
