#include "text_renderer.h"

#include <algorithm>

#include "bridge/magic_tags.h"
#include "core/satoru_context.h"
#include "core/text/text_layout.h"
#include "core/text/text_types.h"
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
#include "utils/logging.h"

namespace satoru {

void TextBatcher::addText(const sk_sp<SkTextBlob>& blob, double tx, double ty, const Style& style) {
    if (m_active && m_currentStyle != style) {
        flush();
    }
    if (!m_active) {
        m_currentStyle = style;
        m_firstBlob = blob;
        m_firstTx = tx;
        m_firstTy = ty;
        m_active = true;
        return;
    }

    if (m_firstBlob) {
        addBlobToBuilder(m_firstBlob, m_firstTx, m_firstTy);
        m_firstBlob = nullptr;
    }
    addBlobToBuilder(blob, tx, ty);
}

void TextBatcher::addBlobToBuilder(const sk_sp<SkTextBlob>& blob, double tx, double ty) {
    SkTextBlob::Iter it(*blob);
    SkTextBlob::Iter::ExperimentalRun run;
    while (it.experimentalNext(&run)) {
        auto builder_run = m_builder.allocRunPos(run.font, run.count);
        memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
        for (int i = 0; i < run.count; ++i) {
            builder_run.pos[i * 2] = run.positions[i].fX + (float)tx;
            builder_run.pos[i * 2 + 1] = run.positions[i].fY + (float)ty;
        }
    }
}

void TextBatcher::flush() {
    if (!m_active) return;

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(m_currentStyle.color.alpha, m_currentStyle.color.red,
                                  m_currentStyle.color.green, m_currentStyle.color.blue));
    if (m_currentStyle.opacity < 1.0f) {
        paint.setAlphaf(paint.getAlphaf() * m_currentStyle.opacity);
    }

    if (m_firstBlob) {
        m_canvas->drawTextBlob(m_firstBlob, (float)m_firstTx, (float)m_firstTy, paint);
        m_firstBlob = nullptr;
    } else {
        sk_sp<SkTextBlob> blob = m_builder.make();
        if (blob) {
            m_canvas->drawTextBlob(blob, 0, 0, paint);
        }
    }
    m_active = false;
}

void TextRenderer::drawText(SatoruContext* ctx, SkCanvas* canvas, const char* text, font_info* fi,
                            const litehtml::web_color& color, const litehtml::position& pos,
                            litehtml::text_overflow overflow, litehtml::direction dir, bool tagging,
                            float currentOpacity, std::vector<text_shadow_info>& usedTextShadows,
                            std::vector<text_draw_info>& usedTextDraws,
                            std::set<char32_t>* usedCodepoints, TextBatcher* batcher) {
    if (!canvas || !fi || fi->fonts.empty()) return;

    fi->is_rtl = (dir == litehtml::direction_rtl);

    std::string text_str = text;
    if (overflow == litehtml::text_overflow_ellipsis) {
        double available_width = pos.width;
        bool forced = (pos.width < 1.0f);
        double margin = forced ? 0.0f : 2.0f;

        if (forced || TextLayout::measureText(ctx, text, fi, -1.0, nullptr).width >
                          available_width + margin) {
            text_str =
                TextLayout::ellipsizeText(ctx, text, fi, (double)available_width, usedCodepoints);
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

        paint.setColor(make_magic_color(MagicTag::TextShadow, index));
    } else if (tagging) {
        text_draw_info info;
        info.weight = fi->desc.weight;
        info.italic = (fi->desc.style == litehtml::font_style_italic);
        info.color = color;
        info.opacity = currentOpacity;

        usedTextDraws.push_back(info);
        int index = (int)usedTextDraws.size();

        paint.setColor(make_magic_color(MagicTag::TextDraw, index));
    } else {
        paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    }

    if (!tagging && !fi->desc.text_shadow.empty()) {
        for (auto it = fi->desc.text_shadow.rbegin(); it != fi->desc.text_shadow.rend(); ++it) {
            const auto& s = *it;
            SkPaint shadow_paint = paint;
            shadow_paint.setColor(
                SkColorSetARGB(s.color.alpha, s.color.red, s.color.green, s.color.blue));
            float blur_std_dev = (float)s.blur.val() * 0.5f;
            if (blur_std_dev > 0)
                shadow_paint.setMaskFilter(
                    SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_std_dev));

            drawTextInternal(ctx, canvas, text_str.c_str(), text_str.size(), fi,
                             (double)pos.x + s.x.val(), (double)pos.y + s.y.val(), shadow_paint,
                             false, usedTextDraws, usedCodepoints, batcher);
        }
    }

    double final_width =
        drawTextInternal(ctx, canvas, text_str.c_str(), text_str.size(), fi, (double)pos.x,
                         (double)pos.y, paint, tagging, usedTextDraws, usedCodepoints, batcher);

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none) {
        drawDecoration(canvas, fi, pos, color, final_width);
    }
}

double TextRenderer::drawTextInternal(SatoruContext* ctx, SkCanvas* canvas, const char* str,
                                      size_t strLen, font_info* fi, double tx, double ty,
                                      const SkPaint& paint, bool tagging,
                                      std::vector<text_draw_info>& usedTextDraws,
                                      std::set<char32_t>* usedCodepoints, TextBatcher* batcher) {
    if (strLen == 0) return 0.0;

    ShapedResult shaped = TextLayout::shapeText(ctx, str, strLen, fi, usedCodepoints);
    if (!shaped.blob) return 0.0;

    if (tagging) {
        if (batcher) batcher->flush();
        SkTextBlob::Iter it(*shaped.blob);
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

                            SkRect dst = SkRect::MakeXYWH(gx + bounds.fLeft, gy + bounds.fTop,
                                                          (float)w, (float)h);
                            canvas->drawImageRect(img, dst,
                                                  SkSamplingOptions(SkFilterMode::kLinear));
                        }
                    }
                }
            }
        }
    } else {
        if (batcher && fi->desc.text_shadow.empty() &&
            fi->desc.decoration_line == litehtml::text_decoration_line_none) {
            TextBatcher::Style style;
            style.fi = fi;
            SkColor c = paint.getColor();
            style.color = {(uint8_t)SkColorGetR(c), (uint8_t)SkColorGetG(c),
                           (uint8_t)SkColorGetB(c), (uint8_t)SkColorGetA(c)};
            style.opacity = 1.0f;  // Opacity is already in paint color or handled by layers
            style.tagging = false;
            batcher->addText(shaped.blob, tx, ty, style);
        } else {
            if (batcher) batcher->flush();
            canvas->drawTextBlob(shaped.blob, (float)tx, (float)ty, paint);
        }
    }

    return shaped.width;
}

void TextRenderer::drawDecoration(SkCanvas* canvas, font_info* fi, const litehtml::position& pos,
                                  const litehtml::web_color& color, double finalWidth) {
    float x_offset_dec = (float)finalWidth;
    float thickness = (float)fi->desc.decoration_thickness.val();
    if (thickness == 0) thickness = 1.0f;

    litehtml::web_color dec_color = fi->desc.decoration_color;
    if (dec_color == litehtml::web_color::current_color) dec_color = color;

    SkPaint dec_paint;
    dec_paint.setColor(
        SkColorSetARGB(dec_color.alpha, dec_color.red, dec_color.green, dec_color.blue));
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
            canvas->drawLine((float)pos.x, y - gap / 2, (float)pos.x + x_offset_dec, y - gap / 2,
                             dec_paint);
            canvas->drawLine((float)pos.x, y + gap / 2, (float)pos.x + x_offset_dec, y + gap / 2,
                             dec_paint);
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
