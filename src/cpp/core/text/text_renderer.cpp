#include "text_renderer.h"

#include <algorithm>

#include "core/satoru_context.h"
#include "core/text/text_layout.h"
#include "core/text/unicode_service.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkFont.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTextBlob.h"
#include "include/effects/SkDashPathEffect.h"
#include "modules/skshaper/include/SkShaper.h"
#include "bridge/magic_tags.h"

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
}  // namespace

void TextRenderer::drawText(SatoruContext* ctx, SkCanvas* canvas, const char* text, font_info* fi,
                            const litehtml::web_color& color, const litehtml::position& pos,
                            litehtml::text_overflow overflow, litehtml::direction dir, bool tagging,
                            float currentOpacity, std::vector<text_shadow_info>& usedTextShadows,
                            std::vector<text_draw_info>& usedTextDraws,
                            std::set<char32_t>* usedCodepoints) {
    if (!canvas || !fi || fi->fonts.empty()) return;

    fi->is_rtl = (dir == litehtml::direction_rtl);

    std::string text_str = text;
    if (overflow == litehtml::text_overflow_ellipsis) {
        double available_width = pos.width;
        bool forced = (pos.width < 1.0f);
        double margin = forced ? 0.0f : 2.0f;

        if (forced || TextLayout::measureText(ctx, text, fi, -1.0, nullptr).width > available_width + margin) {
            text_str = TextLayout::ellipsizeText(ctx, text, fi, (double)available_width, usedCodepoints);
        }
    }

    SkPaint paint;
    paint.setAntiAlias(true);

    if (tagging && !fi->desc.text_shadow.empty()) {
        text_shadow_info info;
        info.shadows = fi->desc.text_shadow;
        info.text_color = color;
        info.opacity = currentOpacity;

        int index = -1;
        for (size_t i = 0; i < usedTextShadows.size(); ++i) {
            if (usedTextShadows[i] == info) {
                index = (int)i + 1;
                break;
            }
        }

        if (index == -1) {
            usedTextShadows.push_back(info);
            index = (int)usedTextShadows.size();
        }

        paint.setColor(SkColorSetARGB(255, 0, (uint8_t)MagicTag::TextShadow, (index & 0xFF)));
    } else if (tagging) {
        text_draw_info info;
        info.weight = fi->desc.weight;
        info.italic = (fi->desc.style == litehtml::font_style_italic);
        info.color = color;
        info.opacity = currentOpacity;

        usedTextDraws.push_back(info);
        int index = (int)usedTextDraws.size();

        uint8_t r = ((index >> 8) & 0x3F) << 2;
        paint.setColor(SkColorSetARGB(255, r, (uint8_t)MagicTag::TextDraw, (index & 0xFF)));
    } else {
        paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    }

    if (!tagging && !fi->desc.text_shadow.empty()) {
        for (auto it = fi->desc.text_shadow.rbegin(); it != fi->desc.text_shadow.rend(); ++it) {
            const auto& s = *it;
            SkPaint shadow_paint = paint;
            shadow_paint.setColor(SkColorSetARGB(s.color.alpha, s.color.red, s.color.green, s.color.blue));
            float blur_std_dev = (float)s.blur.val() * 0.5f;
            if (blur_std_dev > 0)
                shadow_paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_std_dev));

            drawTextInternal(ctx, canvas, text_str.c_str(), text_str.size(), fi, 
                             (double)pos.x + s.x.val(), (double)pos.y + s.y.val(), 
                             shadow_paint, false, usedTextDraws, usedCodepoints);
        }
    }

    double final_width = drawTextInternal(ctx, canvas, text_str.c_str(), text_str.size(), fi, 
                                          (double)pos.x, (double)pos.y, paint, tagging, 
                                          usedTextDraws, usedCodepoints);

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none) {
        drawDecoration(canvas, fi, pos, color, final_width);
    }
}

double TextRenderer::drawTextInternal(SatoruContext* ctx, SkCanvas* canvas, const char* str,
                                      size_t strLen, font_info* fi, double tx, double ty,
                                      const SkPaint& paint, bool tagging,
                                      std::vector<text_draw_info>& usedTextDraws,
                                      std::set<char32_t>* usedCodepoints) {
    if (strLen == 0) return 0.0;
    SkShaper* shaper = ctx->getShaper();
    UnicodeService& unicode = ctx->getUnicodeService();
    if (!shaper) return 0.0;

    std::vector<SatoruFontRunIterator::CharFont> charFonts;
    const char* p_walk = str;
    const char* p_end = str + strLen;
    SkFont last_font;
    bool has_last_font = false;

    while (p_walk < p_end) {
        const char* prev_walk = p_walk;
        char32_t u = unicode.decodeUtf8(&p_walk);
        if (usedCodepoints) usedCodepoints->insert(u);

        SkFont font = ctx->fontManager.selectFont(u, fi, has_last_font ? &last_font : nullptr, unicode);
        last_font = font;
        has_last_font = true;

        size_t char_len = (size_t)(p_walk - prev_walk);
        if (!charFonts.empty() && charFonts.back().font.getTypeface() == font.getTypeface() &&
            charFonts.back().font.getSize() == font.getSize() &&
            charFonts.back().font.isEmbolden() == font.isEmbolden() &&
            charFonts.back().font.isEmbeddedBitmaps() == font.isEmbeddedBitmaps()) {
            charFonts.back().len += char_len;
        } else {
            charFonts.push_back({char_len, font});
        }
    }

    SkTextBlobBuilderRunHandler handler(str, {0, 0});
    SatoruFontRunIterator fontRuns(charFonts);

    int baseLevel = fi->is_rtl ? 1 : 0;
    uint8_t itemLevel = (uint8_t)unicode.getBidiLevel(str, (int)baseLevel, nullptr);

    std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
        SkShaper::MakeBiDiRunIterator(str, strLen, itemLevel);
    if (!bidi) {
        bidi = std::make_unique<SkShaper::TrivialBiDiRunIterator>(itemLevel, strLen);
    }

    std::unique_ptr<SkShaper::ScriptRunIterator> script =
        SkShaper::MakeSkUnicodeHbScriptRunIterator(str, strLen);
    if (!script) {
        script = std::make_unique<SkShaper::TrivialScriptRunIterator>(
            SkSetFourByteTag('Z', 'y', 'y', 'y'), strLen);
    }

    std::unique_ptr<SkShaper::LanguageRunIterator> lang =
        SkShaper::MakeStdLanguageRunIterator(str, strLen);
    if (!lang) {
        lang = std::make_unique<SkShaper::TrivialLanguageRunIterator>("en", strLen);
    }

    shaper->shape(str, strLen, fontRuns, *bidi, *script, *lang, nullptr, 0, 1000000, &handler);

    double width = TextLayout::measureText(ctx, str, fi, -1.0, nullptr).width;

    sk_sp<SkTextBlob> blob = handler.makeBlob();
    if (blob) {
        if (tagging) {
            SkTextBlob::Iter it(*blob);
            SkTextBlob::Iter::ExperimentalRun run;
            while (it.experimentalNext(&run)) {
                for (int i = 0; i < run.count; ++i) {
                    auto pathOpt = run.font.getPath(run.glyphs[i]);
                    float gx = run.positions[i].fX + (float)tx;
                    float gy = run.positions[i].fY + (float)ty;

                    if (pathOpt.has_value() && !pathOpt.value().isEmpty()) {
                        canvas->save();
                        canvas->translate(gx, gy);
                        canvas->drawPath(pathOpt.value(), paint);
                        canvas->restore();
                    } else {
                        SkRect bounds = run.font.getBounds(run.glyphs[i], nullptr);
                        int w = (int)ceilf(bounds.width());
                        int h = (int)ceilf(bounds.height());
                        if (w > 0 && h > 0) {
                            SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
                            auto surface = SkSurfaces::Raster(info);
                            if (surface) {
                                auto tmpCanvas = surface->getCanvas();
                                tmpCanvas->clear(SK_ColorTRANSPARENT);
                                tmpCanvas->drawSimpleText(&run.glyphs[i], sizeof(uint16_t),
                                                       SkTextEncoding::kGlyphID, -bounds.fLeft,
                                                       -bounds.fTop, run.font, paint);
                                auto img = surface->makeImageSnapshot();

                                SkRect dst = SkRect::MakeXYWH(
                                    gx + bounds.fLeft, gy + bounds.fTop, (float)w, (float)h);
                                canvas->drawImageRect(
                                    img, dst, SkSamplingOptions(SkFilterMode::kLinear));
                            }
                        }
                    }
                }
            }
        } else {
            canvas->drawTextBlob(blob, (float)tx, (float)ty, paint);
        }
    }

    return width;
}

void TextRenderer::drawDecoration(SkCanvas* canvas, font_info* fi, const litehtml::position& pos,
                                  const litehtml::web_color& color, double finalWidth) {
    float x_offset_dec = (float)finalWidth;
    float thickness = (float)fi->desc.decoration_thickness.val();
    if (thickness == 0) thickness = 1.0f;

    litehtml::web_color dec_color = fi->desc.decoration_color;
    if (dec_color == litehtml::web_color::current_color) dec_color = color;

    SkPaint dec_paint;
    dec_paint.setColor(SkColorSetARGB(dec_color.alpha, dec_color.red, dec_color.green, dec_color.blue));
    dec_paint.setAntiAlias(true);
    dec_paint.setStrokeWidth(thickness);
    dec_paint.setStyle(SkPaint::kStroke_Style);

    if (fi->desc.decoration_style == litehtml::text_decoration_style_dotted) {
        float intervals[] = {thickness, thickness};
        dec_paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
    } else if (fi->desc.decoration_style == litehtml::text_decoration_style_dashed) {
        float intervals[] = {thickness * 3, thickness * 3};
        dec_paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
    }

    auto draw_decoration_line = [&](float y) {
        if (fi->desc.decoration_style == litehtml::text_decoration_style_double) {
            float gap = thickness + 1.0f;
            canvas->drawLine((float)pos.x, y - gap / 2, (float)pos.x + x_offset_dec, y - gap / 2, dec_paint);
            canvas->drawLine((float)pos.x, y + gap / 2, (float)pos.x + x_offset_dec, y + gap / 2, dec_paint);
        } else if (fi->desc.decoration_style == litehtml::text_decoration_style_wavy) {
            float wave_length = thickness * 8.0f;
            float wave_height = wave_length / 3.0f;

            canvas->save();
            dec_paint.setStrokeWidth(thickness * 1.5f);
            canvas->clipRect(SkRect::MakeXYWH((float)pos.x, y - wave_height - thickness * 2,
                                                x_offset_dec, wave_height * 2 + thickness * 4));

            SkPathBuilder builder;
            float x_start = (float)pos.x;
            float x_end = (float)pos.x + x_offset_dec;
            float x_aligned = floorf(x_start / wave_length) * wave_length;

            builder.moveTo(x_aligned, y);
            for (float x = x_aligned; x < x_end; x += wave_length) {
                builder.quadTo(x + wave_length / 4.0f, y - wave_height, x + wave_length / 2.0f, y);
                builder.quadTo(x + wave_length * 3.0f / 4.0f, y + wave_height, x + wave_length, y);
            }
            canvas->drawPath(builder.detach(), dec_paint);
            canvas->restore();
        } else {
            canvas->drawLine((float)pos.x, y, (float)pos.x + x_offset_dec, y, dec_paint);
        }
    };

    if (fi->desc.decoration_line & litehtml::text_decoration_line_underline) {
        float base_y = (float)pos.y;
        float underline_y = base_y + (float)fi->fm_ascent + (float)fi->desc.underline_offset.val();

        if (fi->desc.decoration_style == litehtml::text_decoration_style_wavy) {
            float wave_length = thickness * 8.0f;
            float wave_height = wave_length / 3.0f;
            underline_y += wave_height + thickness;
        } else {
            underline_y += thickness + 1.0f;
        }
        draw_decoration_line(underline_y);
    }
    if (fi->desc.decoration_line & litehtml::text_decoration_line_overline) {
        draw_decoration_line((float)pos.y);
    }
    if (fi->desc.decoration_line & litehtml::text_decoration_line_line_through) {
        draw_decoration_line((float)pos.y + (float)fi->fm_ascent * 0.65f);
    }
}

}  // namespace satoru
