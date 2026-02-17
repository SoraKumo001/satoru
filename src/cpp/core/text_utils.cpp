#include "text_utils.h"

#include <linebreak.h>
#include <utf8proc.h>

#include <algorithm>

#include "include/core/SkFont.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkTypeface.h"

namespace satoru {

char32_t decode_utf8_char(const char **ptr) {
    if (!ptr || !*ptr || !**ptr) return 0;

    utf8proc_int32_t cp;
    utf8proc_ssize_t result = utf8proc_iterate((const utf8proc_uint8_t *)*ptr, -1, &cp);

    if (result < 0) {
        (*ptr)++;  // Advance one byte on error
        return 0xFFFD;
    }

    *ptr += result;
    return (char32_t)cp;
}

std::string normalize_utf8(const char *text) {
    if (!text || !*text) return "";
    utf8proc_uint8_t *retval = nullptr;
    utf8proc_ssize_t len = utf8proc_map((const utf8proc_uint8_t *)text, 0, &retval,
                                        (utf8proc_option_t)UTF8PROC_COMPOSE);
    if (len < 0 || !retval) return std::string(text);
    std::string result((const char *)retval);
    free(retval);
    return result;
}

MeasureResult measure_text(const char *text, font_info *fi, double max_width,
                           std::set<char32_t> *used_codepoints) {
    MeasureResult result = {0.0, 0, true, text};
    if (!text || !*text || !fi || fi->fonts.empty()) return result;

    const char *p = text;
    const char *run_start = p;
    SkFont *current_font = fi->fonts[0];

    bool limit_width = max_width >= 0;
    const double epsilon = 0.1;

    // Helper to commit run
    auto commit_run = [&](const char *end_ptr) -> bool {
        if (current_font && end_ptr > run_start) {
            double run_w =
                current_font->measureText(run_start, end_ptr - run_start, SkTextEncoding::kUTF8);
            if (limit_width && result.width + run_w > max_width + epsilon) {
                // Run exceeds max width, find split point at grapheme boundaries
                const char *rp = run_start;
                double last_w = 0;
                utf8proc_int32_t state = 0;
                while (rp < end_ptr) {
                    const char *next_rp = rp;
                    char32_t u_inner = decode_utf8_char(&next_rp);

                    // Check for grapheme break
                    bool is_break = true;
                    if (next_rp < end_ptr) {
                        const char *peek_p = next_rp;
                        char32_t next_u = decode_utf8_char(&peek_p);
                        is_break = utf8proc_grapheme_break_stateful(u_inner, next_u, &state);
                    }

                    if (is_break) {
                        double current_w = current_font->measureText(
                            run_start, next_rp - run_start, SkTextEncoding::kUTF8);
                        if (result.width + current_w > max_width + epsilon) {
                            result.fits = false;
                            result.last_safe_pos = rp;
                            result.length = rp - text;
                            result.width += last_w;
                            return false;
                        }
                        last_w = current_w;
                        result.last_safe_pos = next_rp;
                    }
                    rp = next_rp;
                }
                result.width += last_w;
                return true;
            } else {
                result.width += run_w;
                result.last_safe_pos = end_ptr;
            }
        }
        return true;
    };

    while (*p) {
        const char *next_p = p;
        char32_t u = decode_utf8_char(&next_p);

        if (used_codepoints) used_codepoints->insert(u);

        SkFont *font = nullptr;
        if (current_font) {
            SkGlyphID glyph = 0;
            SkUnichar sc = (SkUnichar)u;
            current_font->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>(&sc, 1),
                                                          SkSpan<SkGlyphID>(&glyph, 1));
            if (glyph != 0) font = current_font;
        }

        if (!font) {
            for (auto f : fi->fonts) {
                if (f == current_font) continue;
                SkGlyphID glyph = 0;
                SkUnichar sc = (SkUnichar)u;
                f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>(&sc, 1),
                                                   SkSpan<SkGlyphID>(&glyph, 1));
                if (glyph != 0) {
                    font = f;
                    break;
                }
            }
        }

        if (!font) font = fi->fonts[0];

        if (font != current_font) {
            if (!commit_run(p)) return result;
            run_start = p;
            current_font = font;
        }
        p = next_p;
    }

    if (!commit_run(p)) return result;

    result.length = p - text;
    result.last_safe_pos = p;

    return result;
}

double text_width(const char *text, font_info *fi, std::set<char32_t> *used_codepoints) {
    return measure_text(text, fi, -1.0, used_codepoints).width;
}

std::string ellipsize_text(const char *text, font_info *fi, double max_width,
                           std::set<char32_t> *used_codepoints) {
    if (!text || !*text) return "";

    // First, check if full text fits
    MeasureResult full_res = measure_text(text, fi, max_width, used_codepoints);
    if (full_res.fits) return std::string(text);

    // Calculate ellipsis width
    const char *ellipsis = "...";
    double ellipsis_width = text_width(ellipsis, fi, used_codepoints);

    // Use a small epsilon to handle float precision issues
    const double epsilon = 0.1;

    if (max_width < ellipsis_width - epsilon) {
        // スペースが足りない場合でも、ellipsis モードなら最低限 ... を出す
        return ellipsis;
    }

    double available_width = std::max(0.0, max_width - ellipsis_width);

    // Re-measure with stricter limit
    MeasureResult part_res = measure_text(text, fi, available_width, nullptr);

    std::string result(text, part_res.length);  // length is bytes processed that fit
    result += ellipsis;

    return result;
}

}  // namespace satoru
