#include "text_layout.h"

#include <linebreak.h>

#include <algorithm>

#include "core/satoru_context.h"
#include "core/text/text_types.h"
#include "core/text/unicode_service.h"
#include "include/core/SkFont.h"
#include "include/core/SkTextBlob.h"
#include "modules/skshaper/include/SkShaper.h"
#include "utils/logging.h"

namespace satoru {

namespace {
// Records total width while delegating to another handler
class WidthProxyRunHandler : public SkShaper::RunHandler {
   public:
    WidthProxyRunHandler(SkShaper::RunHandler* inner, ShapedResult& result,
                         litehtml::writing_mode mode, float letter_spacing, float word_spacing,
                         const TextAnalysis& analysis)
        : fInner(inner),
          fResult(result),
          fMode(mode),
          fLetterSpacing(letter_spacing),
          fWordSpacing(word_spacing),
          fAnalysis(analysis) {
        fResult.width = 0;
    }
    void beginLine() override {
        if (fInner) fInner->beginLine();
    }
    void runInfo(const SkShaper::RunHandler::RunInfo& info) override {
        bool is_vertical = (fMode == litehtml::writing_mode_vertical_rl ||
                            fMode == litehtml::writing_mode_vertical_lr);
        bool is_upright = false;
        if (is_vertical && info.utf8Range.fSize > 0) {
            // Find the character analysis for this run. Since runs are split by upright status,
            // we can just check the first character of the run.
            for (const auto& ca : fAnalysis.chars) {
                if (ca.offset >= info.utf8Range.fBegin &&
                    ca.offset < info.utf8Range.fBegin + info.utf8Range.fSize) {
                    is_upright = ca.is_vertical_upright;
                    break;
                }
            }
        }

        if (is_upright) {
            float font_size = info.fFont.getSize();
            fResult.width += info.glyphCount * (font_size + fLetterSpacing);
        } else {
            fResult.width += info.fAdvance.fX;
            fResult.width += info.glyphCount * fLetterSpacing;
        }

        if (fInner) fInner->runInfo(info);
    }
    void commitRunInfo() override {
        if (fInner) fInner->commitRunInfo();
    }
    Buffer runBuffer(const SkShaper::RunHandler::RunInfo& info) override {
        if (fInner) {
            fCurrentBuffer = fInner->runBuffer(info);
            return fCurrentBuffer;
        }
        return {nullptr, nullptr, nullptr, nullptr, {0, 0}};
    }
    void commitRunBuffer(const SkShaper::RunHandler::RunInfo& info) override {
        bool is_vertical = (fMode == litehtml::writing_mode_vertical_rl ||
                            fMode == litehtml::writing_mode_vertical_lr);
        bool is_upright = false;
        if (is_vertical && info.utf8Range.fSize > 0) {
            for (const auto& ca : fAnalysis.chars) {
                if (ca.offset >= info.utf8Range.fBegin &&
                    ca.offset < info.utf8Range.fBegin + info.utf8Range.fSize) {
                    is_upright = ca.is_vertical_upright;
                    break;
                }
            }
        }

        if (is_upright && fCurrentBuffer.positions) {
            float font_size = info.fFont.getSize();
            for (size_t i = 0; i < info.glyphCount; ++i) {
                fCurrentBuffer.positions[i].fX = i * (font_size + fLetterSpacing);
                fCurrentBuffer.positions[i].fY = 0;
            }
        }

        if (fInner) {
            // Coordinate swap for vertical text will be handled by TextRenderer
            // using logical-to-physical mapping. We avoid low-level swapping here.
            fInner->commitRunBuffer(info);
        }
    }
    void commitLine() override {
        if (fInner) fInner->commitLine();
    }

   private:
    SkShaper::RunHandler* fInner;
    ShapedResult& fResult;
    litehtml::writing_mode fMode;
    float fLetterSpacing;
    float fWordSpacing;
    const TextAnalysis& fAnalysis;
    SkShaper::RunHandler::Buffer fCurrentBuffer;
};

// Original DetailedWidthRunHandler logic for measureText widthAtOffset
class OffsetWidthRunHandler : public SkShaper::RunHandler {
   public:
    struct GlyphInfo {
        size_t utf8_offset;
        float advance;
    };

    OffsetWidthRunHandler(litehtml::writing_mode mode, float letter_spacing, float word_spacing,
                          const TextAnalysis& analysis)
        : fWidth(0),
          fMode(mode),
          fLetterSpacing(letter_spacing),
          fWordSpacing(word_spacing),
          fAnalysis(analysis) {}
    void beginLine() override {}
    void runInfo(const RunInfo& info) override {
        bool is_vertical = (fMode == litehtml::writing_mode_vertical_rl ||
                            fMode == litehtml::writing_mode_vertical_lr);
        bool is_upright = false;
        if (is_vertical && info.utf8Range.fSize > 0) {
            for (const auto& ca : fAnalysis.chars) {
                if (ca.offset >= info.utf8Range.fBegin &&
                    ca.offset < info.utf8Range.fBegin + info.utf8Range.fSize) {
                    is_upright = ca.is_vertical_upright;
                    break;
                }
            }
        }

        if (is_upright) {
            float font_size = info.fFont.getSize();
            fWidth += info.glyphCount * (font_size + fLetterSpacing);
        } else {
            fWidth += info.fAdvance.fX + (info.glyphCount * fLetterSpacing);
        }
    }
    void commitRunInfo() override {}
    Buffer runBuffer(const RunInfo& info) override {
        fCurrentRunOffsets.resize(info.glyphCount);
        fCurrentRunPositions.resize(info.glyphCount + 1);
        fCurrentFontSize = info.fFont.getSize();
        return {nullptr, fCurrentRunPositions.data(), nullptr, fCurrentRunOffsets.data(), {0, 0}};
    }
    void commitRunBuffer(const RunInfo& info) override {
        bool is_vertical = (fMode == litehtml::writing_mode_vertical_rl ||
                            fMode == litehtml::writing_mode_vertical_lr);

        for (size_t i = 0; i < info.glyphCount; ++i) {
            bool is_upright = false;
            if (is_vertical) {
                for (const auto& ca : fAnalysis.chars) {
                    if (ca.offset == fCurrentRunOffsets[i]) {
                        is_upright = ca.is_vertical_upright;
                        break;
                    }
                }
            }

            float advance;
            if (is_upright) {
                advance = fCurrentFontSize + fLetterSpacing;
            } else {
                advance = std::abs(fCurrentRunPositions[i + 1].fX - fCurrentRunPositions[i].fX);
                advance += fLetterSpacing;
            }

            fGlyphs.push_back({(size_t)fCurrentRunOffsets[i], advance});
        }
    }
    void commitLine() override {}

    double width() const { return fWidth; }
    double widthAtOffset(size_t offset) const {
        double w = 0;
        for (const auto& g : fGlyphs) {
            if (g.utf8_offset < offset) w += g.advance;
        }
        return w;
    }

   private:
    double fWidth;
    litehtml::writing_mode fMode;
    float fLetterSpacing;
    float fWordSpacing;
    const TextAnalysis& fAnalysis;
    float fCurrentFontSize;
    std::vector<uint32_t> fCurrentRunOffsets;
    std::vector<SkPoint> fCurrentRunPositions;
    std::vector<GlyphInfo> fGlyphs;
};
}  // namespace

MeasureResult TextLayout::measureText(SatoruContext* ctx, const char* text, font_info* fi,
                                      litehtml::writing_mode mode, double maxWidth,
                                      std::set<char32_t>* usedCodepoints) {
    MeasureResult result = {0.0, 0, true, text};
    if (!text || !*text || !fi || fi->fonts.empty() || !ctx) return result;

    MeasureKey key;
    bool canCache = (usedCodepoints == nullptr);
    if (canCache) {
        key.text = text;
        key.font_family = fi->desc.family;
        key.font_size = (float)fi->desc.size;
        key.font_weight = fi->desc.weight;
        key.italic = (fi->desc.style == litehtml::font_style_italic);
        key.maxWidth = maxWidth;
        key.mode = mode;
        key.orientation = fi->desc.orientation;
        key.textCombineUpright = fi->desc.text_combine_upright;
        key.letterSpacing = (float)fi->desc.letter_spacing;
        key.wordSpacing = (float)fi->desc.word_spacing;

        if (MeasureResult* cached = ctx->cacheManager.measureCache.get(key)) {
            MeasureResult res = *cached;
            res.last_safe_pos = text + res.length;
            return res;
        }
    }

    size_t total_len = strlen(text);
    bool limit_width = maxWidth >= 0;

    if (mode != litehtml::writing_mode_horizontal_tb &&
        fi->desc.text_combine_upright == litehtml::text_combine_upright_all) {
        // For text-combine-upright: all, we treat the whole text as a single run 
        // that is rotated and potentially scaled.
        // Its logical inline-size (physical height) should be the height of a single character 
        // (the line-height of the combined block).
        result.width = (double)fi->desc.size;
        result.length = total_len;
        result.fits = true;
        result.last_safe_pos = text + total_len;
        
        if (canCache) {
            ctx->cacheManager.measureCache.put(key, result);
        }
        return result;
    }

    TextAnalysis analysis = analyzeText(ctx, text, total_len, fi, mode, usedCodepoints);
    const char* shape_text = analysis.substituted_text.c_str();
    size_t shape_len = analysis.substituted_text.size();

    // Use OffsetWidthRunHandler directly for measureText to support widthAtOffset
    UnicodeService& unicode = ctx->getUnicodeService();
    SkShaper* shaper = ctx->getShaper();
    if (!shaper) return result;

    std::vector<CharFont> charFonts;
    for (const auto& ca : analysis.chars) {
        if (!charFonts.empty() && charFonts.back().font == ca.font &&
            charFonts.back().is_vertical_upright == ca.is_vertical_upright &&
            charFonts.back().is_vertical_punctuation == ca.is_vertical_punctuation) {
            charFonts.back().len += ca.len;
        } else {
            charFonts.push_back(
                {ca.len, ca.font, ca.is_vertical_upright, ca.is_vertical_punctuation});
        }
    }

    OffsetWidthRunHandler handler(mode, (float)fi->desc.letter_spacing,
                                  (float)fi->desc.word_spacing, analysis);
    SatoruFontRunIterator fontRuns(charFonts);
    uint8_t itemLevel = analysis.bidi_level;
    std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
        SkShaper::MakeBiDiRunIterator(shape_text, shape_len, itemLevel);
    if (!bidi) bidi = std::make_unique<SkShaper::TrivialBiDiRunIterator>(itemLevel, shape_len);
    std::unique_ptr<SkShaper::ScriptRunIterator> script =
        SkShaper::MakeSkUnicodeHbScriptRunIterator(shape_text, shape_len);
    if (!script)
        script = std::make_unique<SkShaper::TrivialScriptRunIterator>(
            SkSetFourByteTag('Z', 'y', 'y', 'y'), shape_len);
    std::unique_ptr<SkShaper::LanguageRunIterator> lang =
        SkShaper::MakeStdLanguageRunIterator(shape_text, shape_len);
    if (!lang) lang = std::make_unique<SkShaper::TrivialLanguageRunIterator>("en", shape_len);

    shaper->shape(shape_text, shape_len, fontRuns, *bidi, *script, *lang, nullptr, 0, 1000000,
                  &handler);

    if (!limit_width || handler.width() <= maxWidth + 0.01) {
        result.width = std::min(handler.width(), limit_width ? maxWidth : handler.width());
        result.length = total_len;
    } else {
        // Width limit handling (grapheme break aware)
        result.fits = false;
        double last_w = 0;
        int state = 0;

        for (size_t i = 0; i < analysis.chars.size(); ++i) {
            const auto& ca = analysis.chars[i];
            bool is_break = true;
            if (i + 1 < analysis.chars.size()) {
                is_break = unicode.shouldBreakGrapheme(ca.codepoint,
                                                       analysis.chars[i + 1].codepoint, &state);
            }
            if (is_break) {
                size_t offset = ca.offset + ca.len;
                double w = handler.widthAtOffset(offset);
                if (w > maxWidth + 0.01) {
                    result.width = last_w;
                    result.length = ca.offset;
                    break;
                }
                last_w = w;
            }
        }
        if (result.fits) {
            result.width = last_w;
            result.length = total_len;
        }
    }

    result.last_safe_pos = text + result.length;
    if (canCache) {
        ctx->cacheManager.measureCache.put(key, result);
    }

    return result;
}

TextAnalysis TextLayout::analyzeText(SatoruContext* ctx, const char* text, size_t len,
                                     font_info* fi, litehtml::writing_mode mode,
                                     std::set<char32_t>* usedCodepoints) {
    TextAnalysis analysis;
    if (!text || !len || !ctx) return analysis;

    UnicodeService& unicode = ctx->getUnicodeService();
    analysis.line_breaks.resize(len);
    unicode.getLineBreaks(text, len, nullptr, analysis.line_breaks, &ctx->cacheManager);

    int baseLevel = fi->is_rtl ? 1 : 0;
    analysis.bidi_level = (uint8_t)unicode.getBidiLevel(text, baseLevel, nullptr);

    const char* p = text;
    const char* end = text + len;
    SkFont last_font;
    bool has_last_font = false;

    while (p < end) {
        TextCharAnalysis ca;
        ca.offset = p - text;
        const char* prev_p = p;
        ca.codepoint = unicode.decodeUtf8(&p);
        size_t original_len = p - prev_p;

        ca.font = ctx->fontManager.selectFont(ca.codepoint, fi,
                                              has_last_font ? &last_font : nullptr, unicode);

        if (mode != litehtml::writing_mode_horizontal_tb) {
            char32_t substituted = unicode.getVerticalSubstitution(ca.codepoint);
            if (substituted != ca.codepoint) {
                // Check if the current font or ANY fallback font supports the substituted codepoint
                bool supported = false;
                for (auto f : fi->fonts) {
                    if (f->getTypeface()->unicharToGlyph(substituted) != 0) {
                        supported = true;
                        ca.font = *f; // Switch to the font that supports it
                        break;
                    }
                }
                if (supported) {
                    ca.codepoint = substituted;
                }
            }
        }

        if (usedCodepoints) usedCodepoints->insert(ca.codepoint);

        size_t current_sub_offset = analysis.substituted_text.size();
        unicode.encodeUtf8(ca.codepoint, analysis.substituted_text);
        ca.len = analysis.substituted_text.size() - current_sub_offset;
        ca.offset = current_sub_offset;

        ca.is_emoji = unicode.isEmoji(ca.codepoint);
        ca.is_mark = unicode.isMark(ca.codepoint);

        if (mode == litehtml::writing_mode_horizontal_tb) {
            ca.is_vertical_upright = true;
            ca.is_vertical_punctuation = false;
        } else {
            ca.is_vertical_punctuation = unicode.isVerticalPunctuation(ca.codepoint);
            if (fi->desc.text_combine_upright == litehtml::text_combine_upright_all) {
                ca.is_vertical_upright = true;
            } else if (fi->desc.orientation == litehtml::text_orientation_upright) {
                ca.is_vertical_upright = true;
            } else if (fi->desc.orientation == litehtml::text_orientation_sideways) {
                ca.is_vertical_upright = false;
            } else {
                // mixed mode
                ca.is_vertical_upright = unicode.isVerticalUpright(ca.codepoint);
            }
            if (ca.codepoint > 32) {
                SATORU_LOG_INFO("DEBUG_LAYOUT: cp=%u, orient=%d, upright=%d", (unsigned int)ca.codepoint, (int)fi->desc.orientation, (int)ca.is_vertical_upright);
            }
        }

        last_font = ca.font;
        has_last_font = true;

        analysis.chars.push_back(ca);
    }

    return analysis;
}

ShapedResult TextLayout::shapeText(SatoruContext* ctx, const char* text, size_t len, font_info* fi,
                                   litehtml::writing_mode mode,
                                   std::set<char32_t>* usedCodepoints) {
    if (!text || !len || !fi || fi->fonts.empty() || !ctx) return {0.0, nullptr};

    ShapingKey key;
    key.text.assign(text, len);
    key.font_family = fi->desc.family;
    key.font_size = (float)fi->desc.size;
    key.font_weight = fi->desc.weight;
    key.italic = (fi->desc.style == litehtml::font_style_italic);
    key.is_rtl = fi->is_rtl;
    key.mode = mode;
    key.orientation = fi->desc.orientation;
    key.textCombineUpright = fi->desc.text_combine_upright;
    key.letterSpacing = (float)fi->desc.letter_spacing;
    key.wordSpacing = (float)fi->desc.word_spacing;

    if (ShapedResult* cached = ctx->cacheManager.shapingCache.get(key)) {
        if (usedCodepoints) {
            UnicodeService& unicode = ctx->getUnicodeService();
            const char* p = text;
            const char* p_end = text + len;
            while (p < p_end) {
                usedCodepoints->insert(unicode.decodeUtf8(&p));
            }
        }
        return *cached;
    }

    TextAnalysis analysis = analyzeText(ctx, text, len, fi, mode, usedCodepoints);
    const char* shape_text = analysis.substituted_text.c_str();
    size_t shape_len = analysis.substituted_text.size();

    std::vector<CharFont> charFonts;
    for (const auto& ca : analysis.chars) {
        if (!charFonts.empty() && charFonts.back().font == ca.font &&
            charFonts.back().is_vertical_upright == ca.is_vertical_upright &&
            charFonts.back().is_vertical_punctuation == ca.is_vertical_punctuation) {
            charFonts.back().len += ca.len;
        } else {
            charFonts.push_back(
                {ca.len, ca.font, ca.is_vertical_upright, ca.is_vertical_punctuation});
        }
    }

    ShapedResult result = {0.0, nullptr};
    SkShaper* shaper = ctx->getShaper();
    if (!shaper) return result;

    SkTextBlobBuilderRunHandler blobHandler(shape_text, {0, 0});
    WidthProxyRunHandler handler(&blobHandler, result, mode, (float)fi->desc.letter_spacing,
                                 (float)fi->desc.word_spacing, analysis);

    SatoruFontRunIterator fontRuns(charFonts);
    uint8_t itemLevel = analysis.bidi_level;
    std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
        SkShaper::MakeBiDiRunIterator(shape_text, shape_len, itemLevel);
    if (!bidi) bidi = std::make_unique<SkShaper::TrivialBiDiRunIterator>(itemLevel, shape_len);
    std::unique_ptr<SkShaper::ScriptRunIterator> script =
        SkShaper::MakeSkUnicodeHbScriptRunIterator(shape_text, shape_len);
    if (!script)
        script = std::make_unique<SkShaper::TrivialScriptRunIterator>(
            SkSetFourByteTag('Z', 'y', 'y', 'y'), shape_len);
    std::unique_ptr<SkShaper::LanguageRunIterator> lang =
        SkShaper::MakeStdLanguageRunIterator(shape_text, shape_len);
    if (!lang) lang = std::make_unique<SkShaper::TrivialLanguageRunIterator>("en", shape_len);

    shaper->shape(shape_text, shape_len, fontRuns, *bidi, *script, *lang, nullptr, 0, 1000000,
                  &handler);

    result.blob = blobHandler.makeBlob();

    ctx->cacheManager.shapingCache.put(key, result);
    return result;
}

std::string TextLayout::ellipsizeText(SatoruContext* ctx, const char* text, font_info* fi,
                                      litehtml::writing_mode mode, double maxWidth,
                                      std::set<char32_t>* usedCodepoints) {
    if (!text || !*text) return "";

    // First, check if full text fits
    MeasureResult full_res = measureText(ctx, text, fi, mode, maxWidth, usedCodepoints);
    if (full_res.fits) return std::string(text);

    // Calculate ellipsis width
    const char* ellipsis = "...";
    double ellipsis_width = measureText(ctx, ellipsis, fi, mode, -1.0, usedCodepoints).width;

    // Use a small epsilon to handle float precision issues
    const double epsilon = 0.1;

    if (maxWidth < ellipsis_width - epsilon) {
        return ellipsis;
    }

    double available_width = std::max(0.0, maxWidth - ellipsis_width);

    // Re-measure with stricter limit
    MeasureResult part_res = measureText(ctx, text, fi, mode, available_width, nullptr);

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
    unicode.getLineBreaks(text, len, nullptr, brks, &ctx->cacheManager);

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
