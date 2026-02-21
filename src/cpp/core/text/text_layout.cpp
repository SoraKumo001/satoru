#include "text_layout.h"

#include <linebreak.h>

#include <algorithm>

#include "core/satoru_context.h"
#include "core/text/unicode_service.h"
#include "include/core/SkFont.h"
#include "include/core/SkTextBlob.h"
#include "modules/skshaper/include/SkShaper.h"

namespace satoru {

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

MeasureResult TextLayout::measureText(SatoruContext* ctx, const char* text, font_info* fi,
                                      double maxWidth, std::set<char32_t>* usedCodepoints) {
    MeasureResult result = {0.0, 0, true, text};
    if (!text || !*text || !fi || fi->fonts.empty() || !ctx) return result;

    SkShaper* shaper = ctx->getShaper();
    UnicodeService& unicode = ctx->getUnicodeService();
    if (!shaper) return result;

    // 1. 各文字の使用フォントを判定 (Font Selection Logic)
    std::vector<SatoruFontRunIterator::CharFont> charFonts;
    const char* p = text;
    SkFont last_font;
    bool has_last_font = false;

    while (*p) {
        const char* prev_p = p;
        char32_t u = unicode.decodeUtf8(&p);
        if (usedCodepoints) usedCodepoints->insert(u);

        SkFont font =
            ctx->fontManager.selectFont(u, fi, has_last_font ? &last_font : nullptr, unicode);
        last_font = font;
        has_last_font = true;

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
    bool limit_width = maxWidth >= 0;

    DetailedWidthRunHandler handler(total_len);
    SatoruFontRunIterator fontRuns(charFonts);

    // BiDi and Script detection using UnicodeService
    int baseLevel = fi->is_rtl ? 1 : 0;
    uint8_t itemLevel = (uint8_t)unicode.getBidiLevel(text, baseLevel, nullptr);

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

    if (!limit_width || handler.width() <= maxWidth + 0.01) {
        result.width = handler.width();
        result.length = total_len;
        result.last_safe_pos = text + total_len;
        return result;
    }

    // Width limit handling (grapheme break aware)
    result.fits = false;
    const char* walk_p = text;
    double last_w = 0;
    int state = 0;

    while (*walk_p) {
        const char* prev_p = walk_p;
        char32_t u1 = unicode.decodeUtf8(&walk_p);
        const char* next_p = walk_p;

        bool is_break = true;
        if (*next_p) {
            const char* temp_p = next_p;
            char32_t u2 = unicode.decodeUtf8(&temp_p);
            is_break = unicode.shouldBreakGrapheme(u1, u2, &state);
        }

        if (is_break) {
            size_t offset = next_p - text;
            double w = handler.widthAtOffset(offset);
            if (w > maxWidth + 0.01) {
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

std::string TextLayout::ellipsizeText(SatoruContext* ctx, const char* text, font_info* fi,
                                      double maxWidth, std::set<char32_t>* usedCodepoints) {
    if (!text || !*text) return "";

    // First, check if full text fits
    MeasureResult full_res = measureText(ctx, text, fi, maxWidth, usedCodepoints);
    if (full_res.fits) return std::string(text);

    // Calculate ellipsis width
    const char* ellipsis = "...";
    double ellipsis_width = measureText(ctx, ellipsis, fi, -1.0, usedCodepoints).width;

    // Use a small epsilon to handle float precision issues
    const double epsilon = 0.1;

    if (maxWidth < ellipsis_width - epsilon) {
        return ellipsis;
    }

    double available_width = std::max(0.0, maxWidth - ellipsis_width);

    // Re-measure with stricter limit
    MeasureResult part_res = measureText(ctx, text, fi, available_width, nullptr);

    std::string result(text, part_res.length);
    result += ellipsis;

    return result;
}

void TextLayout::splitText(SatoruContext* ctx, const char* text,
                           const std::function<void(const char*)>& onWord,
                           const std::function<void(const char*)>& onSpace) {
    if (!text || !*text || !ctx) return;

    UnicodeService& unicode = ctx->getUnicodeService();
    size_t len = strlen(text);
    std::vector<char> brks;
    unicode.getLineBreaks(text, len, brks);

    const char* p = text;
    const char* last_p = text;
    int prev_char_idx = -1;

    while (*p) {
        const char* next_p = p;
        char32_t c = unicode.decodeUtf8(&next_p);
        size_t idx = p - text;

        if (unicode.isSpace(c)) {
            if (p > last_p) {
                onWord(std::string(last_p, p - last_p).c_str());
            }
            onSpace(std::string(p, next_p - p).c_str());
            last_p = next_p;
            prev_char_idx = -1;
        } else {
            if (p > last_p && prev_char_idx != -1) {
                bool can_break = false;
                for (int i = prev_char_idx; i < (int)idx; ++i) {
                    if (brks[i] == LINEBREAK_ALLOWBREAK || brks[i] == LINEBREAK_MUSTBREAK) {
                        can_break = true;
                        break;
                    }
                }
                if (can_break) {
                    onWord(std::string(last_p, p - last_p).c_str());
                    last_p = p;
                }
            }
            prev_char_idx = (int)idx;
        }
        p = next_p;
    }

    if (p > last_p) {
        onWord(std::string(last_p, p - last_p).c_str());
    }
}

}  // namespace satoru
