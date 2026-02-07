#include "container_skia.h"

#include <iostream>
#include <regex>
#include <sstream>

#include "include/core/SkFontMetrics.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkString.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkClipOp.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkDashPathEffect.h"
#include "../utils/utils.h"

namespace {
SkRRect make_rrect(const litehtml::position &pos, const litehtml::border_radiuses &radius) {
    SkRect rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height);
    SkVector rad[4] = {{(float)radius.top_left_x, (float)radius.top_left_y},
                       {(float)radius.top_right_x, (float)radius.top_right_y},
                       {(float)radius.bottom_right_x, (float)radius.bottom_right_y},
                       {(float)radius.bottom_left_x, (float)radius.bottom_left_y}};
    SkRRect rrect;
    rrect.setRectRadii(rect, rad);
    return rrect;
}

std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n'\"");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n'\"");
    return s.substr(start, end - start + 1);
}

char32_t decode_utf8(const char** ptr) {
    const uint8_t* p = (const uint8_t*)*ptr;
    if (!*p) return 0;
    uint8_t b1 = *p++;
    if (!(b1 & 0x80)) {
        *ptr = (const char*)p;
        return b1;
    }
    if ((b1 & 0xE0) == 0xC0) {
        char32_t r = (b1 & 0x1F) << 6;
        if (*p) r |= (*p++ & 0x3F);
        *ptr = (const char*)p;
        return r;
    }
    if ((b1 & 0xF0) == 0xE0) {
        char32_t r = (b1 & 0x0F) << 12;
        if (*p) r |= (*p++ & 0x3F) << 6;
        if (*p) r |= (*p++ & 0x3F);
        *ptr = (const char*)p;
        return r;
    }
    if ((b1 & 0xF8) == 0xF0) {
        char32_t r = (b1 & 0x07) << 18;
        if (*p) r |= (*p++ & 0x3F) << 12;
        if (*p) r |= (*p++ & 0x3F) << 6;
        if (*p) r |= (*p++ & 0x3F);
        *ptr = (const char*)p;
        return r;
    }
    *ptr = (const char*)p;
    return 0;
}
}  // namespace

container_skia::container_skia(int w, int h, SkCanvas *canvas, SatoruContext &context, ResourceManager* rm, bool tagging)
    : m_canvas(canvas), m_width(w), m_height(h), m_context(context), m_resourceManager(rm), m_tagging(tagging) {
    m_last_clip_pos = {0, 0, 0, 0};
}

litehtml::uint_ptr container_skia::create_font(const litehtml::font_description &desc,
                                               const litehtml::document *doc,
                                               litehtml::font_metrics *fm) {
    SkFontStyle::Slant slant = desc.style == litehtml::font_style_normal
                                   ? SkFontStyle::kUpright_Slant
                                   : SkFontStyle::kItalic_Slant;

    auto typefaces = m_context.get_typefaces(desc.family, desc.weight, slant);

    if (typefaces.empty()) {
        // Legacy tracking
        m_missingFonts.insert({desc.family, desc.weight, slant});

        if (m_resourceManager) {
            std::string url = get_font_url(desc.family, desc.weight, slant);
            if (!url.empty()) {
                m_resourceManager->request(url, desc.family, ResourceType::Font);
            }
        }

        typefaces = m_context.get_typefaces("sans-serif", desc.weight, slant);
    }

    // Add global fallbacks if they are not already in the list
    for(auto& tf : m_context.fallbackTypefaces) {
        bool found = false;
        for(auto& existing : typefaces) {
            if(existing.get() == tf.get()) {
                found = true;
                break;
            }
        }
        if(!found) typefaces.push_back(tf);
    }

    font_info *fi = new font_info;
    fi->desc = desc;

    for (auto& typeface : typefaces) {
        fi->fonts.push_back(new SkFont(typeface, (float)desc.size));
    }

    if (fi->fonts.empty()) {
        fi->fonts.push_back(new SkFont(m_context.defaultTypeface, (float)desc.size));
    }

    SkFontMetrics skfm;
    fi->fonts[0]->getMetrics(&skfm);

    if (fm) {
        fm->ascent = (int)-skfm.fAscent;
        fm->descent = (int)skfm.fDescent;
        fm->height = fm->ascent + fm->descent;
        fm->x_height = (int)skfm.fXHeight;
    }

    fi->fm_ascent = (int)-skfm.fAscent;
    fi->fm_height = fi->fm_ascent + (int)skfm.fDescent;

    return (litehtml::uint_ptr)fi;
}

void container_skia::delete_font(litehtml::uint_ptr hFont) {
    font_info *fi = (font_info *)hFont;
    if (fi) {
        for (auto font : fi->fonts) delete font;
        delete fi;
    }
}

litehtml::pixel_t container_skia::text_width(const char *text, litehtml::uint_ptr hFont) {
    font_info *fi = (font_info *)hFont;
    if (!fi || fi->fonts.empty()) return 0;

    double total_width = 0;
    const char *p = text;
    const char *run_start = p;
    SkFont *current_font = nullptr;

    while (*p) {
        const char *next_p = p;
        char32_t u = decode_utf8(&next_p);

        SkFont *font = nullptr;
        for (auto f : fi->fonts) {
            SkGlyphID glyph;
            f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>((const SkUnichar *)&u, 1), SkSpan<SkGlyphID>(&glyph, 1));
            if (glyph != 0) {
                font = f;
                break;
            }
        }
        if (!font) font = fi->fonts[0];

        if (font != current_font) {
            if (current_font && p > run_start) {
                total_width += current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
            }
            run_start = p;
            current_font = font;
        }
        p = next_p;
    }

    if (current_font && p > run_start) {
        total_width += current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
    }

    return (litehtml::pixel_t)total_width;
}

void container_skia::draw_text(litehtml::uint_ptr hdc, const char *text, litehtml::uint_ptr hFont,
                               litehtml::web_color color, const litehtml::position &pos) {
    if (!m_canvas) return;
    font_info *fi = (font_info *)hFont;
    if (!fi || fi->fonts.empty()) return;

    SkPaint paint;
    paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    paint.setAntiAlias(true);

    double x_offset = 0;
    const char *p = text;
    const char *run_start = p;
    SkFont *current_font = nullptr;

    while (*p) {
        const char *next_p = p;
        char32_t u = decode_utf8(&next_p);

        SkFont *font = nullptr;
        for (auto f : fi->fonts) {
            SkGlyphID glyph;
            f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>((const SkUnichar *)&u, 1), SkSpan<SkGlyphID>(&glyph, 1));
            if (glyph != 0) {
                font = f;
                break;
            }
        }
        if (!font) font = fi->fonts[0];

        if (font != current_font) {
            if (current_font && p > run_start) {
                m_canvas->drawSimpleText(run_start, p - run_start, SkTextEncoding::kUTF8,
                                         (float)pos.x + (float)x_offset, (float)pos.y + (float)fi->fm_ascent,      
                                         *current_font, paint);
                x_offset += current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
            }
            run_start = p;
            current_font = font;
        }
        p = next_p;
    }

    if (current_font && p > run_start) {
        m_canvas->drawSimpleText(run_start, p - run_start, SkTextEncoding::kUTF8, (float)pos.x + (float)x_offset,  
                                 (float)pos.y + (float)fi->fm_ascent, *current_font, paint);
        x_offset += current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
    }

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none) {
        litehtml::web_color dc = fi->desc.decoration_color;
        if (dc == litehtml::web_color::current_color) {
            dc = color;
        }

        SkPaint decPaint;
        decPaint.setColor(SkColorSetARGB(dc.alpha, dc.red, dc.green, dc.blue));
        decPaint.setAntiAlias(true);

        float thickness = (float)fi->desc.decoration_thickness.val();
        if (fi->desc.decoration_thickness.is_predefined() || thickness <= 0) {
            thickness = std::max(1.0f, (float)fi->desc.size / 15.0f);
        }
        decPaint.setStrokeWidth(thickness);
        decPaint.setStyle(SkPaint::kStroke_Style);

        if (fi->desc.decoration_style == litehtml::text_decoration_style_dotted) {
            float intervals[] = {thickness, thickness};
            decPaint.setPathEffect(SkDashPathEffect::Make(SkSpan(intervals), 0));
        } else if (fi->desc.decoration_style == litehtml::text_decoration_style_dashed) {
            float intervals[] = {thickness * 3, thickness * 3};
            decPaint.setPathEffect(SkDashPathEffect::Make(SkSpan(intervals), 0));
        }

        float x1 = (float)pos.x;
        float x2 = (float)pos.x + (float)x_offset;

        if (fi->desc.decoration_line & litehtml::text_decoration_line_underline) {
            float y = (float)pos.y + (float)fi->fm_ascent + thickness;
            m_canvas->drawLine(x1, y, x2, y, decPaint);
        }
        if (fi->desc.decoration_line & litehtml::text_decoration_line_overline) {
            float y = (float)pos.y;
            m_canvas->drawLine(x1, y, x2, y, decPaint);
        }
        if (fi->desc.decoration_line & litehtml::text_decoration_line_line_through) {
            float y = (float)pos.y + (float)fi->fm_ascent - (float)fi->fm_height * 0.3f;
            m_canvas->drawLine(x1, y, x2, y, decPaint);
        }
    }
}

void container_skia::draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector &shadows,
                                     const litehtml::position &pos,
                                     const litehtml::border_radiuses &radius, bool inset) {
    if (!m_canvas) return;
    if (m_tagging) {
        for (const auto &s : shadows) {
            if (s.inset != inset) continue;

            shadow_info info;
            info.color = s.color;
            info.blur = (float)s.blur.val();
            info.x = (float)s.x.val();
            info.y = (float)s.y.val();
            info.spread = (float)s.spread.val();
            info.inset = inset;
            info.box_pos = pos;
            info.box_radius = radius;

            int index = 0;
            auto it = m_shadowToIndex.find(info);
            if (it == m_shadowToIndex.end()) {
                m_usedShadows.push_back(info);
                index = (int)m_usedShadows.size();
                m_shadowToIndex[info] = index;
            } else {
                index = it->second;
            }

            SkPaint paint;
            // Shadow tag: R=0, G=0, B=index, A=254 (#0000xx with opacity)
            paint.setColor(SkColorSetARGB(254, 0, 0, (index & 0xFF)));
            m_canvas->drawRRect(make_rrect(pos, radius), paint);
        }
        return;
    }

    // Direct rendering for PNG
    for (const auto &s : shadows) {
        if (s.inset != inset) continue;

        SkRRect box_rrect = make_rrect(pos, radius);
        SkColor shadow_color = SkColorSetARGB(s.color.alpha, s.color.red, s.color.green, s.color.blue);
        float blur_std_dev = (float)s.blur.val() * 0.5f;

        m_canvas->save();
        if (inset) {
            m_canvas->clipRRect(box_rrect, true);

            SkRRect shadow_rrect = box_rrect;
            shadow_rrect.inset(-(float)s.spread.val(), -(float)s.spread.val());

            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setColor(shadow_color);
            if (blur_std_dev > 0) {
                paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_std_dev));
            }

            SkRect huge_rect = box_rrect.rect();
            huge_rect.outset(blur_std_dev * 3 + std::abs((float)s.x.val()) + 100,
                             blur_std_dev * 3 + std::abs((float)s.y.val()) + 100);

            SkPath path = SkPathBuilder()
                .addRect(huge_rect)
                .addRRect(shadow_rrect, SkPathDirection::kCCW)
                .detach();

            m_canvas->translate((float)s.x.val(), (float)s.y.val());
            m_canvas->drawPath(path, paint);
        } else {
            m_canvas->clipRRect(box_rrect, SkClipOp::kDifference, true);

            SkRRect shadow_rrect = box_rrect;
            shadow_rrect.outset((float)s.spread.val(), (float)s.spread.val());

            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setColor(shadow_color);
            if (blur_std_dev > 0) {
                paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_std_dev));
            }

            m_canvas->translate((float)s.x.val(), (float)s.y.val());
            m_canvas->drawRRect(shadow_rrect, paint);
        }
        m_canvas->restore();
    }
}

void container_skia::draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                                const std::string &url, const std::string &base_url) {
    if (!m_canvas) return;
    image_draw_info draw;
    draw.url = url;
    draw.layer = layer;
    m_usedImageDraws.push_back(draw);

    int index = (int)m_usedImageDraws.size();

    SkPaint paint;
    // Image tag: R=0, G=0, B=index, A=255 (#0000xx)
    paint.setColor(SkColorSetARGB(255, 0, 0, (index & 0xFF)));
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), paint);
}

void container_skia::draw_solid_fill(litehtml::uint_ptr hdc,
                                     const litehtml::background_layer &layer,
                                     const litehtml::web_color &color) {
    if (!m_canvas) return;
    SkPaint paint;
    paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    paint.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), paint);
}

void container_skia::draw_linear_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::linear_gradient &gradient) {
    if (!m_canvas) return;
    SkPoint pts[2] = {SkPoint::Make((float)gradient.start.x, (float)gradient.start.y),
                      SkPoint::Make((float)gradient.end.x, (float)gradient.end.y)};

    std::vector<SkColor4f> colors;
    std::vector<float> pos;
    for (const auto &stop : gradient.color_points) {
        colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                          stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
        pos.push_back(stop.offset);
    }

    SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                      SkGradient::Interpolation());
    auto shader = SkShaders::LinearGradient(pts, skGrad);

    SkPaint paint;
    paint.setShader(shader);
    paint.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), paint);
}

void container_skia::draw_radial_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::radial_gradient &gradient) {
    if (!m_canvas) return;
    SkPoint center = SkPoint::Make((float)gradient.position.x, (float)gradient.position.y);
    SkScalar radius = (float)std::max(gradient.radius.x, gradient.radius.y);

    std::vector<SkColor4f> colors;
    std::vector<float> pos;
    for (const auto &stop : gradient.color_points) {
        colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                          stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
        pos.push_back(stop.offset);
    }

    SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                      SkGradient::Interpolation());
    auto shader = SkShaders::RadialGradient(center, radius, skGrad);

    SkPaint paint;
    paint.setShader(shader);
    paint.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), paint);
}

void container_skia::draw_conic_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::conic_gradient &gradient) {
    if (!m_canvas || !m_tagging) return;

    conic_gradient_info info;
    info.layer = layer;
    info.gradient = gradient;
    m_usedConicGradients.push_back(info);

    int index = (int)m_usedConicGradients.size();

    SkPaint paint;
    // Conic tag: R=0, G=0, B=index, A=253 (#0000xx with opacity)
    paint.setColor(SkColorSetARGB(253, 0, 0, (index & 0xFF)));
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), paint);
}

void container_skia::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders &borders,
                                  const litehtml::position &draw_pos, bool root) {
    if (!m_canvas) return;

    bool uniform =
        borders.top.width == borders.bottom.width && borders.top.width == borders.left.width &&
        borders.top.width == borders.right.width && borders.top.color == borders.bottom.color &&
        borders.top.color == borders.left.color && borders.top.color == borders.right.color;

    if (uniform && borders.top.width > 0) {
        SkPaint paint;
        paint.setColor(SkColorSetARGB(borders.top.color.alpha, borders.top.color.red,
                                      borders.top.color.green, borders.top.color.blue));
        paint.setStrokeWidth((float)borders.top.width);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setAntiAlias(true);

        float half = (float)borders.top.width / 2.0f;

        bool has_radius = borders.radius.top_left_x > 0 || borders.radius.top_left_y > 0 ||
                          borders.radius.top_right_x > 0 || borders.radius.top_right_y > 0 ||
                          borders.radius.bottom_right_x > 0 || borders.radius.bottom_right_y > 0 ||
                          borders.radius.bottom_left_x > 0 || borders.radius.bottom_left_y > 0;

        if (has_radius) {
            SkRRect rrect = make_rrect(draw_pos, borders.radius);
            rrect.inset(half, half);
            m_canvas->drawRRect(rrect, paint);
        } else {
            SkRect rect = SkRect::MakeXYWH((float)draw_pos.x + half, (float)draw_pos.y + half,
                                           (float)draw_pos.width - borders.top.width,
                                           (float)draw_pos.height - borders.top.width);
            m_canvas->drawRect(rect, paint);
        }
    } else {
        bool has_radius = borders.radius.top_left_x > 0 || borders.radius.top_left_y > 0 ||
                          borders.radius.top_right_x > 0 || borders.radius.top_right_y > 0 ||
                          borders.radius.bottom_right_x > 0 || borders.radius.bottom_right_y > 0 ||
                          borders.radius.bottom_left_x > 0 || borders.radius.bottom_left_y > 0;

        // Optimization: if all non-zero borders have the same color, we can draw them as a single path.
        // This avoids gaps at the corners caused by segment approximations not matching the clip path.
        litehtml::web_color common_color = {0, 0, 0, 0};
        bool multi_color = false;
        int active_borders = 0;
        auto check_border = [&](const litehtml::border &b) {
            if (b.width > 0 && b.style != litehtml::border_style_none && b.style != litehtml::border_style_hidden) {
                if (active_borders == 0) common_color = b.color;
                else if (b.color != common_color) multi_color = true;
                active_borders++;
            }
        };
        check_border(borders.top);
        check_border(borders.bottom);
        check_border(borders.left);
        check_border(borders.right);

        if (active_borders == 0) return;

        m_canvas->save();
        
        SkRRect outer = make_rrect(draw_pos, borders.radius);
        SkRect inner_rect = SkRect::MakeXYWH(
            (float)draw_pos.x + (float)borders.left.width,
            (float)draw_pos.y + (float)borders.top.width,
            (float)draw_pos.width - (float)borders.left.width - (float)borders.right.width,
            (float)draw_pos.height - (float)borders.top.width - (float)borders.bottom.width
        );

        if (!multi_color) {
            // Same color for all borders: draw as a single path (Outer RRect - Inner RRect)
            SkPaint paint;
            paint.setColor(SkColorSetARGB(common_color.alpha, common_color.red, common_color.green, common_color.blue));
            paint.setAntiAlias(true);

            if (inner_rect.width() > 0 && inner_rect.height() > 0) {
                SkVector inner_rad[4] = {
                    {std::max(0.0f, (float)borders.radius.top_left_x - (float)borders.left.width),
                     std::max(0.0f, (float)borders.radius.top_left_y - (float)borders.top.width)},
                    {std::max(0.0f, (float)borders.radius.top_right_x - (float)borders.right.width),
                     std::max(0.0f, (float)borders.radius.top_right_y - (float)borders.top.width)},
                    {std::max(0.0f, (float)borders.radius.bottom_right_x - (float)borders.right.width),
                     std::max(0.0f, (float)borders.radius.bottom_right_y - (float)borders.bottom.width)},
                    {std::max(0.0f, (float)borders.radius.bottom_left_x - (float)borders.left.width),
                     std::max(0.0f, (float)borders.radius.bottom_left_y - (float)borders.bottom.width)}
                };
                SkRRect inner;
                inner.setRectRadii(inner_rect, inner_rad);

                SkPath path = SkPathBuilder()
                    .addRRect(outer, SkPathDirection::kCW)
                    .addRRect(inner, SkPathDirection::kCCW)
                    .detach();
                m_canvas->drawPath(path, paint);
            } else {
                m_canvas->drawRRect(outer, paint);
            }
        } else {
            // Multi-color or fallback: draw as segments
            if (has_radius) {
                m_canvas->clipRRect(outer, true);
            }

            auto draw_border_segment = [&](const litehtml::border &b, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
                if (b.width <= 0 || b.style == litehtml::border_style_none || b.style == litehtml::border_style_hidden)
                    return;

                SkPath path = SkPathBuilder()
                    .moveTo(x1, y1)
                    .lineTo(x2, y2)
                    .lineTo(x3, y3)
                    .lineTo(x4, y4)
                    .close()
                    .detach();

                SkPaint paint;
                paint.setColor(SkColorSetARGB(b.color.alpha, b.color.red, b.color.green, b.color.blue));
                paint.setAntiAlias(true);
                m_canvas->drawPath(path, paint);
            };

            float x = (float)draw_pos.x;
            float y = (float)draw_pos.y;
            float w = (float)draw_pos.width;
            float h = (float)draw_pos.height;
            float lw = (float)borders.left.width;
            float tw = (float)borders.top.width;
            float rw = (float)borders.right.width;
            float bw = (float)borders.bottom.width;

            draw_border_segment(borders.top, x, y, x + w, y, x + w - rw, y + tw, x + lw, y + tw);
            draw_border_segment(borders.bottom, x, y + h, x + w, y + h, x + w - rw, y + h - bw, x + lw, y + h - bw);
            draw_border_segment(borders.left, x, y, x + lw, y + tw, x + lw, y + h - bw, x, y + h);
            draw_border_segment(borders.right, x + w, y, x + w - rw, y + tw, x + w - rw, y + h - bw, x + w, y + h);
        }

        m_canvas->restore();
    }
}

litehtml::pixel_t container_skia::pt_to_px(float pt) const {
    return (litehtml::pixel_t)(pt * 96.0f / 72.0f);
}

litehtml::pixel_t container_skia::get_default_font_size() const { return 16; }

const char *container_skia::get_default_font_name() const { return "sans-serif"; }

void container_skia::load_image(const char *src, const char *baseurl, bool redraw_on_ready) {
    if (m_resourceManager && src && *src) {
        m_resourceManager->request(src, src, ResourceType::Image);
    }
}

void container_skia::get_image_size(const char *src, const char *baseurl, litehtml::size &sz) {
    int w, h;
    if (m_context.get_image_size(src, w, h)) {
        sz.width = w;
        sz.height = h;
    } else {
        sz.width = 0;
        sz.height = 0;
    }
}

void container_skia::get_viewport(litehtml::position &viewport) const {
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = m_width;
    viewport.height = m_height;
}

void container_skia::transform_text(litehtml::string &text, litehtml::text_transform tt) {
    if (text.empty()) return;
    switch (tt) {
        case litehtml::text_transform_uppercase:
            for (auto &c : text) c = (char)toupper(c);
            break;
        case litehtml::text_transform_lowercase:
            for (auto &c : text) c = (char)tolower(c);
            break;
        default:
            break;
    }
}

void container_skia::import_css(litehtml::string &text, const litehtml::string &url,
                                litehtml::string &baseurl) {
    if (!url.empty()) {
        m_requiredCss.push_back(url); // Legacy
        if (m_resourceManager) {
            m_resourceManager->request(url, url, ResourceType::Css);
        }
    } else {
        scan_font_faces(text);
    }
}

void container_skia::scan_font_faces(const std::string &css) {
    // Robust regex for @font-face and its properties
    std::regex fontFaceRegex("@font-face\\s*\\{([^}]+)\\}", std::regex::icase);
    std::regex familyRegex("font-family:\\s*([^;]+);?", std::regex::icase);
    std::regex urlRegex("url\\s*\\(\\s*['\"]?([^'\")]+)['\"]?\\s*\\)", std::regex::icase);

    auto words_begin = std::sregex_iterator(css.begin(), css.end(), fontFaceRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::string body = (*i)[1].str();
        std::smatch m;

        font_request req;
        req.weight = 400;
        req.slant = SkFontStyle::kUpright_Slant;

        if (std::regex_search(body, m, familyRegex)) {
            req.family = clean_font_name(m[1].str().c_str());
        }

        if (!req.family.empty() && std::regex_search(body, m, urlRegex)) {
            std::string url = trim(m[1].str());
            m_fontFaces[req] = url;
        }
    }
}

std::string container_skia::get_font_url(const std::string &family, int weight,
                                         SkFontStyle::Slant slant) const {
    std::stringstream ss(family);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim leading/trailing whitespace and quotes from the family name part
        std::string trimmed = trim(item);
        std::string cleanFamily = clean_font_name(trimmed.c_str());

        // Exact match
        font_request req = {cleanFamily, weight, slant};
        auto it = m_fontFaces.find(req);
        if (it != m_fontFaces.end()) {
            return it->second;
        }

        // Nearest weight match for the same family/slant
        int minDiff = 1000;
        std::string bestUrl = "";
        for (const auto &pair : m_fontFaces) {
            if (pair.first.family == cleanFamily && pair.first.slant == slant) {
                int diff = std::abs(pair.first.weight - weight);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestUrl = pair.second;
                }
            }
        }
        if (!bestUrl.empty()) {
             return bestUrl;
        }
    }

    return "";
}

void container_skia::set_clip(const litehtml::position &pos,
                              const litehtml::border_radiuses &bdr_radius) {
    if (m_canvas) {
        m_canvas->save();
        // Force path-based clipping to ensure accuracy in SVG output.
        // SkSVGCanvas approximates RRect with a rect+rx/ry, which fails for non-uniform corners.
        SkPath path = SkPathBuilder().addRRect(make_rrect(pos, bdr_radius)).detach();
        m_canvas->clipPath(path, true);
    }
    m_last_clip_pos = pos;
    m_last_clip_radius = bdr_radius;
}

void container_skia::del_clip() {
    if (m_canvas) m_canvas->restore();
}

void container_skia::get_media_features(litehtml::media_features &features) const {
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

void container_skia::get_language(litehtml::string &language, litehtml::string &culture) const {
    language = "en";
    culture = "en-US";
}