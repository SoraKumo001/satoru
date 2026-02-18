#include "text_utils.h"

#include <linebreak.h>
#include <utf8proc.h>

#include <algorithm>

#include "bridge/bridge_types.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkTypeface.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "satoru_context.h"
#include "utils/skunicode_satoru.h"

namespace satoru {

char32_t decode_utf8_char(const char** ptr) {
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

std::string normalize_utf8(const char* text) {
    if (!text || !*text) return "";
    utf8proc_uint8_t* retval = nullptr;
    utf8proc_ssize_t len = utf8proc_map((const utf8proc_uint8_t*)text, 0, &retval,
                                        (utf8proc_option_t)UTF8PROC_COMPOSE);
    if (len < 0 || !retval) return std::string(text);
    std::string result((const char*)retval);
    free(retval);
    return result;
}

namespace {
class SatoruFontRunIterator : public SkShaper::FontRunIterator {
   public:
    struct CharFont {
        size_t len;
        SkFont font;
    };

    SatoruFontRunIterator(const std::vector<CharFont>& charFonts)
        : m_charFonts(charFonts), m_currentPos(0), m_currentIndex(0) {}

    void consume() override {
        if (m_currentIndex < m_charFonts.size()) {
            m_currentPos += m_charFonts[m_currentIndex].len;
            m_currentIndex++;
        }
    }

    size_t endOfCurrentRun() const override {
        return m_currentPos +
               (m_currentIndex < m_charFonts.size() ? m_charFonts[m_currentIndex].len : 0);
    }

    bool atEnd() const override { return m_currentIndex >= m_charFonts.size(); }

    const SkFont& currentFont() const override {
        return m_charFonts[std::min(m_currentIndex, m_charFonts.size() - 1)].font;
    }

   private:
    const std::vector<CharFont>& m_charFonts;
    size_t m_currentPos;
    size_t m_currentIndex;
};

class WidthRunHandler : public SkShaper::RunHandler {
   public:
    WidthRunHandler() : fWidth(0) {}
    void beginLine() override {}
    void runInfo(const RunInfo& info) override { fWidth += info.fAdvance.fX; }
    void commitRunInfo() override {}
    Buffer runBuffer(const RunInfo& info) override {
        return {nullptr, nullptr, nullptr, nullptr, {0, 0}};
    }
    void commitRunBuffer(const RunInfo&) override {}
    void commitLine() override {}
    double width() const { return fWidth; }

   private:
    double fWidth;
};

// Records glyph positions and their corresponding UTF-8 offsets
class DetailedWidthRunHandler : public SkShaper::RunHandler {
   public:
    struct GlyphInfo {
        size_t utf8_offset;
        float advance;
    };

    DetailedWidthRunHandler(size_t total_len) : fWidth(0), fTotalLen(total_len) {}
    void beginLine() override {}
    void runInfo(const RunInfo& info) override { fWidth += info.fAdvance.fX; }
    void commitRunInfo() override {}
    Buffer runBuffer(const RunInfo& info) override {
        fCurrentRunOffsets.resize(info.glyphCount);
        fCurrentRunPositions.resize(info.glyphCount + 1);
        return {nullptr, fCurrentRunPositions.data(), nullptr, fCurrentRunOffsets.data(), {0, 0}};
    }
    void commitRunBuffer(const RunInfo& info) override {
        for (size_t i = 0; i < info.glyphCount; ++i) {
            float advance = std::abs(fCurrentRunPositions[i + 1].fX - fCurrentRunPositions[i].fX);
            fGlyphs.push_back({fCurrentRunOffsets[i], advance});
        }
    }
    void commitLine() override {}

    double width() const { return fWidth; }

    // Returns the width of text up to the given UTF-8 offset
    double widthAtOffset(size_t offset) const {
        double w = 0;
        for (const auto& g : fGlyphs) {
            if (g.utf8_offset < offset) {
                w += g.advance;
            }
        }
        return w;
    }

   private:
    double fWidth;
    size_t fTotalLen;
    std::vector<uint32_t> fCurrentRunOffsets;
    std::vector<SkPoint> fCurrentRunPositions;
    std::vector<GlyphInfo> fGlyphs;
};
}  // namespace

MeasureResult measure_text(SatoruContext* ctx, const char* text, font_info* fi, double max_width,
                           std::set<char32_t>* used_codepoints) {
    MeasureResult result = {0.0, 0, true, text};
    if (!text || !*text || !fi || fi->fonts.empty() || !ctx) return result;

    SkShaper* shaper = ctx->getShaper();
    if (!shaper) return result;

    // 1. 各文字の使用フォントを判定
    std::vector<SatoruFontRunIterator::CharFont> charFonts;
    const char* p = text;
    SkFont* last_selected_font = nullptr;

    while (*p) {
        const char* prev_p = p;
        char32_t u = decode_utf8_char(&p);
        if (used_codepoints) used_codepoints->insert(u);

        SkFont* selected_font = nullptr;

        // 結合文字の場合は、直前のフォントを優先的に使用する
        auto category = utf8proc_category(u);
        bool is_mark = (category == UTF8PROC_CATEGORY_MN || category == UTF8PROC_CATEGORY_MC ||
                        category == UTF8PROC_CATEGORY_ME);

        if (is_mark && last_selected_font) {
            selected_font = last_selected_font;
        } else {
            for (auto f : fi->fonts) {
                SkGlyphID glyph = 0;
                SkUnichar sc = (SkUnichar)u;
                f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>(&sc, 1),
                                                   SkSpan<SkGlyphID>(&glyph, 1));
                if (glyph != 0) {
                    selected_font = f;
                    break;
                }
            }
            if (!selected_font) selected_font = fi->fonts[0];
        }

        last_selected_font = selected_font;

        SkFont font = *selected_font;
        if (fi->fake_bold) font.setEmbolden(true);

        if ((u >= 0x1F300 && u <= 0x1F9FF) || (u >= 0x2600 && u <= 0x26FF)) {
            font.setEmbeddedBitmaps(true);
            font.setHinting(SkFontHinting::kNone);
        }

        size_t char_len = (size_t)(p - prev_p);
        if (!charFonts.empty() && charFonts.back().font.getTypeface() == font.getTypeface() &&
            charFonts.back().font.getSize() == font.getSize() &&
            charFonts.back().font.isEmbolden() == font.isEmbolden() &&
            charFonts.back().font.isEmbeddedBitmaps() == font.isEmbeddedBitmaps()) {
            charFonts.back().len += char_len;
        } else {
            charFonts.push_back({char_len, font});
        }
    }

    size_t total_len = p - text;
    bool limit_width = max_width >= 0;

    DetailedWidthRunHandler handler(total_len);
    SatoruFontRunIterator fontRuns(charFonts);

    // Use SkUnicode-based iterators for better BiDi and Script detection
    int baseLevel = fi->is_rtl ? 1 : 0;
    uint8_t itemLevel = (uint8_t)get_bidi_level(text, baseLevel, nullptr);

    std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
        SkShaper::MakeBiDiRunIterator(text, total_len, itemLevel);
    if (!bidi) {
        bidi = std::make_unique<SkShaper::TrivialBiDiRunIterator>(itemLevel, total_len);
    }

    std::unique_ptr<SkShaper::ScriptRunIterator> script =
        SkShaper::MakeSkUnicodeHbScriptRunIterator(text, total_len);
    if (!script) {
        script = std::make_unique<SkShaper::TrivialScriptRunIterator>(
            SkSetFourByteTag('Z', 'y', 'y', 'y'), total_len);
    }

    std::unique_ptr<SkShaper::LanguageRunIterator> lang =
        SkShaper::MakeStdLanguageRunIterator(text, total_len);
    if (!lang) {
        lang = std::make_unique<SkShaper::TrivialLanguageRunIterator>("en", total_len);
    }

    shaper->shape(text, total_len, fontRuns, *bidi, *script, *lang, nullptr, 0, 1000000, &handler);

    if (!limit_width || handler.width() <= max_width + 0.01) {
        result.width = handler.width();
        result.length = total_len;
        result.last_safe_pos = text + total_len;
        return result;
    }

    // 幅制限がある場合は、一度のシェイピング結果から境界を探す
    result.fits = false;
    const char* walk_p = text;
    double last_w = 0;
    utf8proc_int32_t state = 0;

    while (*walk_p) {
        const char* prev_p = walk_p;
        char32_t u1 = decode_utf8_char(&walk_p);
        const char* next_p = walk_p;

        bool is_break = true;
        if (*next_p) {
            const char* temp_p = next_p;
            char32_t u2 = decode_utf8_char(&temp_p);
            is_break = utf8proc_grapheme_break_stateful(u1, u2, &state);
        }

        if (is_break) {
            size_t offset = next_p - text;
            double w = handler.widthAtOffset(offset);
            if (w > max_width + 0.01) {
                result.fits = false;
                result.width = last_w;
                result.length = prev_p - text;
                result.last_safe_pos = prev_p;
                return result;
            }
            last_w = w;
        }
    }

    result.width = last_w;
    result.length = walk_p - text;
    result.last_safe_pos = walk_p;
    return result;
}

double text_width(SatoruContext* ctx, const char* text, font_info* fi,
                  std::set<char32_t>* used_codepoints) {
    return measure_text(ctx, text, fi, -1.0, used_codepoints).width;
}

std::string ellipsize_text(SatoruContext* ctx, const char* text, font_info* fi, double max_width,
                           std::set<char32_t>* used_codepoints) {
    if (!text || !*text) return "";

    // First, check if full text fits
    MeasureResult full_res = measure_text(ctx, text, fi, max_width, used_codepoints);
    if (full_res.fits) return std::string(text);

    // Calculate ellipsis width
    const char* ellipsis = "...";
    double ellipsis_width = text_width(ctx, ellipsis, fi, used_codepoints);

    // Use a small epsilon to handle float precision issues
    const double epsilon = 0.1;

    if (max_width < ellipsis_width - epsilon) {
        // スペースが足りない場合でも、ellipsis モードなら最低限 ... を出す
        return ellipsis;
    }

    double available_width = std::max(0.0, max_width - ellipsis_width);

    // Re-measure with stricter limit
    MeasureResult part_res = measure_text(ctx, text, fi, available_width, nullptr);

    std::string result(text, part_res.length);  // length is bytes processed that fit
    result += ellipsis;

    return result;
}

int get_bidi_level(const char* text, int base_level, int* last_level) {
    if (!text || !*text) return base_level;

    const char* p = text;
    bool all_neutral = true;
    while (*p) {
        char32_t u = decode_utf8_char(&p);
        auto cat = utf8proc_category(u);

        // Skip punctuation, marks, and neutral characters
        if (cat == UTF8PROC_CATEGORY_PO || cat == UTF8PROC_CATEGORY_PD ||
            cat == UTF8PROC_CATEGORY_PS || cat == UTF8PROC_CATEGORY_PE ||
            cat == UTF8PROC_CATEGORY_PI || cat == UTF8PROC_CATEGORY_PF ||
            cat == UTF8PROC_CATEGORY_PC || cat == UTF8PROC_CATEGORY_MN ||
            cat == UTF8PROC_CATEGORY_MC || cat == UTF8PROC_CATEGORY_ME || cat == UTF8PROC_CATEGORY_ZS ||
            (u <= 0x2F) || (u >= 0x3A && u <= 0x40) || (u >= 0x5B && u <= 0x60) || (u >= 0x7B && u <= 0x7F)) {
            continue;
        }

        all_neutral = false;

        // Check if character is RTL (Arabic, Hebrew, Syriac, Thaana, N'Ko, etc.)
        bool is_rtl = (u >= 0x0590 && u <= 0x08FF) ||   // Hebrew, Arabic, Syriac, Arabic Supplement, Thaana, Samaritan, Mandaic, Arabic Extended-A
                      (u >= 0xFB50 && u <= 0xFDFF) ||   // Arabic Presentation Forms-A
                      (u >= 0xFE70 && u <= 0xFEFF) ||   // Arabic Presentation Forms-B
                      (u >= 0x10800 && u <= 0x10FFF) || // Various ancient RTL scripts
                      (u >= 0x1E800 && u <= 0x1EFFF);   // More RTL scripts

        int level;
        if (is_rtl) {
            level = 1;  // RTL Level 1
        } else {
            level = (base_level == 1) ? 2 : 0;  // LTR in RTL block is Level 2, otherwise 0
        }

        if (last_level) *last_level = level;
        return level;
    }

    if (all_neutral && last_level && *last_level != -1) {
        return *last_level;
    }

    return base_level;
}

}  // namespace satoru
