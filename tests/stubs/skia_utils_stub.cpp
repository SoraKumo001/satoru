// Stub implementation of skia_utils functions needed by font_manager.cpp
#include <core/SkRRect.h>
#include <core/SkRefCnt.h>
#include <litehtml.h>
#include <string>
#include <vector>

// Forward declare functions from skia_utils.h
std::string clean_font_name(const char* name);

std::string clean_font_name(const char* name) {
    if (!name) return "";
    std::string result;
    for (const char* p = name; *p; ++p) {
        char c = *p;
        if (c == '\'' || c == '\"') continue;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        result += (char)std::tolower((unsigned char)c);
    }
    return result;
}

std::string base64_encode(const uint8_t* data, size_t len) {
    // Stub — not needed for font_manager tests
    return "";
}

std::vector<uint8_t> base64_decode(const std::string& in) {
    return {};
}

std::string url_decode(const std::string& in) {
    return in;
}

SkRRect make_rrect(const litehtml::position& pos, const litehtml::border_radiuses& radius) {
    return SkRRect();
}
