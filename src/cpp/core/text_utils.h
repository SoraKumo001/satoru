#ifndef TEXT_UTILS_H
#define TEXT_UTILS_H

#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "include/core/SkFont.h"

namespace satoru {

struct MeasureResult {
    double width;
    size_t length;  // bytes processed that fit
    bool fits;      // true if all text fits within max_width
    const char* last_safe_pos; // pointer to the end of the last character that fits
};

// Decodes a UTF-8 character and advances the pointer.
char32_t decode_utf8_char(const char** ptr);

// Measures the text width. If max_width is provided (>= 0), stops when width exceeds max_width.
MeasureResult measure_text(const char* text, font_info* fi, double max_width = -1.0,
                           std::set<char32_t>* used_codepoints = nullptr);

// Helper to calculate full text width (wrapper around measure_text).
double text_width(const char* text, font_info* fi, std::set<char32_t>* used_codepoints = nullptr);

// Ellipsizes the text to fit within max_width.
std::string ellipsize_text(const char* text, font_info* fi, double max_width,
                           std::set<char32_t>* used_codepoints = nullptr);

}  // namespace satoru

#endif  // TEXT_UTILS_H
