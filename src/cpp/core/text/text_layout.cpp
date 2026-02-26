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
                         litehtml::writing_mode mode)
        : fInner(inner), fResult(result), fMode(mode) {
        fResult.width = 0;
    }
    void beginLine() override {
        if (fInner) fInner->beginLine();
    }
    void runInfo(const SkShaper::RunHandler::RunInfo& info) override {
        if (fMode == litehtml::writing_mode_horizontal_tb) {
            fResult.width += info.fAdvance.fX;
        } else {
            fResult.width += info.fAdvance.fY;
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
        if (fInner) {
            if (fMode != litehtml::writing_mode_horizontal_tb && fCurrentBuffer.positions) {
                for (size_t i = 0; i < info.glyphCount; ++i) {
                    float oldX = fCurrentBuffer.positions[i].fX;
                    float oldY = fCurrentBuffer.positions[i].fY;
                    fCurrentBuffer.positions[i].fX = -oldY;
                    fCurrentBuffer.positions[i].fY = oldX;
                }
                SATORU_LOG_INFO("Vertical Writing: Rotated %d glyphs", (int)info.glyphCount);
            }
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
    SkShaper::RunHandler::Buffer fCurrentBuffer;
};

// Original DetailedWidthRunHandler logic for measureText widthAtOffset
class OffsetWidthRunHandler : public SkShaper::RunHandler {
   public:
    struct GlyphInfo {
        size_t utf8_offset;
        float advance;
    };

    OffsetWidthRunHandler(litehtml::writing_mode mode) : fWidth(0), fMode(mode) {}
    void beginLine() override {}
    void runInfo(const RunInfo& info) override {
        if (fMode == litehtml::writing_mode_horizontal_tb) {
            fWidth += info.fAdvance.fX;
        } else {
            fWidth += info.fAdvance.fY;
        }
    }
    void commitRunInfo() override {}
    Buffer runBuffer(const RunInfo& info) override {
        fCurrentRunOffsets.resize(info.glyphCount);
        fCurrentRunPositions.resize(info.glyphCount + 1);
        return {nullptr, fCurrentRunPositions.data(), nullptr, fCurrentRunOffsets.data(), {0, 0}};
    }
    void commitRunBuffer(const RunInfo& info) override {
        for (size_t i = 0; i < info.glyphCount; ++i) {
            float advance;
            if (fMode == litehtml::writing_mode_horizontal_tb) {
                advance = std::abs(fCurrentRunPositions[i + 1].fX - fCurrentRunPositions[i].fX);
            } else {
                advance = std::abs(fCurrentRunPositions[i + 1].fY - fCurrentRunPositions[i].fY);
            }
            fGlyphs.push_back({fCurrentRunOffsets[i], advance});
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
    std::vector<uint32_t> fCurrentRunOffsets;
    std::vector<SkPoint> fCurrentRunPositions;
    std::vector<GlyphInfo> fGlyphs;
};
}  // namespace

MeasureResult TextLayout::measureText(SatoruContext* ctx, const char* text, font_info* fi,
                                      litehtml::writing_mode mode, double maxWidth, std::set<char32_t>* usedCodepoints) {
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

        if (MeasureResult* cached = ctx->measurementCache.get(key)) {
            MeasureResult res = *cached;
            res.last_safe_pos = text + res.length;
            return res;
        }
    }

    size_t total_len = strlen(text);
    bool limit_width = maxWidth >= 0;

    TextAnalysis analysis = analyzeText(ctx, text, total_len, fi, mode, usedCodepoints);

    // Use OffsetWidthRunHandler directly for measureText to support widthAtOffset
    UnicodeService& unicode = ctx->getUnicodeService();
    SkShaper* shaper = ctx->getShaper();
    if (!shaper) return result;

    std::vector<CharFont> charFonts;
    for (const auto& ca : analysis.chars) {
        if (!charFonts.empty() && charFonts.back().font == ca.font) {
            charFonts.back().len += ca.len;
        } else {
            charFonts.push_back({ca.len, ca.font});
        }
    }

    OffsetWidthRunHandler handler(mode);
    SatoruFontRunIterator fontRuns(charFonts);
    uint8_t itemLevel = analysis.bidi_level;
    std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
        SkShaper::MakeBiDiRunIterator(text, total_len, itemLevel);
    if (!bidi) bidi = std::make_unique<SkShaper::TrivialBiDiRunIterator>(itemLevel, total_len);
    std::unique_ptr<SkShaper::ScriptRunIterator> script =
        SkShaper::MakeSkUnicodeHbScriptRunIterator(text, total_len);
    if (!script)
        script = std::make_unique<SkShaper::TrivialScriptRunIterator>(
            SkSetFourByteTag('Z', 'y', 'y', 'y'), total_len);
    std::unique_ptr<SkShaper::LanguageRunIterator> lang =
        SkShaper::MakeStdLanguageRunIterator(text, total_len);
    if (!lang) lang = std::make_unique<SkShaper::TrivialLanguageRunIterator>("en", total_len);

    shaper->shape(text, total_len, fontRuns, *bidi, *script, *lang, nullptr, 0, 1000000, &handler);

    if (!limit_width || handler.width() <= maxWidth + 0.01) {
        result.width = handler.width();
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
        ctx->measurementCache.put(key, result);
    }

    return result;
}

TextAnalysis TextLayout::analyzeText(SatoruContext* ctx, const char* text, size_t len,
                                     font_info* fi, litehtml::writing_mode mode, std::set<char32_t>* usedCodepoints) {
    TextAnalysis analysis;
    if (!text || !len || !ctx) return analysis;

    UnicodeService& unicode = ctx->getUnicodeService();
    analysis.line_breaks.resize(len);
    unicode.getLineBreaks(text, len, nullptr, analysis.line_breaks);

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
        ca.len = p - prev_p;

        if (mode != litehtml::writing_mode_horizontal_tb) {
            ca.codepoint = unicode.getVerticalSubstitution(ca.codepoint);
        }

        if (usedCodepoints) usedCodepoints->insert(ca.codepoint);

        ca.is_emoji = unicode.isEmoji(ca.codepoint);
        ca.is_mark = unicode.isMark(ca.codepoint);

        ca.font = ctx->fontManager.selectFont(ca.codepoint, fi,
                                              has_last_font ? &last_font : nullptr, unicode);
        last_font = ca.font;
        has_last_font = true;

        analysis.chars.push_back(ca);
    }

    return analysis;
}

ShapedResult TextLayout::shapeText(SatoruContext* ctx, const char* text, size_t len, font_info* fi, 
                                  litehtml::writing_mode mode, std::set<char32_t>* usedCodepoints) {
    if (!text || !len || !fi || fi->fonts.empty() || !ctx) return {0.0, nullptr};

    ShapingKey key;
    key.text.assign(text, len);
    key.font_family = fi->desc.family;
    key.font_size = (float)fi->desc.size;
    key.font_weight = fi->desc.weight;
    key.italic = (fi->desc.style == litehtml::font_style_italic);
    key.is_rtl = fi->is_rtl;
    key.mode = mode;

    if (ShapedResult* cached = ctx->shapingCache.get(key)) {
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

    std::vector<CharFont> charFonts;
    for (const auto& ca : analysis.chars) {
        if (!charFonts.empty() && charFonts.back().font == ca.font) {
            charFonts.back().len += ca.len;
        } else {
            charFonts.push_back({ca.len, ca.font});
        }
    }

    ShapedResult result = {0.0, nullptr};
    SkShaper* shaper = ctx->getShaper();
    if (!shaper) return result;

    SkTextBlobBuilderRunHandler blobHandler(text, {0, 0});
    WidthProxyRunHandler handler(&blobHandler, result, mode);

    SatoruFontRunIterator fontRuns(charFonts);
    uint8_t itemLevel = analysis.bidi_level;
    std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
        SkShaper::MakeBiDiRunIterator(text, len, itemLevel);
    if (!bidi) bidi = std::make_unique<SkShaper::TrivialBiDiRunIterator>(itemLevel, len);
    std::unique_ptr<SkShaper::ScriptRunIterator> script =
        SkShaper::MakeSkUnicodeHbScriptRunIterator(text, len);
    if (!script)
        script = std::make_unique<SkShaper::TrivialScriptRunIterator>(
            SkSetFourByteTag('Z', 'y', 'y', 'y'), len);
    std::unique_ptr<SkShaper::LanguageRunIterator> lang =
        SkShaper::MakeStdLanguageRunIterator(text, len);
    if (!lang) lang = std::make_unique<SkShaper::TrivialLanguageRunIterator>("en", len);

    shaper->shape(text, len, fontRuns, *bidi, *script, *lang, nullptr, 0, 1000000, &handler);

    result.blob = blobHandler.makeBlob();

    ctx->shapingCache.put(key, result);
    return result;
}

std::string TextLayout::ellipsizeText(SatoruContext* ctx, const char* text, font_info* fi,
                                      litehtml::writing_mode mode, double maxWidth, std::set<char32_t>* usedCodepoints) {
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
    unicode.getLineBreaks(text, len, nullptr, brks);

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
