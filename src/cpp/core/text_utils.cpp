#include "text_utils.h"

#include "core/text/text_layout.h"
#include "core/text/unicode_service.h"
#include "satoru_context.h"

namespace satoru {

char32_t decode_utf8_char(const char** ptr) {
    // Note: This is now a wrapper. Ideally callers should use UnicodeService directly.
    // But since we don't have ctx here, we need a static way or just keep it for now.
    // For now, let's use a temporary UnicodeService or move it to a static utility.
    UnicodeService svc;
    return svc.decodeUtf8(ptr);
}

std::string normalize_utf8(const char* text) {
    UnicodeService svc;
    return svc.normalize(text);
}

MeasureResult measure_text(SatoruContext* ctx, const char* text, font_info* fi, double max_width,
                           std::set<char32_t>* used_codepoints) {
    return TextLayout::measureText(ctx, text, fi, litehtml::writing_mode_horizontal_tb, max_width, used_codepoints);
}

double text_width(SatoruContext* ctx, const char* text, font_info* fi,
                  std::set<char32_t>* used_codepoints) {
    return TextLayout::measureText(ctx, text, fi, litehtml::writing_mode_horizontal_tb, -1.0, used_codepoints).width;
}

std::string ellipsize_text(SatoruContext* ctx, const char* text, font_info* fi, double max_width,
                           std::set<char32_t>* used_codepoints) {
    return TextLayout::ellipsizeText(ctx, text, fi, litehtml::writing_mode_horizontal_tb, max_width, used_codepoints);
}

int get_bidi_level(const char* text, int base_level, int* last_level) {
    UnicodeService svc;
    return svc.getBidiLevel(text, base_level, last_level);
}

}  // namespace satoru

