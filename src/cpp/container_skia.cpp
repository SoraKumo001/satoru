#include "container_skia.h"
#include "utils.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkSpan.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkBlurTypes.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkData.h"
#include "src/base/SkUTF.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

container_skia::container_skia(int w, int h, SkCanvas* canvas, SatoruContext& context, bool tagging)
    : m_width(w), m_height(h), m_canvas(canvas), m_context(context), m_tagging(tagging) {}

litehtml::uint_ptr container_skia::create_font(const litehtml::font_description& desc, const litehtml::document* doc, litehtml::font_metrics* fm) {
    std::string cleanedName = clean_font_name(desc.family.c_str());
    sk_sp<SkTypeface> typeface;

    auto it = m_context.typefaceCache.find(cleanedName);
    if (it != m_context.typefaceCache.end()) {
        const auto& variations = it->second;
        if (!variations.empty()) {
            int bestScore = -1;
            for (const auto& tf : variations) {
                SkFontStyle tfStyle = tf->fontStyle();
                int score = 0;
                if ((int)tfStyle.slant() == (int)desc.style) score += 1000;
                int weightDiff = std::abs(tfStyle.weight() - desc.weight);
                score += (1000 - weightDiff);
                if (score > bestScore) {
                    bestScore = score;
                    typeface = tf;
                }
            }
        }
    }

    if (!typeface) {
        if (m_context.defaultTypeface) typeface = m_context.defaultTypeface;
        else typeface = m_context.fontMgr->legacyMakeTypeface(nullptr, SkFontStyle(desc.weight, SkFontStyle::kNormal_Width, (SkFontStyle::Slant)desc.style));
    }

    if (!typeface) return 0;

    SkFont* font = new SkFont(typeface, (float)desc.size);
    font->setSubpixel(true);
    font->setEdging(SkFont::Edging::kAntiAlias);
    font->setHinting(SkFontHinting::kNone);
    font->setLinearMetrics(true);

    if (desc.style == litehtml::font_style_italic && typeface->fontStyle().slant() == SkFontStyle::kUpright_Slant) {
        font->setSkewX(-0.25f);
    }

    if (fm) {
        SkFontMetrics skFm;
        font->getMetrics(&skFm);
        fm->ascent = -skFm.fAscent;
        fm->descent = skFm.fDescent;
        fm->height = -skFm.fAscent + skFm.fDescent + skFm.fLeading;
        fm->x_height = skFm.fXHeight;
        fm->font_size = (float)desc.size;
        if (fm->height <= 0) fm->height = (float)desc.size;
    }

    font_info* fi = new font_info();
    fi->font = font;
    fi->desc = desc;
    return (litehtml::uint_ptr)fi;
}

void container_skia::delete_font(litehtml::uint_ptr hFont) {
    font_info* fi = (font_info*)hFont;
    if (fi) {
        delete fi->font;
        delete fi;
    }
}

litehtml::pixel_t container_skia::text_width(const char* text, litehtml::uint_ptr hFont) {
    if (!text || !hFont) return 0;
    font_info* fi = (font_info*)hFont;
    SkFont* baseFont = fi->font;

    float total_width = 0;
    const char* ptr = text;
    const char* end = text + strlen(text);

    while (ptr < end) {
        const char* run_start = ptr;
        const char* temp_ptr = ptr;
        SkUnichar first_uni = SkUTF::NextUTF8(&temp_ptr, end);

        if (baseFont->unicharToGlyph(first_uni) != 0) {
            while (ptr < end) {
                const char* next_ptr = ptr;
                SkUnichar uni = SkUTF::NextUTF8(&next_ptr, end);
                if (uni <= 0 || baseFont->unicharToGlyph(uni) == 0) break;
                ptr = next_ptr;
            }
            total_width += baseFont->measureText(run_start, ptr - run_start, SkTextEncoding::kUTF8);
        } else {
            SkUnichar uni = SkUTF::NextUTF8(&ptr, end);
            SkFont targetFont = *baseFont;
            for (auto& fbTypeface : m_context.fallbackTypefaces) {
                if (fbTypeface->unicharToGlyph(uni) != 0) {
                    targetFont.setTypeface(fbTypeface);
                    break;
                }
            }
            total_width += targetFont.measureText(&uni, sizeof(SkUnichar), SkTextEncoding::kUTF32);
        }
    }

    return (litehtml::pixel_t)(total_width + 0.5f);
}

static bool is_whitespace_uni(SkUnichar uni) {
    return uni <= 32 || uni == 0xA0 || uni == 0x3000 || (uni >= 0x2000 && uni <= 0x200A) || uni == 0x202F || uni == 0x205F;
}

void container_skia::draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) {
    if (!m_canvas || !text || !text[0] || !hFont) return;
    font_info* fi = (font_info*)hFont;
    SkFont* baseFont = fi->font;

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));

    float actualWeight = (float)baseFont->getTypeface()->fontStyle().weight();
    float requestedWeight = (float)fi->desc.weight;
    if (requestedWeight > actualWeight) {
        float strokeWidth = (requestedWeight - actualWeight) * baseFont->getSize() / 7500.0f;
        paint.setStyle(SkPaint::kStrokeAndFill_Style);
        paint.setStrokeWidth(strokeWidth);
        paint.setStrokeJoin(SkPaint::kRound_Join);
    }

    SkFontMetrics skFm;
    baseFont->getMetrics(&skFm);
    float baseline_y = (float)pos.y - skFm.fAscent;
    float current_x = (float)pos.x;

    auto draw_text_runs = [&](float offset_x, float offset_y, const SkPaint& p) {
        const char* ptr = text;
        const char* end = text + strlen(text);
        float cx = offset_x;
        bool skip_draw = (SkColorGetA(p.getColor()) == 0);

        while (ptr < end) {
            const char* run_start = ptr;
            const char* temp_ptr = ptr;
            SkUnichar first_uni = SkUTF::NextUTF8(&temp_ptr, end);

            if (baseFont->unicharToGlyph(first_uni) != 0) {
                while (ptr < end) {
                    const char* next_ptr = ptr;
                    SkUnichar uni = SkUTF::NextUTF8(&next_ptr, end);
                    if (uni <= 0 || baseFont->unicharToGlyph(uni) == 0) break;
                    ptr = next_ptr;
                }
                if (!skip_draw) {
                    m_canvas->drawSimpleText(run_start, ptr - run_start, SkTextEncoding::kUTF8, cx, offset_y, *baseFont, p);
                }
                cx += baseFont->measureText(run_start, ptr - run_start, SkTextEncoding::kUTF8);
            } else {
                SkUnichar uni = SkUTF::NextUTF8(&ptr, end);
                SkFont targetFont = *baseFont;
                for (auto& fbTypeface : m_context.fallbackTypefaces) {
                    if (fbTypeface->unicharToGlyph(uni) != 0) {
                        targetFont.setTypeface(fbTypeface);
                        break;
                    }
                }
                if (!skip_draw) {
                    m_canvas->drawSimpleText(&uni, sizeof(SkUnichar), SkTextEncoding::kUTF32, cx, offset_y, targetFont, p);
                }
                cx += targetFont.measureText(&uni, sizeof(SkUnichar), SkTextEncoding::kUTF32);
            }
        }
        return cx - offset_x;
    };

    for (int i = (int)fi->desc.text_shadow.size() - 1; i >= 0; --i) {
        const auto& shadow = fi->desc.text_shadow[i];
        if (shadow.color.alpha == 0) continue;
        
        SkPaint shadowPaint = paint;
        if (m_tagging) {
            shadow_info si;
            si.color = shadow.color;
            si.blur = (float)shadow.blur.val();
            si.x = (float)shadow.x.val();
            si.y = (float)shadow.y.val();
            si.spread = 0;
            si.inset = false;

            int index = 0;
            auto it = m_shadowToIndex.find(si);
            if (it == m_shadowToIndex.end()) {
                m_usedShadows.push_back(si);
                index = (int)m_usedShadows.size();
                m_shadowToIndex[si] = index;
            } else {
                index = it->second;
            }
            shadowPaint.setColor(SkColorSetARGB(255, 0xFC, (index >> 8) & 0xFF, index & 0xFF));
        } else {
            shadowPaint.setColor(SkColorSetARGB(shadow.color.alpha, shadow.color.red, shadow.color.green, shadow.color.blue));
            if (shadow.blur.val() > 0) {
                shadowPaint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, (float)shadow.blur.val() * 0.5f));
            }
        }
        draw_text_runs(current_x + (float)shadow.x.val(), baseline_y + (float)shadow.y.val(), shadowPaint);
    }

    float final_width = draw_text_runs(current_x, baseline_y, paint);

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none && final_width > 0.1f) {
        SkPaint decPaint = paint;
        if (fi->desc.decoration_color != litehtml::web_color::current_color) {
            decPaint.setColor(SkColorSetARGB(fi->desc.decoration_color.alpha, fi->desc.decoration_color.red, fi->desc.decoration_color.green, fi->desc.decoration_color.blue));
        }
        float thickness = (float)fi->desc.decoration_thickness.val();
        if (thickness <= 0) thickness = std::max(1.0f, (float)fi->desc.size / 15.0f);
        decPaint.setStrokeWidth(thickness);
        decPaint.setStyle(SkPaint::kStroke_Style);

        if (fi->desc.decoration_style == litehtml::text_decoration_style_dashed) {
            const SkScalar intervals[] = {thickness * 3, thickness * 2};
            decPaint.setPathEffect(SkDashPathEffect::Make(SkSpan(intervals), 0));
        } else if (fi->desc.decoration_style == litehtml::text_decoration_style_dotted) {
            const SkScalar intervals[] = {thickness, thickness};
            decPaint.setPathEffect(SkDashPathEffect::Make(SkSpan(intervals), 0));
            decPaint.setStrokeCap(SkPaint::kRound_Cap);
        }

        if (fi->desc.decoration_line & litehtml::text_decoration_line_underline) {
            float uy = baseline_y + (skFm.fUnderlinePosition > 0 ? skFm.fUnderlinePosition : thickness);
            m_canvas->drawLine(current_x, uy, current_x + final_width, uy, decPaint);
        }
        if (fi->desc.decoration_line & litehtml::text_decoration_line_line_through) {
            float ty = baseline_y + skFm.fStrikeoutPosition;
            if (skFm.fStrikeoutPosition == 0) ty = baseline_y - (skFm.fCapHeight / 2.0f);
            m_canvas->drawLine(current_x, ty, current_x + final_width, ty, decPaint);
        }
        if (fi->desc.decoration_line & litehtml::text_decoration_line_overline) {
            float oy = (float)pos.y;
            m_canvas->drawLine(current_x, oy, current_x + final_width, oy, decPaint);
        }
    }
}

void container_skia::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) {
    if (!m_canvas || draw_pos.width <= 0.1f || draw_pos.height <= 0.1f) return;

    SkRect rect = SkRect::MakeXYWH((float)draw_pos.x, (float)draw_pos.y, (float)draw_pos.width, (float)draw_pos.height);
    SkVector radii[4] = {
        {(float)borders.radius.top_left_x, (float)borders.radius.top_left_y},
        {(float)borders.radius.top_right_x, (float)borders.radius.top_right_y},
        {(float)borders.radius.bottom_right_x, (float)borders.radius.bottom_right_y},
        {(float)borders.radius.bottom_left_x, (float)borders.radius.bottom_left_y}
    };
    SkRRect rrect;
    rrect.setRectRadii(rect, radii);

    auto apply_style = [&](SkPaint& p, litehtml::border_style style, float width) {
        if (style == litehtml::border_style_dashed) {
            const SkScalar intervals[] = {width * 3, width * 2};
            p.setPathEffect(SkDashPathEffect::Make(SkSpan(intervals), 0));
        } else if (style == litehtml::border_style_dotted) {
            const SkScalar intervals[] = {width, width};
            p.setPathEffect(SkDashPathEffect::Make(SkSpan(intervals), 0));
            p.setStrokeCap(SkPaint::kRound_Cap);
        }
    };

    m_canvas->save();
    m_canvas->clipRRect(rrect, true);

    if (borders.top.width == borders.bottom.width && borders.top.width == borders.left.width && borders.top.width == borders.right.width && 
        borders.top.color == borders.bottom.color && borders.top.color == borders.left.color && borders.top.color == borders.right.color && 
        borders.top.style == borders.bottom.style && borders.top.style == borders.left.style && borders.top.style == borders.right.style)   
    {
        if (borders.top.width > 0 && borders.top.style != litehtml::border_style_none && borders.top.color.alpha > 0) {
            SkPaint p;
            p.setAntiAlias(true);
            p.setStyle(SkPaint::kStroke_Style);
            p.setStrokeWidth((float)borders.top.width * 2.0f);
            p.setColor(SkColorSetARGB(borders.top.color.alpha, borders.top.color.red, borders.top.color.green, borders.top.color.blue));    
            apply_style(p, borders.top.style, (float)borders.top.width);
            m_canvas->drawRRect(rrect, p);
        }
    }
    else {
        auto draw_b = [&](float x1, float y1, float x2, float y2, const litehtml::border& b) {
            if (b.width > 0 && b.style != litehtml::border_style_none && b.color.alpha > 0) {
                SkPaint p;
                p.setAntiAlias(true);
                p.setStyle(SkPaint::kStroke_Style);
                p.setStrokeWidth((float)b.width * 2.0f);
                p.setColor(SkColorSetARGB(b.color.alpha, b.color.red, b.color.green, b.color.blue));
                apply_style(p, b.style, (float)b.width);
                m_canvas->drawLine(x1, y1, x2, y2, p);
            }
        };

        if (borders.top.width > 0) draw_b((float)draw_pos.x, (float)draw_pos.y, (float)draw_pos.right(), (float)draw_pos.y, borders.top);   
        if (borders.bottom.width > 0) draw_b((float)draw_pos.x, (float)draw_pos.bottom(), (float)draw_pos.right(), (float)draw_pos.bottom(), borders.bottom);
        if (borders.left.width > 0) draw_b((float)draw_pos.x, (float)draw_pos.y, (float)draw_pos.x, (float)draw_pos.bottom(), borders.left);
        if (borders.right.width > 0) draw_b((float)draw_pos.right(), (float)draw_pos.y, (float)draw_pos.right(), (float)draw_pos.bottom(), borders.right);
    }
    m_canvas->restore();
}

void container_skia::draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector& shadows, const litehtml::position& pos, const litehtml::border_radiuses& radius, bool inset_pass) {
    if (!m_canvas || shadows.empty() || pos.width <= 0.1f || pos.height <= 0.1f) return;

    for (const auto& shadow : shadows) {
        if (shadow.inset != inset_pass || shadow.color.alpha == 0) continue;

        SkPaint paint;
        paint.setAntiAlias(true);

        if (m_tagging) {
            shadow_info si;
            si.color = shadow.color;
            si.blur = (float)shadow.blur.val();
            si.x = (float)shadow.x.val();
            si.y = (float)shadow.y.val();
            si.spread = (float)shadow.spread.val();
            si.inset = shadow.inset;
            si.box_pos = pos;
            si.box_radius = radius;

            int index = 0;
            auto it = m_shadowToIndex.find(si);
            if (it == m_shadowToIndex.end()) {
                m_usedShadows.push_back(si);
                index = (int)m_usedShadows.size();
                m_shadowToIndex[si] = index;
            } else {
                index = it->second;
            }
            paint.setColor(SkColorSetARGB(255, 0xFC, (index >> 8) & 0xFF, index & 0xFF));
        } else {
            paint.setColor(SkColorSetARGB(shadow.color.alpha, shadow.color.red, shadow.color.green, shadow.color.blue));
            if (shadow.blur.val() > 0) {
                paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, (float)shadow.blur.val() * 0.5f));
            }
        }

        SkVector radii[4] = {
            {(float)radius.top_left_x, (float)radius.top_left_y},
            {(float)radius.top_right_x, (float)radius.top_right_y},
            {(float)radius.bottom_right_x, (float)radius.bottom_right_y},
            {(float)radius.bottom_left_x, (float)radius.bottom_left_y}
        };

        SkRect baseRect = SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height);
        SkRRect baseRRect;
        baseRRect.setRectRadii(baseRect, radii);

        float spread = (float)shadow.spread.val();

        if (shadow.inset) {
            m_canvas->save();
            m_canvas->clipRRect(baseRRect, true);
            
            SkRect hugeRect = baseRect;
            float margin = (float)shadow.blur.val() * 3.0f + std::abs(spread) + std::abs((float)shadow.x.val()) + std::abs((float)shadow.y.val());
            hugeRect.outset(margin + 100.0f, margin + 100.0f);

            SkRRect holeRRect = baseRRect;
            holeRRect.inset(spread, spread);
            holeRRect.offset((float)shadow.x.val(), (float)shadow.y.val());

            SkPath path = SkPathBuilder().setFillType(SkPathFillType::kEvenOdd)
                                         .addRect(hugeRect)
                                         .addRRect(holeRRect)
                                         .detach();

            m_canvas->drawPath(path, paint);
            m_canvas->restore();
        } else {
            SkRect shadowRect = baseRect;
            shadowRect.outset(spread, spread);
            shadowRect.offset((float)shadow.x.val(), (float)shadow.y.val());
            
            SkRRect shadowRRect;
            SkVector shadowRadii[4];
            for(int i=0; i<4; ++i) {
                shadowRadii[i].set(std::max(0.0f, radii[i].fX + spread), std::max(0.0f, radii[i].fY + spread));
            }
            shadowRRect.setRectRadii(shadowRect, shadowRadii);
            m_canvas->drawRRect(shadowRRect, paint);
        }
    }
}

void container_skia::draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::linear_gradient& gradient) {
    if (!m_canvas || layer.border_box.width <= 0.1f || layer.border_box.height <= 0.1f) return;

    std::vector<SkColor4f> colors;
    std::vector<float> positions;
    for (const auto& pt : gradient.color_points) {
        colors.push_back({pt.color.red/255.0f, pt.color.green/255.0f, pt.color.blue/255.0f, pt.color.alpha/255.0f});
        positions.push_back(pt.offset);
    }

    SkPoint pts[2] = {
        {(float)gradient.start.x, (float)gradient.start.y},
        {(float)gradient.end.x, (float)gradient.end.y}
    };

    SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(positions), SkTileMode::kClamp), SkGradient::Interpolation());
    auto shader = SkShaders::LinearGradient(pts, skGrad);

    SkPaint paint;
    paint.setShader(shader);
    paint.setAntiAlias(true);

    SkRect rect = SkRect::MakeXYWH((float)layer.border_box.x, (float)layer.border_box.y, (float)layer.border_box.width, (float)layer.border_box.height);
    SkVector radii[4] = {
        {(float)layer.border_radius.top_left_x, (float)layer.border_radius.top_left_y},
        {(float)layer.border_radius.top_right_x, (float)layer.border_radius.top_right_y},
        {(float)layer.border_radius.bottom_right_x, (float)layer.border_radius.bottom_right_y},
        {(float)layer.border_radius.bottom_left_x, (float)layer.border_radius.bottom_left_y}
    };
    SkRRect rrect;
    rrect.setRectRadii(rect, radii);
    m_canvas->drawRRect(rrect, paint);
}

void container_skia::draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::radial_gradient& gradient) {
    if (!m_canvas || layer.border_box.width <= 0.1f || layer.border_box.height <= 0.1f) return;

    std::vector<SkColor4f> colors;
    std::vector<float> positions;
    for (const auto& pt : gradient.color_points) {
        colors.push_back({pt.color.red/255.0f, pt.color.green/255.0f, pt.color.blue/255.0f, pt.color.alpha/255.0f});
        positions.push_back(pt.offset);
    }

    SkPoint center = {(float)gradient.position.x, (float)gradient.position.y};
    float rx = (float)gradient.radius.x;
    float ry = (float)gradient.radius.y;

    SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(positions), SkTileMode::kClamp), SkGradient::Interpolation());

    SkMatrix matrix;
    matrix.setScale(rx, ry);
    matrix.postTranslate(center.x(), center.y());

    auto shader = SkShaders::RadialGradient({0, 0}, 1.0f, skGrad, &matrix);

    SkPaint paint;
    paint.setShader(shader);
    paint.setAntiAlias(true);

    SkRect rect = SkRect::MakeXYWH((float)layer.border_box.x, (float)layer.border_box.y, (float)layer.border_box.width, (float)layer.border_box.height);
    SkVector radii[4] = {
        {(float)layer.border_radius.top_left_x, (float)layer.border_radius.top_left_y},
        {(float)layer.border_radius.top_right_x, (float)layer.border_radius.top_right_y},
        {(float)layer.border_radius.bottom_right_x, (float)layer.border_radius.bottom_right_y},
        {(float)layer.border_radius.bottom_left_x, (float)layer.border_radius.bottom_left_y}
    };
    SkRRect rrect;
    rrect.setRectRadii(rect, radii);
    m_canvas->drawRRect(rrect, paint);
}

void container_skia::draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::web_color& color) {   
    if (!m_canvas || color.alpha == 0 || layer.border_box.width <= 0.1f || layer.border_box.height <= 0.1f) return;

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));

    SkRect rect = SkRect::MakeXYWH((float)layer.border_box.x, (float)layer.border_box.y, (float)layer.border_box.width, (float)layer.border_box.height);
    SkVector radii[4] = {
        {(float)layer.border_radius.top_left_x, (float)layer.border_radius.top_left_y},
        {(float)layer.border_radius.top_right_x, (float)layer.border_radius.top_right_y},
        {(float)layer.border_radius.bottom_right_x, (float)layer.border_radius.bottom_right_y},
        {(float)layer.border_radius.bottom_left_x, (float)layer.border_radius.bottom_left_y}
    };
    SkRRect rrect;
    rrect.setRectRadii(rect, radii);
    m_canvas->drawRRect(rrect, paint);
}

void container_skia::draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const std::string& url, const std::string& base_url) {
    if (!m_canvas || layer.border_box.width <= 0.1f || layer.border_box.height <= 0.1f) return;

    auto it = m_context.imageCache.find(url);
    if (it != m_context.imageCache.end()) {
        const auto& info = it->second;

        SkRect rect = SkRect::MakeXYWH((float)layer.border_box.x, (float)layer.border_box.y, (float)layer.border_box.width, (float)layer.border_box.height);
        SkVector radii[4] = {
            {(float)layer.border_radius.top_left_x, (float)layer.border_radius.top_left_y},
            {(float)layer.border_radius.top_right_x, (float)layer.border_radius.top_right_y},
            {(float)layer.border_radius.bottom_right_x, (float)layer.border_radius.bottom_right_y},
            {(float)layer.border_radius.bottom_left_x, (float)layer.border_radius.bottom_left_y}
        };
        SkRRect rrect;
        rrect.setRectRadii(rect, radii);

        if (m_tagging) {
            image_draw_info idi;
            idi.url = url;
            idi.layer = layer;

            m_usedImageDraws.push_back(idi);
            int index = (int)m_usedImageDraws.size();

            SkPaint paint;
            paint.setAntiAlias(false);
            paint.setColor(SkColorSetARGB(255, 0xFB, (index >> 8) & 0xFF, index & 0xFF));

            m_canvas->save();
            m_canvas->clipRRect(rrect, true);
            m_canvas->drawRect(rect, paint);
            m_canvas->restore();
        } else {
            if (info.skImage) {
                m_canvas->save();
                m_canvas->clipRRect(rrect, true);
                m_canvas->drawImageRect(info.skImage, rect, SkSamplingOptions());
                m_canvas->restore();
            }
        }
    }
}

void container_skia::load_image(const char* src, const char* baseurl, bool redraw_on_ready) {}

void container_skia::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) {
    if (!m_canvas || pos.width <= 0.1f || pos.height <= 0.1f) return;
    m_canvas->save();

    SkRect rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height);
    if (bdr_radius.top_left_x > 0 || bdr_radius.top_right_x > 0) {
        SkVector radii[4] = {
            {(float)bdr_radius.top_left_x, (float)bdr_radius.top_left_y},
            {(float)bdr_radius.top_right_x, (float)bdr_radius.top_right_y},
            {(float)bdr_radius.bottom_right_x, (float)bdr_radius.bottom_right_y},
            {(float)bdr_radius.bottom_left_x, (float)bdr_radius.bottom_left_y}
        };
        SkRRect rrect;
        rrect.setRectRadii(rect, radii);
        m_canvas->clipRRect(rrect, true);
    } else {
        m_canvas->clipRect(rect, true);
    }
}

void container_skia::del_clip() {
    if (m_canvas) m_canvas->restore();
}

litehtml::pixel_t container_skia::pt_to_px(float pt) const { return (litehtml::pixel_t)(pt * 1.33333333f); }
litehtml::pixel_t container_skia::get_default_font_size() const { return 16.0f; }
const char* container_skia::get_default_font_name() const { return "sans-serif"; }

void container_skia::get_image_size(const char* src, const char* baseurl, litehtml::size& sz) {
    std::string key = src;
    auto it = m_context.imageCache.find(key);
    if (it != m_context.imageCache.end()) {
        sz.width = it->second.width;
        sz.height = it->second.height;
    }
}

void container_skia::get_viewport(litehtml::position& viewport) const {
    viewport.x = 0; viewport.y = 0; viewport.width = m_width; viewport.height = m_height;
}

void container_skia::get_media_features(litehtml::media_features& features) const {
    features.type = litehtml::media_type_screen;
    features.width = m_width;
    features.height = m_height;
    features.device_width = m_width;
    features.device_height = m_height;
    features.color = 8;
    features.monochrome = 0;
    features.color_index = 256;
    features.resolution = 96;
}

void container_skia::get_language(litehtml::string& language, litehtml::string& culture) const {
    language = "ja";
    culture = "ja_JP";
}

void container_skia::draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::conic_gradient& gradient) {
    if (!m_canvas || layer.border_box.width <= 0.1f || layer.border_box.height <= 0.1f) return;

    SkRect rect = SkRect::MakeXYWH((float)layer.border_box.x, (float)layer.border_box.y, (float)layer.border_box.width, (float)layer.border_box.height);
    SkVector radii[4] = {
        {(float)layer.border_radius.top_left_x, (float)layer.border_radius.top_left_y},
        {(float)layer.border_radius.top_right_x, (float)layer.border_radius.top_right_y},
        {(float)layer.border_radius.bottom_right_x, (float)layer.border_radius.bottom_right_y},
        {(float)layer.border_radius.bottom_left_x, (float)layer.border_radius.bottom_left_y}
    };
    SkRRect rrect;
    rrect.setRectRadii(rect, radii);

    if (m_tagging) {
        conic_gradient_info cgi;
        cgi.layer = layer;
        cgi.gradient = gradient;
        m_usedConicGradients.push_back(cgi);
        int index = (int)m_usedConicGradients.size();

        SkPaint paint;
        paint.setAntiAlias(false);
        paint.setColor(SkColorSetARGB(255, 0xFD, (index >> 8) & 0xFF, index & 0xFF));

        m_canvas->save();
        m_canvas->clipRRect(rrect, true);
        m_canvas->drawRect(rect, paint);
        m_canvas->restore();
        return;
    }

    std::vector<SkColor4f> colors;
    std::vector<float> positions;
    for (const auto& pt : gradient.color_points) {
        colors.push_back({pt.color.red/255.0f, pt.color.green/255.0f, pt.color.blue/255.0f, pt.color.alpha/255.0f});
        positions.push_back(pt.offset);
    }

    SkPoint center = {(float)gradient.position.x, (float)gradient.position.y};
    float startAngle = gradient.angle - 90.0f;

    SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(positions), SkTileMode::kClamp), SkGradient::Interpolation());
    auto shader = SkShaders::SweepGradient(center, startAngle, startAngle + 360.0f, skGrad, nullptr);

    SkPaint paint;
    paint.setShader(shader);
    paint.setAntiAlias(true);

    m_canvas->drawRRect(rrect, paint);
}
