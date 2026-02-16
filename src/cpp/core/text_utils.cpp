#include "text_utils.h"

#include <algorithm>
#include <iostream>

#include "include/core/SkFont.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkTypeface.h"

namespace satoru {

char32_t decode_utf8_char(const char **ptr) {
    const uint8_t *p = (const uint8_t *)*ptr;
    if (!p || !*p) return 0;

    uint32_t cp = 0;
    uint8_t b1 = *p++;

    if (b1 < 0x80) {
        cp = b1;
    } else if ((b1 & 0xE0) == 0xC0) {
        cp = (b1 & 0x1F) << 6;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F);
    } else if ((b1 & 0xF0) == 0xE0) {
        cp = (b1 & 0x0F) << 12;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F);
    } else if ((b1 & 0xF8) == 0xF0) {
        cp = (b1 & 0x07) << 18;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F) << 12;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F);
    }

    *ptr = (const char *)p;
    return (char32_t)cp;
}

MeasureResult measure_text(const char *text, font_info *fi, double max_width,
                           std::set<char32_t> *used_codepoints) {
    MeasureResult result = {0.0, 0, true, text};
    if (!text || !*text || !fi || fi->fonts.empty()) return result;

    const char *p = text;
    const char *run_start = p;
    SkFont *current_font = fi->fonts[0];  // Optimistically assume first font

    // If max_width is negative, treat it as infinite
    bool limit_width = max_width >= 0;
    
    // Helper to commit run
    auto commit_run = [&](const char* end_ptr) -> bool {
        if (current_font && end_ptr > run_start) {
            double run_w = current_font->measureText(run_start, end_ptr - run_start, SkTextEncoding::kUTF8);
            if (limit_width && result.width + run_w > max_width) {
                // Run exceeds max width, need to find split point
                const char* rp = run_start;
                while (rp < end_ptr) {
                    const char* next_rp = rp;
                    char32_t u_inner = decode_utf8_char(&next_rp);
                    (void)u_inner; // suppress unused warning if not used
                    double char_w = current_font->measureText(rp, next_rp - rp, SkTextEncoding::kUTF8);
                    if (result.width + char_w > max_width) {
                        result.fits = false;
                        result.last_safe_pos = rp;
                        result.length = rp - text;
                        return false; // stop measuring
                    }
                    result.width += char_w;
                    result.last_safe_pos = next_rp;
                    rp = next_rp;
                }
                // Should not reach here if logic is correct unless float precision issues?
                // Just in case, add full run if loop completed without break (e.g. strict equality?)
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
        char32_t u;
        
        // Fast path for ASCII
        if ((unsigned char)*p < 0x80) {
            u = (unsigned char)*p;
            next_p++;
        } else {
            u = decode_utf8_char(&next_p);
        }

        if (used_codepoints) used_codepoints->insert(u);

        SkFont *font = nullptr;
        
        // Check current font first
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

    // Commit final run
    if (!commit_run(p)) return result;
    
    result.length = p - text;
    result.last_safe_pos = p;
    
    // Adjust for floating point precision issues (legacy behavior)
    if (result.width > 0) {
        result.width -= 0.001;
    }
    
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
    const char* ellipsis = "...";
    double ellipsis_width = text_width(ellipsis, fi, used_codepoints);
    
    if (ellipsis_width >= max_width) {
        return std::string(ellipsis);
    }
    
    double available_width = max_width - ellipsis_width;
    
    // Re-measure with stricter limit
    // Note: We already measured partially, but reusing `measure_text` is cleaner than manually doing it.
    // Ideally we could resume or optimize, but this is simple and robust.
    MeasureResult part_res = measure_text(text, fi, available_width, nullptr); // codepoints already collected in first pass?
    // Actually, if we pass used_codepoints again, we just re-insert same ones mostly.
    
    std::string result(text, part_res.length); // length is bytes processed that fit
    result += ellipsis;
    
    return result;
}

}  // namespace satoru
