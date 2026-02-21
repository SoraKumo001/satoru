#ifndef TEXT_UTILS_H
#define TEXT_UTILS_H

#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "include/core/SkFont.h"
#include "text/text_layout.h"

class SatoruContext;

namespace satoru {

// Decodes a UTF-8 character and advances the pointer.
char32_t decode_utf8_char(const char** ptr);

// Normalizes a UTF-8 string to NFC.
std::string normalize_utf8(const char* text);

// Measures the text width. If max_width is provided (>= 0), stops when width exceeds max_width.
MeasureResult measure_text(SatoruContext* ctx, const char* text, font_info* fi,
                           double max_width = -1.0, std::set<char32_t>* used_codepoints = nullptr);

// Helper to calculate full text width (wrapper around measure_text).
double text_width(SatoruContext* ctx, const char* text, font_info* fi,
                  std::set<char32_t>* used_codepoints = nullptr);

// Ellipsizes the text to fit within max_width.
std::string ellipsize_text(SatoruContext* ctx, const char* text, font_info* fi, double max_width,
                           std::set<char32_t>* used_codepoints = nullptr);

int get_bidi_level(const char* text, int base_level, int* last_level = nullptr);

}  // namespace satoru

#endif  // TEXT_UTILS_H
