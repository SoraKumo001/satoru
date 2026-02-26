#include "text_renderer.h"

#include <algorithm>

#include "bridge/magic_tags.h"
#include "core/logical_geometry.h"
#include "core/satoru_context.h"
#include "core/text/text_layout.h"
#include "core/text/text_types.h"
#include "core/text/unicode_service.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkColorSpace.h"
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

    bool is_vertical = (style.mode == litehtml::writing_mode_vertical_rl ||
                        style.mode == litehtml::writing_mode_vertical_lr);

    if (!m_active) {
        m_currentStyle = style;
        if (is_vertical) {
            // For vertical writing, we always need to rebuild the blob to apply
            // coordinate swapping and centering/rotation logic.
            addBlobToBuilder(blob, tx, ty);
        } else {
            m_firstBlob = blob;
            m_firstTx = tx;
            m_firstTy = ty;
        }
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
        bool is_vertical = (m_currentStyle.mode == litehtml::writing_mode_vertical_rl ||
                            m_currentStyle.mode == litehtml::writing_mode_vertical_lr);

        if (is_vertical && !m_currentStyle.is_vertical_upright) {
            auto builder_run = m_builder.allocRunRSXform(run.font, run.count);
            memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
            float line_thickness = m_currentStyle.line_width;
            float center_x = (float)tx + line_thickness / 2.0f;
            for (int i = 0; i < run.count; ++i) {
                float gx = center_x;
                float gy = (float)ty + run.positions[i].fX;
                // Rotate 90 deg CW: cos=0, sin=1
                builder_run.xforms()[i] = SkRSXform::Make(0, 1, gx, gy);
            }
        } else {
            auto builder_run = m_builder.allocRunPos(run.font, run.count);
            memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
            for (int i = 0; i < run.count; ++i) {
                if (is_vertical) {
                    float center_x = (float)tx + m_currentStyle.line_width / 2.0f;
                    float gx = center_x - m_currentStyle.fi->desc.size / 2.0f;
                    float gy =
                        (float)ty + m_currentStyle.fi->desc.size * 0.92f + run.positions[i].fX;

                    if (m_currentStyle.is_vertical_punctuation) {
                        // Offset punctuation to the top-right
                        gx += (float)m_currentStyle.fi->desc.size * 0.6f;
                        gy -= (float)m_currentStyle.fi->desc.size * 0.6f;
                    }

                    builder_run.pos[i * 2] = gx;
                    builder_run.pos[i * 2 + 1] = gy;
                } else {
                    builder_run.pos[i * 2] = run.positions[i].fX + (float)tx;
                    builder_run.pos[i * 2 + 1] = run.positions[i].fY + (float)ty;
                }
            }
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
                            litehtml::text_overflow overflow, litehtml::direction dir,
                            litehtml::writing_mode mode, bool tagging, float currentOpacity,
                            std::vector<text_shadow_info>& usedTextShadows,
                            std::vector<text_draw_info>& usedTextDraws,
                            std::vector<SkPath>& usedGlyphs,
                            std::vector<glyph_draw_info>& usedGlyphDraws,
                            std::set<char32_t>* usedCodepoints, TextBatcher* batcher) {
    if (!canvas || !fi || fi->fonts.empty()) return;

    fi->is_rtl = (dir == litehtml::direction_rtl);

    litehtml::position actual_pos = pos;
    std::string text_str = text;
    if (overflow == litehtml::text_overflow_ellipsis) {
        double available_size =
            (mode == litehtml::writing_mode_horizontal_tb) ? pos.width : pos.height;
        bool forced = (available_size < 1.0f);
        double margin = forced ? 0.0f : 2.0f;

        if (forced || TextLayout::measureText(ctx, text, fi, mode, -1.0, nullptr).width >
                          available_size + margin) {
            text_str = TextLayout::ellipsizeText(ctx, text, fi, mode, (double)available_size,
                                                 usedCodepoints);
        }
    }

    SkPaint paint;
    paint.setAntiAlias(true);

    int styleIndex = -1;
    MagicTag styleTag = MagicTag::TextDraw;

    if (tagging && !fi->desc.text_shadow.empty()) {
        text_shadow_info info;
        info.shadows = fi->desc.text_shadow;
        info.text_color = color;
        info.opacity = currentOpacity;

        styleTag = MagicTag::TextShadow;
        for (size_t i = 0; i < usedTextShadows.size(); ++i) {
            if (usedTextShadows[i] == info) {
                styleIndex = (int)i + 1;
                break;
            }
        }

        if (styleIndex == -1) {
            usedTextShadows.push_back(info);
            styleIndex = (int)usedTextShadows.size();
        }

        paint.setColor(make_magic_color(MagicTag::TextShadow, styleIndex));
    } else if (tagging) {
        text_draw_info info;
        info.weight = fi->desc.weight;
        info.italic = (fi->desc.style == litehtml::font_style_italic);
        info.color = color;
        info.opacity = currentOpacity;

        styleTag = MagicTag::TextDraw;
        usedTextDraws.push_back(info);
        styleIndex = (int)usedTextDraws.size();

        paint.setColor(make_magic_color(MagicTag::TextDraw, styleIndex));
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

            drawTextInternal(ctx, canvas, text_str.c_str(), text_str.size(), fi, actual_pos, mode,
                             shadow_paint, false, usedTextDraws, usedGlyphs, usedGlyphDraws,
                             usedCodepoints, batcher);
        }
    }

    double final_width =
        drawTextInternal(ctx, canvas, text_str.c_str(), text_str.size(), fi, actual_pos, mode,
                         paint, tagging, usedTextDraws, usedGlyphs, usedGlyphDraws, usedCodepoints,
                         batcher, (int)styleTag, styleIndex);

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none) {
        drawDecoration(canvas, fi, pos, color, final_width, mode);
    }
}

double TextRenderer::drawTextInternal(SatoruContext* ctx, SkCanvas* canvas, const char* str,
                                      size_t strLen, font_info* fi, const litehtml::position& pos,
                                      litehtml::writing_mode mode, const SkPaint& paint,
                                      bool tagging, std::vector<text_draw_info>& usedTextDraws,
                                      std::vector<SkPath>& usedGlyphs,
                                      std::vector<glyph_draw_info>& usedGlyphDraws,
                                      std::set<char32_t>* usedCodepoints, TextBatcher* batcher,
                                      int styleTag, int styleIndex) {
    if (strLen == 0) return 0.0;

    TextAnalysis analysis = TextLayout::analyzeText(ctx, str, strLen, fi, mode, usedCodepoints);
    double total_advance = 0;
    double current_tx = (double)pos.x;
    double current_ty = (double)pos.y;

    bool is_vertical =
        (mode == litehtml::writing_mode_vertical_rl || mode == litehtml::writing_mode_vertical_lr);

    size_t start = 0;
    while (start < analysis.chars.size()) {
        size_t end = start + 1;
        while (
            end < analysis.chars.size() && analysis.chars[end].font == analysis.chars[start].font &&
            analysis.chars[end].is_vertical_upright == analysis.chars[start].is_vertical_upright &&
            analysis.chars[end].is_vertical_punctuation ==
                analysis.chars[start].is_vertical_punctuation) {
            end++;
        }

        size_t run_offset = analysis.chars[start].offset;
        size_t run_len = analysis.chars[end - 1].offset + analysis.chars[end - 1].len - run_offset;
        bool is_upright = analysis.chars[start].is_vertical_upright;
        bool is_punctuation = analysis.chars[start].is_vertical_punctuation;

        ShapedResult shaped =
            TextLayout::shapeText(ctx, str + run_offset, run_len, fi, mode, nullptr);
        if (!shaped.blob) {
            start = end;
            continue;
        }

        if (tagging) {
            if (batcher) batcher->flush();
            SkTextBlob::Iter it(*shaped.blob);
            SkTextBlob::Iter::ExperimentalRun run;
            while (it.experimentalNext(&run)) {
                for (int i = 0; i < run.count; ++i) {
                    auto pathOpt = run.font.getPath(run.glyphs[i]);
                    float gx, gy;
                    float rotation = 0;
                    if (is_vertical) {
                        float line_thickness = (float)pos.width;
                        float center_x = (float)current_tx + line_thickness / 2.0f;
                        if (is_upright) {
                            gx = center_x - fi->desc.size / 2.0f;
                            gy = (float)current_ty + fi->desc.size * 0.92f + run.positions[i].fX;
                            if (is_punctuation) {
                                gx += (float)fi->desc.size * 0.6f;
                                gy -= (float)fi->desc.size * 0.6f;
                            }
                        } else {
                            gx = center_x;
                            gy = (float)current_ty + run.positions[i].fX;
                            rotation = 90.0f;
                        }
                    } else {
                        gx = run.positions[i].fX + (float)current_tx;
                        gy = run.positions[i].fY + (float)current_ty;
                    }

                    if (pathOpt.has_value() && !pathOpt.value().isEmpty()) {
                        int glyphIdx = -1;
                        const SkPath& path = pathOpt.value();
                        for (size_t gi = 0; gi < usedGlyphs.size(); ++gi) {
                            if (usedGlyphs[gi] == path) {
                                glyphIdx = (int)gi + 1;
                                break;
                            }
                        }
                        if (glyphIdx == -1) {
                            usedGlyphs.push_back(path);
                            glyphIdx = (int)usedGlyphs.size();
                        }

                        glyph_draw_info drawInfo;
                        drawInfo.glyph_index = glyphIdx;
                        drawInfo.style_tag = styleTag;
                        drawInfo.style_index = styleIndex;

                        usedGlyphDraws.push_back(drawInfo);
                        int drawIdx = (int)usedGlyphDraws.size();

                        SkPaint glyphPaint = paint;
                        glyphPaint.setColor(make_magic_color(MagicTag::GlyphPath, drawIdx));

                        canvas->save();
                        canvas->translate(gx, gy);
                        if (rotation != 0) canvas->rotate(rotation);
                        canvas->drawPath(path, glyphPaint);
                        canvas->restore();
                    } else {
                        SkRect bounds = run.font.getBounds(run.glyphs[i], &paint);
                        int w = (int)ceilf(bounds.width());
                        int h = (int)ceilf(bounds.height());
                        if (w > 0 && h > 0) {
                            SkImageInfo info =
                                SkImageInfo::MakeN32Premul(w, h, SkColorSpace::MakeSRGB());
                            auto surface = SkSurfaces::Raster(info);
                            if (surface) {
                                auto tmpCanvas = surface->getCanvas();
                                tmpCanvas->clear(SK_ColorTRANSPARENT);
                                tmpCanvas->drawSimpleText(&run.glyphs[i], sizeof(uint16_t),
                                                          SkTextEncoding::kGlyphID, -bounds.fLeft,
                                                          -bounds.fTop, run.font, paint);
                                auto img = surface->makeImageSnapshot();

                                canvas->save();
                                canvas->translate(gx, gy);
                                if (rotation != 0) canvas->rotate(rotation);
                                SkRect dst =
                                    SkRect::MakeXYWH(bounds.fLeft, bounds.fTop, (float)w, (float)h);
                                canvas->drawImageRect(img, dst,
                                                      SkSamplingOptions(SkFilterMode::kLinear));
                                canvas->restore();
                            }
                        }
                    }
                }
            }
        } else if (batcher && fi->desc.text_shadow.empty() &&
                   fi->desc.decoration_line == litehtml::text_decoration_line_none) {
            TextBatcher::Style style;
            style.fi = fi;
            SkColor c = paint.getColor();
            style.color = {(uint8_t)SkColorGetR(c), (uint8_t)SkColorGetG(c),
                           (uint8_t)SkColorGetB(c), (uint8_t)SkColorGetA(c)};
            style.opacity = 1.0f;
            style.tagging = false;
            style.mode = mode;
            style.line_width = is_vertical ? (float)pos.width : (float)pos.height;
            style.is_vertical_upright = is_upright;
            style.is_vertical_punctuation = is_punctuation;
            batcher->addText(shaped.blob, current_tx, current_ty, style);
        } else {
            if (batcher) batcher->flush();
            canvas->save();
            if (is_vertical) {
                float line_thickness = (float)pos.width;
                float center_x = (float)current_tx + line_thickness / 2.0f;

                SkTextBlobBuilder builder;
                SkTextBlob::Iter it(*shaped.blob);
                SkTextBlob::Iter::ExperimentalRun run;
                while (it.experimentalNext(&run)) {
                    if (is_upright) {
                        auto builder_run = builder.allocRunPos(run.font, run.count);
                        memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
                        for (int i = 0; i < run.count; ++i) {
                            float gx = center_x - fi->desc.size / 2.0f;
                            float gy =
                                (float)current_ty + fi->desc.size * 0.92f + run.positions[i].fX;
                            if (is_punctuation) {
                                gx += (float)fi->desc.size * 0.6f;
                                gy -= (float)fi->desc.size * 0.6f;
                            }
                            builder_run.pos[i * 2] = gx;
                            builder_run.pos[i * 2 + 1] = gy;
                        }
                    } else {
                        auto builder_run = builder.allocRunRSXform(run.font, run.count);
                        memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
                        for (int i = 0; i < run.count; ++i) {
                            float gx = center_x;
                            float gy = (float)current_ty + run.positions[i].fX;
                            // Rotate 90 deg CW: cos=0, sin=1
                            builder_run.xforms()[i] = SkRSXform::Make(0, 1, gx, gy);
                        }
                    }
                }
                sk_sp<SkTextBlob> verticalBlob = builder.make();
                canvas->drawTextBlob(verticalBlob, 0, 0, paint);
            } else {
                canvas->drawTextBlob(shaped.blob, (float)current_tx, (float)current_ty, paint);
            }
            canvas->restore();
        }

        if (is_vertical) {
            current_ty += shaped.width;
        } else {
            current_tx += shaped.width;
        }
        total_advance += shaped.width;
        start = end;
    }

    return total_advance;
}

void TextRenderer::drawDecoration(SkCanvas* canvas, font_info* fi, const litehtml::position& pos,
                                  const litehtml::web_color& color, double finalWidth,
                                  litehtml::writing_mode mode) {
    float inline_size = (float)finalWidth;
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

    WritingModeContext wm_ctx(mode, pos.width, pos.height);

    auto draw_logical_line = [&](float block_offset) {
        logical_pos start(0, block_offset);
        logical_pos end(inline_size, block_offset);
        logical_size size(0, 0);  // lines have no size for context mapping

        litehtml::position p_start = wm_ctx.to_physical(start, size);
        litehtml::position p_end = wm_ctx.to_physical(end, size);

        if (fi->desc.decoration_style == litehtml::text_decoration_style_dotted) {
            float intervals[] = {thickness, thickness};
            dec_paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
        } else if (fi->desc.decoration_style == litehtml::text_decoration_style_dashed) {
            float intervals[] = {thickness * 3, thickness * 3};
            dec_paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
        }

        if (fi->desc.decoration_style == litehtml::text_decoration_style_double) {
            float gap = thickness + 1.0f;
            canvas->drawLine((float)pos.x + p_start.x, (float)pos.y + p_start.y - gap / 2,
                             (float)pos.x + p_end.x, (float)pos.y + p_end.y - gap / 2, dec_paint);
            canvas->drawLine((float)pos.x + p_start.x, (float)pos.y + p_start.y + gap / 2,
                             (float)pos.x + p_end.x, (float)pos.y + p_end.y + gap / 2, dec_paint);
        } else if (fi->desc.decoration_style == litehtml::text_decoration_style_wavy) {
            // TODO: Wavy line logicalization if needed, currently skipping complex logic
            canvas->drawLine((float)pos.x + p_start.x, (float)pos.y + p_start.y,
                             (float)pos.x + p_end.x, (float)pos.y + p_end.y, dec_paint);
        } else {
            canvas->drawLine((float)pos.x + p_start.x, (float)pos.y + p_start.y,
                             (float)pos.x + p_end.x, (float)pos.y + p_end.y, dec_paint);
        }
    };

    if (fi->desc.decoration_line & litehtml::text_decoration_line_underline) {
        float underline_offset =
            (float)fi->fm_ascent + (float)fi->desc.underline_offset.val() + thickness;
        draw_logical_line(underline_offset);
    }
    if (fi->desc.decoration_line & litehtml::text_decoration_line_overline) {
        draw_logical_line(0);
    }
    if (fi->desc.decoration_line & litehtml::text_decoration_line_line_through) {
        draw_logical_line((float)fi->fm_ascent * 0.65f);
    }
}

}  // namespace satoru
