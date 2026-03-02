#include "text_renderer.h"

#include <algorithm>

#include "bridge/magic_tags.h"
#include "core/logical_geometry.h"
#include "core/satoru_context.h"
#include "core/text/tagging_context.h"
#include "core/text/text_decoration_renderer.h"
#include "core/text/text_geometry.h"
#include "core/text/text_layout.h"
#include "core/text/text_types.h"
#include "core/text/unicode_service.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkFont.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
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
        litehtml::position line_pos((pixel_t)tx, (pixel_t)ty, (pixel_t)m_currentStyle.line_width,
                                    0);
        TextGeometry geom(m_currentStyle.mode, line_pos, m_currentStyle.fi);

        if (geom.isVertical() && !m_currentStyle.is_vertical_upright) {
            auto builder_run = m_builder.allocRunRSXform(run.font, run.count);
            memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
            for (int i = 0; i < run.count; ++i) {
                auto p =
                    geom.getGlyphPlacement(run.positions[i].fX, run.positions[i].fY, false, false);
                // Rotate 90 deg CW: cos=0, sin=1
                builder_run.xforms()[i] = SkRSXform::Make(0, 1, p.x, p.y);
            }
        } else {
            auto builder_run = m_builder.allocRunPos(run.font, run.count);
            memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
            for (int i = 0; i < run.count; ++i) {
                auto p = geom.getGlyphPlacement(run.positions[i].fX, run.positions[i].fY,
                                                m_currentStyle.is_vertical_upright,
                                                m_currentStyle.is_vertical_punctuation);
                builder_run.pos[i * 2] = p.x;
                builder_run.pos[i * 2 + 1] = p.y;
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
        TextDecorationRenderer::drawDecoration(canvas, fi, pos, color, final_width, mode);
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
            TaggingContext tagging_ctx(canvas, usedGlyphs, usedGlyphDraws, styleTag, styleIndex);
            TextGeometry geom(mode, pos, fi);

            while (it.experimentalNext(&run)) {
                float logical_run_start =
                    is_vertical ? (float)(current_ty - pos.y) : (float)(current_tx - pos.x);

                for (int i = 0; i < run.count; ++i) {
                    auto p =
                        geom.getGlyphPlacement(logical_run_start + run.positions[i].fX,
                                               run.positions[i].fY, is_upright, is_punctuation);
                    tagging_ctx.drawGlyph(run.font, run.glyphs[i], p.x, p.y, p.rotation, paint);
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
                TextGeometry geom(mode, pos, fi);
                SkTextBlobBuilder builder;
                SkTextBlob::Iter it(*shaped.blob);
                SkTextBlob::Iter::ExperimentalRun run;
                while (it.experimentalNext(&run)) {
                    float logical_run_start = (float)(current_ty - pos.y);
                    if (is_upright) {
                        auto builder_run = builder.allocRunPos(run.font, run.count);
                        memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
                        for (int i = 0; i < run.count; ++i) {
                            auto p =
                                geom.getGlyphPlacement(logical_run_start + run.positions[i].fX,
                                                       run.positions[i].fY, true, is_punctuation);
                            builder_run.pos[i * 2] = p.x;
                            builder_run.pos[i * 2 + 1] = p.y;
                        }
                    } else {
                        auto builder_run = builder.allocRunRSXform(run.font, run.count);
                        memcpy(builder_run.glyphs, run.glyphs, run.count * sizeof(uint16_t));
                        for (int i = 0; i < run.count; ++i) {
                            auto p = geom.getGlyphPlacement(logical_run_start + run.positions[i].fX,
                                                            run.positions[i].fY, false, false);
                            // Rotate 90 deg CW: cos=0, sin=1
                            builder_run.xforms()[i] = SkRSXform::Make(0, 1, p.x, p.y);
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

}  // namespace satoru
