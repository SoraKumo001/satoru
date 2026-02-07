#include "container_skia.h"

#include <iostream>
#include <regex>
#include <sstream>

#include "../utils/utils.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkClipOp.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkString.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"

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

char32_t decode_utf8(const char **ptr) {
    const uint8_t *p = (const uint8_t *)*ptr;
    if (!*p) return 0;
    uint8_t b1 = *p++;
    if (!(b1 & 0x80)) {
        *ptr = (const char *)p;
        return b1;
    }
    if ((b1 & 0xE0) == 0xC0) {
        char32_t r = (b1 & 0x1F) << 6;
        if (*p) r |= (*p++ & 0x3F);
        *ptr = (const char *)p;
        return r;
    }
    if ((b1 & 0xF0) == 0xE0) {
        char32_t r = (b1 & 0x0F) << 12;
        if (*p) r |= (*p++ & 0x3F) << 6;
        if (*p) r |= (*p++ & 0x3F);
        *ptr = (const char *)p;
        return r;
    }
    if ((b1 & 0xF8) == 0xF0) {
        char32_t r = (b1 & 0x07) << 18;
        if (*p) r |= (*p++ & 0x3F) << 12;
        if (*p) r |= (*p++ & 0x3F) << 6;
        if (*p) r |= (*p++ & 0x3F);
        *ptr = (const char *)p;
        return r;
    }
    *ptr = (const char *)p;
    return 0;
}
}  // namespace

container_skia::container_skia(int w, int h, SkCanvas *canvas, SatoruContext &context,
                               ResourceManager *rm, bool tagging)
    : m_canvas(canvas),
      m_width(w),
      m_height(h),
      m_context(context),
      m_resourceManager(rm),
      m_tagging(tagging) {
    m_last_clip_pos = {0, 0, 0, 0};
}

litehtml::uint_ptr container_skia::create_font(const litehtml::font_description &desc,
                                               const litehtml::document *doc,
                                               litehtml::font_metrics *fm) {
    SkFontStyle::Slant slant = desc.style == litehtml::font_style_normal
                                   ? SkFontStyle::kUpright_Slant
                                   : SkFontStyle::kItalic_Slant;

    bool fake_bold = false;
    auto typefaces = m_context.get_typefaces(desc.family, desc.weight, slant, fake_bold);

    if (typefaces.empty()) {
        m_missingFonts.insert({desc.family, desc.weight, slant});
        if (m_resourceManager) {
            std::string url = get_font_url(desc.family, desc.weight, slant);
            if (!url.empty()) m_resourceManager->request(url, desc.family, ResourceType::Font);
        }
        typefaces = m_context.get_typefaces("sans-serif", desc.weight, slant, fake_bold);
    }

    for (auto &tf : m_context.fallbackTypefaces) {
        bool found = false;
        for (auto &existing : typefaces) {
            if (existing.get() == tf.get()) {
                found = true;
                break;
            }
        }
        if (!found) typefaces.push_back(tf);
    }

    font_info *fi = new font_info;
    fi->desc = desc;
    fi->fake_bold = fake_bold;

    for (auto &typeface : typefaces) fi->fonts.push_back(new SkFont(typeface, (float)desc.size));
    if (fi->fonts.empty() && m_context.defaultTypeface)
        fi->fonts.push_back(new SkFont(m_context.defaultTypeface, (float)desc.size));
    else if (fi->fonts.empty())
        fi->fonts.push_back(new SkFont(SkTypeface::MakeEmpty(), (float)desc.size));

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
            f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>((const SkUnichar *)&u, 1),
                                               SkSpan<SkGlyphID>(&glyph, 1));
            if (glyph != 0) {
                font = f;
                break;
            }
        }
        if (!font) font = fi->fonts[0];
        if (font != current_font) {
            if (current_font && p > run_start)
                total_width +=
                    current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
            run_start = p;
            current_font = font;
        }
        p = next_p;
    }
    if (current_font && p > run_start)
        total_width += current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
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
            f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>((const SkUnichar *)&u, 1),
                                               SkSpan<SkGlyphID>(&glyph, 1));
            if (glyph != 0) {
                font = f;
                break;
            }
        }
        if (!font) font = fi->fonts[0];
        if (font != current_font) {
            if (current_font && p > run_start) {
                SkFont render_font = *current_font;
                if (fi->fake_bold) render_font.setEmbolden(true);
                m_canvas->drawSimpleText(run_start, p - run_start, SkTextEncoding::kUTF8,
                                         (float)pos.x + (float)x_offset,
                                         (float)pos.y + (float)fi->fm_ascent, render_font, paint);
                x_offset +=
                    current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
            }
            run_start = p;
            current_font = font;
        }
        p = next_p;
    }
    if (current_font && p > run_start) {
        SkFont render_font = *current_font;
        if (fi->fake_bold) render_font.setEmbolden(true);
        m_canvas->drawSimpleText(run_start, p - run_start, SkTextEncoding::kUTF8,
                                 (float)pos.x + (float)x_offset,
                                 (float)pos.y + (float)fi->fm_ascent, render_font, paint);
        x_offset += current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
    }

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none) {
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
                m_canvas->drawLine((float)pos.x, y - gap / 2, (float)pos.x + (float)x_offset,
                                   y - gap / 2, dec_paint);
                m_canvas->drawLine((float)pos.x, y + gap / 2, (float)pos.x + (float)x_offset,
                                   y + gap / 2, dec_paint);
            } else {
                m_canvas->drawLine((float)pos.x, y, (float)pos.x + (float)x_offset, y, dec_paint);
            }
        };

        if (fi->desc.decoration_line & litehtml::text_decoration_line_underline) {
            draw_decoration_line((float)pos.y + (float)fi->fm_ascent +
                                 (float)fi->desc.underline_offset.val() + 1.0f);
        }
        if (fi->desc.decoration_line & litehtml::text_decoration_line_overline) {
            draw_decoration_line((float)pos.y);
        }
        if (fi->desc.decoration_line & litehtml::text_decoration_line_line_through) {
            draw_decoration_line((float)pos.y + (float)fi->fm_ascent * 0.65f);
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
            m_usedShadows.push_back(info);
            int index = (int)m_usedShadows.size();
            SkPaint p;
            p.setColor(SkColorSetARGB(255, 0, 1, (index & 0xFF)));
            m_canvas->drawRRect(make_rrect(pos, radius), p);
        }
        return;
    }
    for (const auto &s : shadows) {
        if (s.inset != inset) continue;
        SkRRect box_rrect = make_rrect(pos, radius);
        SkColor shadow_color =
            SkColorSetARGB(s.color.alpha, s.color.red, s.color.green, s.color.blue);
        float blur_std_dev = (float)s.blur.val() * 0.5f;
        m_canvas->save();
        if (inset) {
            m_canvas->clipRRect(box_rrect, true);
            SkRRect shadow_rrect = box_rrect;
            shadow_rrect.inset(-(float)s.spread.val(), -(float)s.spread.val());
            SkPaint p;
            p.setAntiAlias(true);
            p.setColor(shadow_color);
            if (blur_std_dev > 0)
                p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_std_dev));
            SkRect hr = box_rrect.rect();
            hr.outset(blur_std_dev * 3 + std::abs((float)s.x.val()) + 100,
                      blur_std_dev * 3 + std::abs((float)s.y.val()) + 100);
            m_canvas->translate((float)s.x.val(), (float)s.y.val());
            m_canvas->drawPath(
                SkPathBuilder().addRect(hr).addRRect(shadow_rrect, SkPathDirection::kCCW).detach(),
                p);
        } else {
            m_canvas->clipRRect(box_rrect, SkClipOp::kDifference, true);
            SkRRect shadow_rrect = box_rrect;
            shadow_rrect.outset((float)s.spread.val(), (float)s.spread.val());
            SkPaint p;
            p.setAntiAlias(true);
            p.setColor(shadow_color);
            if (blur_std_dev > 0)
                p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_std_dev));
            m_canvas->translate((float)s.x.val(), (float)s.y.val());
            m_canvas->drawRRect(shadow_rrect, p);
        }
        m_canvas->restore();
    }
}

void container_skia::draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                                const std::string &url, const std::string &base_url) {
    if (!m_canvas) return;
    if (m_tagging) {
        image_draw_info draw;
        draw.url = url;
        draw.layer = layer;
        m_usedImageDraws.push_back(draw);
        int index = (int)m_usedImageDraws.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, 0, (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
        auto it = m_context.imageCache.find(url);
        if (it != m_context.imageCache.end() && it->second.skImage) {
            SkPaint p;
            p.setAntiAlias(true);
            m_canvas->save();
            m_canvas->clipRRect(make_rrect(layer.border_box, layer.border_radius), true);
            m_canvas->drawImageRect(
                it->second.skImage,
                SkRect::MakeXYWH((float)layer.origin_box.x, (float)layer.origin_box.y,
                                 (float)layer.origin_box.width, (float)layer.origin_box.height),
                SkSamplingOptions(SkFilterMode::kLinear), &p);
            m_canvas->restore();
        }
    }
}

void container_skia::draw_solid_fill(litehtml::uint_ptr hdc,
                                     const litehtml::background_layer &layer,
                                     const litehtml::web_color &color) {
    if (!m_canvas) return;
    SkPaint p;
    p.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    p.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
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
    SkGradient grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                    SkGradient::Interpolation());
    SkPaint p;
    p.setShader(SkShaders::LinearGradient(pts, grad));
    p.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
}

void container_skia::draw_radial_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::radial_gradient &gradient) {
    if (!m_canvas) return;
    SkPoint center = SkPoint::Make((float)gradient.position.x, (float)gradient.position.y);
    float rx = (float)gradient.radius.x, ry = (float)gradient.radius.y;
    if (rx <= 0 || ry <= 0) return;
    std::vector<SkColor4f> colors;
    std::vector<float> pos;
    for (const auto &stop : gradient.color_points) {
        colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                          stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
        pos.push_back(stop.offset);
    }
    SkMatrix matrix;
    matrix.setScale(1.0f, ry / rx, center.x(), center.y());
    SkGradient grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                    SkGradient::Interpolation());
    SkPaint p;
    p.setShader(SkShaders::RadialGradient(center, rx, grad, &matrix));
    p.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
}

void container_skia::draw_conic_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::conic_gradient &gradient) {
    if (!m_canvas) return;
    if (m_tagging) {
        conic_gradient_info info;
        info.layer = layer;
        info.gradient = gradient;
        m_usedConicGradients.push_back(info);
        int index = (int)m_usedConicGradients.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, 1, (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
        SkPoint center = SkPoint::Make((float)gradient.position.x, (float)gradient.position.y);
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        for (const auto &stop : gradient.color_points) {
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
            pos.push_back(stop.offset);
        }
        SkGradient grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                        SkGradient::Interpolation());
        SkPaint p;
        p.setShader(
            SkShaders::SweepGradient(center, gradient.angle, gradient.angle + 360.0f, grad));
        p.setAntiAlias(true);
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    }
}

void container_skia::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders &borders,
                                  const litehtml::position &draw_pos, bool root) {
    if (!m_canvas) return;
    bool uniform =
        borders.top.width == borders.bottom.width && borders.top.width == borders.left.width &&
        borders.top.width == borders.right.width && borders.top.color == borders.bottom.color &&
        borders.top.color == borders.left.color && borders.top.color == borders.right.color;
    if (uniform && borders.top.width > 0) {
        SkPaint p;
        p.setColor(SkColorSetARGB(borders.top.color.alpha, borders.top.color.red,
                                  borders.top.color.green, borders.top.color.blue));
        p.setStrokeWidth((float)borders.top.width);
        p.setStyle(SkPaint::kStroke_Style);
        p.setAntiAlias(true);
        SkRRect rr = make_rrect(draw_pos, borders.radius);
        rr.inset((float)borders.top.width / 2.0f, (float)borders.top.width / 2.0f);
        m_canvas->drawRRect(rr, p);
    } else {
        litehtml::web_color common = {0, 0, 0, 0};
        bool mc = false;
        int active = 0;
        auto chk = [&](const litehtml::border &b) {
            if (b.width > 0 && b.style != litehtml::border_style_none &&
                b.style != litehtml::border_style_hidden) {
                if (active == 0)
                    common = b.color;
                else if (b.color != common)
                    mc = true;
                active++;
            }
        };
        chk(borders.top);
        chk(borders.bottom);
        chk(borders.left);
        chk(borders.right);
        if (active == 0) return;
        m_canvas->save();
        SkRRect outer = make_rrect(draw_pos, borders.radius);
        if (!mc) {
            SkPaint p;
            p.setColor(SkColorSetARGB(common.alpha, common.red, common.green, common.blue));
            p.setAntiAlias(true);
            SkRect ir = SkRect::MakeXYWH(
                (float)draw_pos.x + borders.left.width, (float)draw_pos.y + borders.top.width,
                (float)draw_pos.width - borders.left.width - borders.right.width,
                (float)draw_pos.height - borders.top.width - borders.bottom.width);
            if (ir.width() > 0 && ir.height() > 0) {
                SkVector rads[4] = {
                    {std::max(0.0f, (float)borders.radius.top_left_x - (float)borders.left.width),
                     std::max(0.0f, (float)borders.radius.top_left_y - (float)borders.top.width)},
                    {std::max(0.0f, (float)borders.radius.top_right_x - (float)borders.right.width),
                     std::max(0.0f, (float)borders.radius.top_right_y - (float)borders.top.width)},
                    {std::max(0.0f,
                              (float)borders.radius.bottom_right_x - (float)borders.right.width),
                     std::max(0.0f,
                              (float)borders.radius.bottom_right_y - (float)borders.bottom.width)},
                    {std::max(0.0f,
                              (float)borders.radius.bottom_left_x - (float)borders.left.width),
                     std::max(0.0f,
                              (float)borders.radius.bottom_left_y - (float)borders.bottom.width)}};
                SkRRect inner;
                inner.setRectRadii(ir, rads);
                m_canvas->drawPath(SkPathBuilder()
                                       .addRRect(outer, SkPathDirection::kCW)
                                       .addRRect(inner, SkPathDirection::kCCW)
                                       .detach(),
                                   p);
            } else
                m_canvas->drawRRect(outer, p);
        } else {
            m_canvas->clipRRect(outer, true);
            auto dseg = [&](const litehtml::border &b, float x1, float y1, float x2, float y2,
                            float x3, float y3, float x4, float y4) {
                if (b.width <= 0 || b.style == litehtml::border_style_none ||
                    b.style == litehtml::border_style_hidden)
                    return;
                SkPaint p;
                p.setColor(SkColorSetARGB(b.color.alpha, b.color.red, b.color.green, b.color.blue));
                p.setAntiAlias(true);
                m_canvas->drawPath(SkPathBuilder()
                                       .moveTo(x1, y1)
                                       .lineTo(x2, y2)
                                       .lineTo(x3, y3)
                                       .lineTo(x4, y4)
                                       .close()
                                       .detach(),
                                   p);
            };
            float x = (float)draw_pos.x, y = (float)draw_pos.y, w = (float)draw_pos.width,
                  h = (float)draw_pos.height, lw = (float)borders.left.width,
                  tw = (float)borders.top.width, rw = (float)borders.right.width,
                  bw = (float)borders.bottom.width;
            dseg(borders.top, x, y, x + w, y, x + w - rw, y + tw, x + lw, y + tw);
            dseg(borders.bottom, x, y + h, x + w, y + h, x + w - rw, y + h - bw, x + lw,
                 y + h - bw);
            dseg(borders.left, x, y, x + lw, y + tw, x + lw, y + h - bw, x, y + h);
            dseg(borders.right, x + w, y, x + w - rw, y + tw, x + w - rw, y + h - bw, x + w, y + h);
        }
        m_canvas->restore();
    }
}

litehtml::pixel_t container_skia::pt_to_px(float pt) const { return pt * 96.0f / 72.0f; }
litehtml::pixel_t container_skia::get_default_font_size() const { return 16.0f; }
const char *container_skia::get_default_font_name() const { return "sans-serif"; }
void container_skia::load_image(const char *src, const char *baseurl, bool redraw_on_ready) {
    if (m_resourceManager && src && *src) m_resourceManager->request(src, src, ResourceType::Image);
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
    if (tt == litehtml::text_transform_uppercase)
        for (auto &c : text) c = (char)toupper(c);
    else if (tt == litehtml::text_transform_lowercase)
        for (auto &c : text) c = (char)tolower(c);
}
void container_skia::import_css(litehtml::string &text, const litehtml::string &url,
                                litehtml::string &baseurl) {
    if (!url.empty() && m_resourceManager)
        m_resourceManager->request(url, url, ResourceType::Css);
    else
        scan_font_faces(text);
}
void container_skia::scan_font_faces(const std::string &css) {
    std::regex fontFaceRegex("@font-face\\s*\\{([^}]+)\\}", std::regex::icase);
    std::regex familyRegex("font-family:\\s*([^;]+);?", std::regex::icase);
    std::regex weightRegex("font-weight:\\s*([^;]+);?", std::regex::icase);
    std::regex styleRegex("font-style:\\s*([^;]+);?", std::regex::icase);
    std::regex urlRegex("url\\s*\\(\\s*['\"]?([^'\")]+)['\"]?\\s*\\)", std::regex::icase);
    auto words_begin = std::sregex_iterator(css.begin(), css.end(), fontFaceRegex);
    for (std::sregex_iterator i = words_begin; i != std::sregex_iterator(); ++i) {
        std::string body = (*i)[1].str();
        std::smatch m;
        font_request req;
        req.weight = 400;
        req.slant = SkFontStyle::kUpright_Slant;
        if (std::regex_search(body, m, familyRegex))
            req.family = clean_font_name(m[1].str().c_str());
        if (std::regex_search(body, m, weightRegex)) {
            std::string w = trim(m[1].str());
            if (w == "bold")
                req.weight = 700;
            else if (w == "normal")
                req.weight = 400;
            else
                try {
                    req.weight = std::stoi(w);
                } catch (...) {
                }
        }
        if (std::regex_search(body, m, styleRegex)) {
            std::string s = trim(m[1].str());
            if (s == "italic" || s == "oblique") req.slant = SkFontStyle::kItalic_Slant;
        }
        if (!req.family.empty() && std::regex_search(body, m, urlRegex))
            m_fontFaces[req] = trim(m[1].str());
    }
}
std::string container_skia::get_font_url(const std::string &family, int weight,
                                         SkFontStyle::Slant slant) const {
    std::stringstream ss(family);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string cleanFamily = clean_font_name(trim(item).c_str());
        font_request req = {cleanFamily, weight, slant};
        auto it = m_fontFaces.find(req);
        if (it != m_fontFaces.end()) return it->second;
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
        if (!bestUrl.empty()) return bestUrl;
    }
    return "";
}
void container_skia::set_clip(const litehtml::position &pos,
                              const litehtml::border_radiuses &bdr_radius) {
    if (m_canvas)
        m_canvas->save(), m_canvas->clipPath(SkPathBuilder()
                                                 .addRRect(make_rrect(pos, bdr_radius))
                                                 .moveTo(0, 0)
                                                 .lineTo(0, 0)
                                                 .detach(),
                                             true);
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