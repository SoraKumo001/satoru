#include "text_layout.h"

#include <linebreak.h>

#include <algorithm>

#include "core/satoru_context.h"
#include "core/text/text_types.h"
#include "core/text/unicode_service.h"
#include "include/core/SkFont.h"
#include "include/core/SkTextBlob.h"
#include "modules/skshaper/include/SkShaper.h"

namespace satoru {

namespace {
// Records total width while delegating to another handler
class WidthProxyRunHandler : public SkShaper::RunHandler {
   public:
    WidthProxyRunHandler(SkShaper::RunHandler* inner, ShapedResult& result)
        : fInner(inner), fResult(result) {
        fResult.width = 0;
    }
    void beginLine() override { fInner->beginLine(); }
    void runInfo(const SkShaper::RunHandler::RunInfo& info) override {
        fResult.width += info.fAdvance.fX;
        fInner->runInfo(info);
    }
    void commitRunInfo() override { fInner->commitRunInfo(); }
    Buffer runBuffer(const SkShaper::RunHandler::RunInfo& info) override {
        return fInner->runBuffer(info);
    }
    void commitRunBuffer(const SkShaper::RunHandler::RunInfo& info) override {
        fInner->commitRunBuffer(info);
    }
    void commitLine() override { fInner->commitLine(); }

   private:
    SkShaper::RunHandler* fInner;
    ShapedResult& fResult;
};

// Original DetailedWidthRunHandler logic for measureText widthAtOffset
class OffsetWidthRunHandler : public SkShaper::RunHandler {
   public:
    struct GlyphInfo {
        size_t utf8_offset;
        float advance;
    };

    OffsetWidthRunHandler() : fWidth(0) {}
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
    double widthAtOffset(size_t offset) const {
        double w = 0;
        for (const auto& g : fGlyphs) {
            if (g.utf8_offset < offset) w += g.advance;
        }
        return w;
    }

   private:
    double fWidth;
    std::vector<uint32_t> fCurrentRunOffsets;
    std::vector<SkPoint> fCurrentRunPositions;
    std::vector<GlyphInfo> fGlyphs;
};
}  // namespace

MeasureResult TextLayout::measureText(SatoruContext* ctx, const char* text, font_info* fi,
                                      double maxWidth, std::set<char32_t>* usedCodepoints) {
    MeasureResult result = {0.0, 0, true, text};
    if (!text || !*text || !fi || fi->fonts.empty() || !ctx) return result;

    size_t total_len = strlen(text);
    bool limit_width = maxWidth >= 0;

    // Use OffsetWidthRunHandler directly for measureText to support widthAtOffset
    UnicodeService& unicode = ctx->getUnicodeService();
    SkShaper* shaper = ctx->getShaper();
    if (!shaper) return result;

    std::vector<CharFont> charFonts;
    const char* walk_p = text;
    const char* walk_end = text + total_len;
    SkFont last_font;
    bool has_last_font = false;
    while (walk_p < walk_end) {
        const char* prev_walk = walk_p;
        char32_t u = unicode.decodeUtf8(&walk_p);
        if (usedCodepoints) usedCodepoints->insert(u);
        SkFont font =
            ctx->fontManager.selectFont(u, fi, has_last_font ? &last_font : nullptr, unicode);
        last_font = font;
        has_last_font = true;
        size_t char_len = (size_t)(walk_p - prev_walk);
        if (!charFonts.empty() && charFonts.back().font == font) {
            charFonts.back().len += char_len;
        } else {
            charFonts.push_back({char_len, font});
        }
    }

    OffsetWidthRunHandler handler;
    SatoruFontRunIterator fontRuns(charFonts);
    int baseLevel = fi->is_rtl ? 1 : 0;
    uint8_t itemLevel = (uint8_t)unicode.getBidiLevel(text, baseLevel, nullptr);
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
        result.last_safe_pos = text + total_len;
        return result;
    }

    // Width limit handling (grapheme break aware)
    result.fits = false;
    walk_p = text;
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

ShapedResult TextLayout::shapeText(SatoruContext* ctx, const char* text, size_t len, font_info* fi,
                                   std::set<char32_t>* usedCodepoints) {
    if (!text || !len || !fi || fi->fonts.empty() || !ctx) return {0.0, nullptr};

    ShapingKey key;
    key.text.assign(text, len);
    key.font_family = fi->desc.family;
    key.font_size = (float)fi->desc.size;
    key.font_weight = fi->desc.weight;
    key.italic = (fi->desc.style == litehtml::font_style_italic);
    key.is_rtl = fi->is_rtl;

    auto it = ctx->shapingCache.find(key);
    if (it != ctx->shapingCache.end()) {
        if (usedCodepoints) {
            // キャッシュヒット時も使用コードポイントを収集する必要がある
            UnicodeService& unicode = ctx->getUnicodeService();
            const char* p = text;
            const char* p_end = text + len;
            while (p < p_end) {
                usedCodepoints->insert(unicode.decodeUtf8(&p));
            }
        }
        return it->second;
    }

    ShapedResult result = {0.0, nullptr};
    SkShaper* shaper = ctx->getShaper();
    UnicodeService& unicode = ctx->getUnicodeService();
    if (!shaper) return {0.0, nullptr};

    std::vector<CharFont> charFonts;
    const char* walk_p = text;
    const char* walk_end = text + len;
    SkFont last_font;
    bool has_last_font = false;
    while (walk_p < walk_end) {
        const char* prev_walk = walk_p;
        char32_t u = unicode.decodeUtf8(&walk_p);
        if (usedCodepoints) usedCodepoints->insert(u);
        SkFont font =
            ctx->fontManager.selectFont(u, fi, has_last_font ? &last_font : nullptr, unicode);
        last_font = font;
        has_last_font = true;
        size_t char_len = (size_t)(walk_p - prev_walk);
        if (!charFonts.empty() && charFonts.back().font == font) {
            charFonts.back().len += char_len;
        } else {
            charFonts.push_back({char_len, font});
        }
    }

    SkTextBlobBuilderRunHandler blobHandler(text, {0, 0});
    WidthProxyRunHandler handler(&blobHandler, result);
    SatoruFontRunIterator fontRuns(charFonts);

    int baseLevel = fi->is_rtl ? 1 : 0;
    uint8_t itemLevel = (uint8_t)unicode.getBidiLevel(text, baseLevel, nullptr);
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

    ctx->shapingCache[key] = result;
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
