#include "container_skia.h"

#include <algorithm>
#include <iostream>
#include <sstream>

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
#include "include/core/SkTileMode.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"
#include "el_svg.h"
#include "litehtml/el_table.h"
#include "litehtml/el_td.h"
#include "litehtml/el_tr.h"
#include "utils/skia_utils.h"

namespace {
static SkColor darken(litehtml::web_color c, float fraction) {
    return SkColorSetARGB(c.alpha,
                          (uint8_t)std::max(0.0f, (float)c.red - ((float)c.red * fraction)),
                          (uint8_t)std::max(0.0f, (float)c.green - ((float)c.green * fraction)),
                          (uint8_t)std::max(0.0f, (float)c.blue - ((float)c.blue * fraction)));
}

static SkColor lighten(litehtml::web_color c, float fraction) {
    return SkColorSetARGB(
        c.alpha, (uint8_t)std::min(255.0f, (float)c.red + ((255.0f - (float)c.red) * fraction)),
        (uint8_t)std::min(255.0f, (float)c.green + ((255.0f - (float)c.green) * fraction)),
        (uint8_t)std::min(255.0f, (float)c.blue + ((255.0f - (float)c.blue) * fraction)));
}

std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n'\"");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n'\"");
    return s.substr(start, end - start + 1);
}

char32_t decode_utf8(const char **ptr) {
    const uint8_t *p = (const uint8_t *)*ptr;
    if (!p || !*p) return 0;

    uint32_t cp = 0;
    uint8_t b1 = *p++;

    if (b1 < 0x80) {
        cp = b1;
    } else if ((b1 & 0xE0) == 0xC0) {
        cp = (b1 & 0x1F) << 6;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F);
    } else if ((b1 & 0xF0) == 0xE0) {
        cp = (b1 & 0x0F) << 12;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F);
    } else if ((b1 & 0xF8) == 0xF0) {
        cp = (b1 & 0x07) << 18;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F) << 12;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) == 0x80) cp |= (*p++ & 0x3F);
    }

    *ptr = (const char *)p;
    return (char32_t)cp;
}
}  // namespace

container_skia::container_skia(int w, int h, SkCanvas *canvas, SatoruContext &context,
                               ResourceManager *rm, bool tagging)
    : m_canvas(canvas),
      m_width(w),
      m_height(h),
      m_context(context),
      m_resourceManager(rm),
      m_tagging(tagging) {}

litehtml::uint_ptr container_skia::create_font(const litehtml::font_description &desc,
                                               const litehtml::document *doc,
                                               litehtml::font_metrics *fm) {
    SkFontStyle::Slant slant = desc.style == litehtml::font_style_normal
                                   ? SkFontStyle::kUpright_Slant
                                   : SkFontStyle::kItalic_Slant;

    std::vector<sk_sp<SkTypeface>> typefaces;
    bool fake_bold = false;

    std::stringstream ss(desc.family);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string family = trim(item);
        if (family.empty()) continue;

        bool fb = false;
        auto tfs = m_context.get_typefaces(family, desc.weight, slant, fb);
        for (auto &tf : tfs) {
            typefaces.push_back(tf);
        }
        if (fb) fake_bold = true;

        if (m_resourceManager) {
            font_request req;
            req.family = family;
            req.weight = desc.weight;
            req.slant = slant;
            m_requestedFontAttributes.insert(req);
        }
    }

    if (typefaces.empty()) {
        m_missingFonts.insert({desc.family, desc.weight, slant});
        typefaces = m_context.get_typefaces("sans-serif", desc.weight, slant, fake_bold);
    }

    // Add global fallbacks (e.g. Noto Sans JP)
    for (auto &tf : m_context.fontManager.getFallbackTypefaces()) {
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

    for (auto &typeface : typefaces) {
        SkFont *font = m_context.fontManager.createSkFont(typeface, (float)desc.size, desc.weight);
        if (font) {
            fi->fonts.push_back(font);
        }
    }

    if (fi->fonts.size() > 1) {
        // printf("[WASM] create_font: Family '%s' has %zu subset fonts loaded.\n",
        // desc.family.c_str(), fi->fonts.size());
    }

    if (fi->fonts.empty()) {
        sk_sp<SkTypeface> def = m_context.fontManager.getDefaultTypeface();
        if (def) {
            fi->fonts.push_back(
                m_context.fontManager.createSkFont(def, (float)desc.size, desc.weight));
        } else {
            fi->fonts.push_back(new SkFont(SkTypeface::MakeEmpty(), (float)desc.size));
        }
    }

    if (!fi->fonts.empty()) {
        int actual_weight = fi->fonts[0]->getTypeface()->fontStyle().weight();
        if (actual_weight >= desc.weight) {
            fi->fake_bold = false;
        }
    }

    SkFontMetrics skfm;
    fi->fonts[0]->getMetrics(&skfm);
    if (fm) {
        fm->font_size = (float)desc.size;
        fm->ascent = -skfm.fAscent;
        fm->descent = skfm.fDescent;
        fm->height = fm->ascent + fm->descent + skfm.fLeading;
        fm->x_height = skfm.fXHeight;
        fm->ch_width = (litehtml::pixel_t)fi->fonts[0]->measureText("0", 1, SkTextEncoding::kUTF8);
    }
    fi->fm_ascent = (int)-skfm.fAscent;
    fi->fm_height = fm->height;
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
        if (m_resourceManager) m_usedCodepoints.insert(u);

        SkFont *font = nullptr;
        for (auto f : fi->fonts) {
            SkGlyphID glyph = 0;
            SkUnichar sc = (SkUnichar)u;
            f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>(&sc, 1),
                                               SkSpan<SkGlyphID>(&glyph, 1));
            if (glyph != 0) {
                font = f;
                break;
            }
        }
        if (!font) {
            font = fi->fonts[0];
        }
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
                               litehtml::web_color color, const litehtml::position &pos,
                               litehtml::text_overflow overflow) {
    if (!m_canvas) return;
    font_info *fi = (font_info *)hFont;
    if (!fi || fi->fonts.empty()) return;

    std::string text_str = text;
    if (overflow == litehtml::text_overflow_ellipsis) {
        litehtml::pixel_t available_width = pos.width;
        if (!m_clips.empty()) {
            available_width = std::min(available_width,
                                       (litehtml::pixel_t)(m_clips.back().first.right() - pos.x));
        }

        litehtml::pixel_t full_width = text_width(text, hFont);
        if (full_width > available_width) {
            std::string ellipsis = "...";
            litehtml::pixel_t ellipsis_width = text_width(ellipsis.c_str(), hFont);
            if (ellipsis_width >= available_width) {
                text_str = ellipsis;
            } else {
                double max_w = (double)available_width - ellipsis_width;
                double current_w = 0;
                const char *p = text;
                const char *run_start = p;
                const char *last_safe = p;
                SkFont *current_font = nullptr;

                while (*p) {
                    const char *next_p = p;
                    char32_t u = decode_utf8(&next_p);
                    SkFont *font = nullptr;
                    for (auto f : fi->fonts) {
                        SkGlyphID glyph = 0;
                        SkUnichar sc = (SkUnichar)u;
                        f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>(&sc, 1),
                                                           SkSpan<SkGlyphID>(&glyph, 1));
                        if (glyph != 0) {
                            font = f;
                            break;
                        }
                    }
                    if (!font) font = fi->fonts[0];
                    if (font != current_font) {
                        if (current_font && p > run_start) {
                            double run_w = current_font->measureText(run_start, p - run_start,
                                                                     SkTextEncoding::kUTF8);
                            if (current_w + run_w > max_w) {
                                const char *rp = run_start;
                                while (rp < p) {
                                    const char *next_rp = rp;
                                    decode_utf8(&next_rp);
                                    double char_w = current_font->measureText(
                                        rp, next_rp - rp, SkTextEncoding::kUTF8);
                                    if (current_w + char_w > max_w) goto found_split;
                                    current_w += char_w;
                                    last_safe = next_rp;
                                    rp = next_rp;
                                }
                            } else {
                                current_w += run_w;
                                last_safe = p;
                            }
                        }
                        run_start = p;
                        current_font = font;
                    }
                    p = next_p;
                }
                if (current_font && p > run_start) {
                    double run_w =
                        current_font->measureText(run_start, p - run_start, SkTextEncoding::kUTF8);
                    if (current_w + run_w > max_w) {
                        const char *rp = run_start;
                        while (rp < p) {
                            const char *next_rp = rp;
                            decode_utf8(&next_rp);
                            double char_w =
                                current_font->measureText(rp, next_rp - rp, SkTextEncoding::kUTF8);
                            if (current_w + char_w > max_w) goto found_split;
                            current_w += char_w;
                            last_safe = next_rp;
                            rp = next_rp;
                        }
                    } else {
                        last_safe = p;
                    }
                }

            found_split:
                text_str = std::string(text, last_safe - text) + ellipsis;
            }
        }
    }

    SkPaint paint;
    paint.setAntiAlias(true);

    if (m_tagging && !fi->desc.text_shadow.empty()) {
        text_shadow_info info;
        info.shadows = fi->desc.text_shadow;
        info.text_color = color;
        m_usedTextShadows.push_back(info);
        int index = (int)m_usedTextShadows.size();
        paint.setColor(SkColorSetARGB(255, 0, 2, (index & 0xFF)));
    } else {
        paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
        if (m_tagging) {
            paint.setAlphaf(paint.getAlphaf() * get_current_opacity());
        }
    }

    auto draw_text_internal = [&](const char *str, size_t len, double x, double y,
                                  const SkPaint &p) {
        double x_offset = 0;
        const char *ptr = str;
        const char *run_start = ptr;
        SkFont *current_font = nullptr;
        const char *end = str + len;
        while (ptr < end) {
            const char *next_ptr = ptr;
            char32_t u = decode_utf8(&next_ptr);
            if (m_resourceManager) m_usedCodepoints.insert(u);

            SkFont *font = nullptr;
            for (auto f : fi->fonts) {
                SkGlyphID glyph = 0;
                SkUnichar sc = (SkUnichar)u;
                f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>(&sc, 1),
                                                   SkSpan<SkGlyphID>(&glyph, 1));
                if (glyph != 0) {
                    font = f;
                    break;
                }
            }
            if (!font) {
                font = fi->fonts[0];
            }
            if (font != current_font) {
                if (current_font && ptr > run_start) {
                    SkFont render_font = *current_font;
                    if (fi->fake_bold) render_font.setEmbolden(true);
                    m_canvas->drawSimpleText(run_start, ptr - run_start, SkTextEncoding::kUTF8,
                                             (float)x + (float)x_offset,
                                             (float)y + (float)fi->fm_ascent, render_font, p);
                    x_offset += current_font->measureText(run_start, ptr - run_start,
                                                          SkTextEncoding::kUTF8);
                }
                run_start = ptr;
                current_font = font;
            }
            ptr = next_ptr;
        }
        if (current_font && ptr > run_start) {
            SkFont render_font = *current_font;
            if (fi->fake_bold) render_font.setEmbolden(true);
            m_canvas->drawSimpleText(run_start, ptr - run_start, SkTextEncoding::kUTF8,
                                     (float)x + (float)x_offset, (float)y + (float)fi->fm_ascent,
                                     render_font, p);
            x_offset +=
                current_font->measureText(run_start, ptr - run_start, SkTextEncoding::kUTF8);
        }
        return x_offset;
    };

    if (!m_tagging && !fi->desc.text_shadow.empty()) {
        for (auto it = fi->desc.text_shadow.rbegin(); it != fi->desc.text_shadow.rend(); ++it) {
            const auto &s = *it;
            SkPaint shadow_paint = paint;
            shadow_paint.setColor(
                SkColorSetARGB(s.color.alpha, s.color.red, s.color.green, s.color.blue));
            float blur_std_dev = (float)s.blur.val() * 0.5f;
            if (blur_std_dev > 0)
                shadow_paint.setMaskFilter(
                    SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_std_dev));

            draw_text_internal(text_str.c_str(), text_str.size(), (double)pos.x + s.x.val(),
                               (double)pos.y + s.y.val(), shadow_paint);
        }
    }

    double final_x_offset =
        draw_text_internal(text_str.c_str(), text_str.size(), (double)pos.x, (double)pos.y, paint);

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none) {
        float x_offset_dec = (float)final_x_offset;
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
                m_canvas->drawLine((float)pos.x, y - gap / 2, (float)pos.x + x_offset_dec,
                                   y - gap / 2, dec_paint);
                m_canvas->drawLine((float)pos.x, y + gap / 2, (float)pos.x + x_offset_dec,
                                   y + gap / 2, dec_paint);
            } else {
                m_canvas->drawLine((float)pos.x, y, (float)pos.x + x_offset_dec, y, dec_paint);
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
        for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
            const auto &s = *it;
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
    for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
        const auto &s = *it;
        if (s.inset != inset) continue;
        SkRRect box_rrect = make_rrect(pos, radius);
        float current_opacity = get_current_opacity();
        SkColor shadow_color = SkColorSetARGB((uint8_t)(s.color.alpha * current_opacity),
                                              s.color.red, s.color.green, s.color.blue);
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
        draw.opacity = get_current_opacity();
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
            // PNG mode uses saveLayer, so we don't need to apply opacity to p here
            // unless we want to support non-stacking context opacity (rare in satoru).
            m_canvas->save();
            m_canvas->clipRRect(make_rrect(layer.border_box, layer.border_radius), true);
            SkTileMode tileX = SkTileMode::kRepeat;
            SkTileMode tileY = SkTileMode::kRepeat;

            switch (layer.repeat) {
                case litehtml::background_repeat_repeat:
                    tileX = SkTileMode::kRepeat;
                    tileY = SkTileMode::kRepeat;
                    break;
                case litehtml::background_repeat_repeat_x:
                    tileX = SkTileMode::kRepeat;
                    tileY = SkTileMode::kDecal;
                    break;
                case litehtml::background_repeat_repeat_y:
                    tileX = SkTileMode::kDecal;
                    tileY = SkTileMode::kRepeat;
                    break;
                case litehtml::background_repeat_no_repeat:
                    tileX = SkTileMode::kDecal;
                    tileY = SkTileMode::kDecal;
                    break;
            }

            float scaleX = (float)layer.origin_box.width / it->second.skImage->width();
            float scaleY = (float)layer.origin_box.height / it->second.skImage->height();

            SkMatrix matrix;
            matrix.setScaleTranslate(scaleX, scaleY, (float)layer.origin_box.x,
                                     (float)layer.origin_box.y);

            p.setShader(it->second.skImage->makeShader(
                tileX, tileY, SkSamplingOptions(SkFilterMode::kLinear), &matrix));

            m_canvas->drawRect(
                SkRect::MakeXYWH((float)layer.clip_box.x, (float)layer.clip_box.y,
                                 (float)layer.clip_box.width, (float)layer.clip_box.height),
                p);
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
    if (m_tagging) {
        p.setAlphaf(p.getAlphaf() * get_current_opacity());
    }
    p.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
}

void container_skia::draw_linear_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::linear_gradient &gradient) {
    if (!m_canvas) return;
    if (m_tagging) {
        linear_gradient_info info;
        info.layer = layer;
        info.gradient = gradient;
        info.opacity = get_current_opacity();
        m_usedLinearGradients.push_back(info);
        int index = (int)m_usedLinearGradients.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, 3, (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
        SkPoint pts[2] = {SkPoint::Make((float)gradient.start.x, (float)gradient.start.y),
                          SkPoint::Make((float)gradient.end.x, (float)gradient.end.y)};
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        float current_opacity = get_current_opacity();
        for (const auto &stop : gradient.color_points) {
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f,
                              (stop.color.alpha / 255.0f) * current_opacity});
            pos.push_back(stop.offset);
        }
        SkGradient grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                        SkGradient::Interpolation());
        SkPaint p;
        p.setShader(SkShaders::LinearGradient(pts, grad));
        p.setAntiAlias(true);
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    }
}

void container_skia::draw_radial_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::radial_gradient &gradient) {
    if (!m_canvas) return;
    if (m_tagging) {
        radial_gradient_info info;
        info.layer = layer;
        info.gradient = gradient;
        info.opacity = get_current_opacity();
        m_usedRadialGradients.push_back(info);
        int index = (int)m_usedRadialGradients.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, 2, (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
        SkPoint center = SkPoint::Make((float)gradient.position.x, (float)gradient.position.y);
        float rx = (float)gradient.radius.x, ry = (float)gradient.radius.y;
        if (rx <= 0 || ry <= 0) return;
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        float current_opacity = get_current_opacity();
        for (const auto &stop : gradient.color_points) {
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f,
                              (stop.color.alpha / 255.0f) * current_opacity});
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
}

void container_skia::draw_conic_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::conic_gradient &gradient) {
    if (!m_canvas) return;
    if (m_tagging) {
        conic_gradient_info info;
        info.layer = layer;
        info.gradient = gradient;
        info.opacity = get_current_opacity();
        m_usedConicGradients.push_back(info);
        int index = (int)m_usedConicGradients.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, 1, (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
        SkPoint center = SkPoint::Make((float)gradient.position.x, (float)gradient.position.y);
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        float current_opacity = get_current_opacity();
        for (size_t i = 0; i < gradient.color_points.size(); ++i) {
            const auto &stop = gradient.color_points[i];
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f,
                              (stop.color.alpha / 255.0f) * current_opacity});
            float offset = stop.offset;
            if (i > 0 && offset <= pos.back()) {
                offset = pos.back() + 0.00001f;
            }
            pos.push_back(offset);
        }
        if (!pos.empty() && pos.back() > 1.0f) {
            float max_val = pos.back();
            for (auto &p : pos) p /= max_val;
            pos.back() = 1.0f;
        }
        SkGradient grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                        SkGradient::Interpolation());
        SkMatrix matrix;
        matrix.setRotate(gradient.angle - 90.0f, center.x(), center.y());
        SkPaint p;
        p.setShader(SkShaders::SweepGradient(center, grad, &matrix));
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
        borders.top.color == borders.left.color && borders.top.color == borders.right.color &&
        borders.top.style == borders.bottom.style && borders.top.style == borders.left.style &&
        borders.top.style == borders.right.style &&
        borders.top.style != litehtml::border_style_groove &&
        borders.top.style != litehtml::border_style_ridge &&
        borders.top.style != litehtml::border_style_inset &&
        borders.top.style != litehtml::border_style_outset;

    if (uniform && borders.top.width > 0) {
        if (borders.top.style == litehtml::border_style_none ||
            borders.top.style == litehtml::border_style_hidden)
            return;

        SkPaint p;
        p.setColor(SkColorSetARGB(borders.top.color.alpha, borders.top.color.red,
                                  borders.top.color.green, borders.top.color.blue));
        if (m_tagging) {
            p.setAlphaf(p.getAlphaf() * get_current_opacity());
        }
        p.setAntiAlias(true);

        SkRRect rr = make_rrect(draw_pos, borders.radius);

        if (borders.top.style == litehtml::border_style_dotted ||
            borders.top.style == litehtml::border_style_dashed) {
            p.setStrokeWidth((float)borders.top.width);
            p.setStyle(SkPaint::kStroke_Style);
            float intervals[2];
            if (borders.top.style == litehtml::border_style_dotted) {
                intervals[0] = 0.0f;
                intervals[1] = 2.0f * (float)borders.top.width;
                p.setStrokeCap(SkPaint::kRound_Cap);
            } else {
                intervals[0] = std::max(3.0f, 2.0f * (float)borders.top.width);
                intervals[1] = (float)borders.top.width;
            }
            p.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
            rr.inset((float)borders.top.width / 2.0f, (float)borders.top.width / 2.0f);
            m_canvas->drawRRect(rr, p);
        } else if (borders.top.style == litehtml::border_style_double) {
            p.setStrokeWidth((float)borders.top.width / 3.0f);
            p.setStyle(SkPaint::kStroke_Style);
            // Outer ring
            SkRRect outer = rr;
            outer.inset((float)borders.top.width / 6.0f, (float)borders.top.width / 6.0f);
            m_canvas->drawRRect(outer, p);
            // Inner ring
            SkRRect inner = rr;
            inner.inset((float)borders.top.width * 5.0f / 6.0f,
                        (float)borders.top.width * 5.0f / 6.0f);
            m_canvas->drawRRect(inner, p);
        } else {
            // solid
            p.setStrokeWidth((float)borders.top.width);
            p.setStyle(SkPaint::kStroke_Style);
            rr.inset((float)borders.top.width / 2.0f, (float)borders.top.width / 2.0f);
            m_canvas->drawRRect(rr, p);
        }
    } else {
        float x = (float)draw_pos.x, y = (float)draw_pos.y, w = (float)draw_pos.width,
              h = (float)draw_pos.height, lw = (float)borders.left.width,
              tw = (float)borders.top.width, rw = (float)borders.right.width,
              bw = (float)borders.bottom.width;

        if (lw <= 0 && tw <= 0 && rw <= 0 && bw <= 0) return;

        m_canvas->save();
        SkRRect outer_rr = make_rrect(draw_pos, borders.radius);
        SkPoint center = {x + w / 2.0f, y + h / 2.0f};

        auto draw_side = [&](const litehtml::border &b, const SkPath &quadrant, bool is_top_left,
                             float sw) {
            if (b.width <= 0 || b.style == litehtml::border_style_none ||
                b.style == litehtml::border_style_hidden)
                return;

            m_canvas->save();
            m_canvas->clipPath(quadrant, true);

            SkPaint p;
            p.setAntiAlias(true);
            p.setColor(SkColorSetARGB(b.color.alpha, b.color.red, b.color.green, b.color.blue));
            if (m_tagging) {
                p.setAlphaf(p.getAlphaf() * get_current_opacity());
            }

            if (b.style == litehtml::border_style_dotted ||
                b.style == litehtml::border_style_dashed) {
                p.setStyle(SkPaint::kStroke_Style);
                p.setStrokeWidth((float)b.width);
                float intervals[2];
                if (b.style == litehtml::border_style_dotted) {
                    intervals[0] = 0.0f;
                    intervals[1] = 2.0f * (float)b.width;
                    p.setStrokeCap(SkPaint::kRound_Cap);
                } else {
                    intervals[0] = std::max(3.0f, 2.0f * (float)b.width);
                    intervals[1] = (float)b.width;
                }
                p.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
                SkRRect stroke_rr = outer_rr;
                stroke_rr.inset(sw / 2.0f, sw / 2.0f);
                m_canvas->drawRRect(stroke_rr, p);
            } else if (b.style == litehtml::border_style_double) {
                p.setStyle(SkPaint::kStroke_Style);
                p.setStrokeWidth(sw / 3.0f);
                SkRRect r1 = outer_rr;
                r1.inset(sw / 6.0f, sw / 6.0f);
                m_canvas->drawRRect(r1, p);
                SkRRect r2 = outer_rr;
                r2.inset(sw * 5.0f / 6.0f, sw * 5.0f / 6.0f);
                m_canvas->drawRRect(r2, p);
            } else if (b.style == litehtml::border_style_groove ||
                       b.style == litehtml::border_style_ridge) {
                bool ridge = (b.style == litehtml::border_style_ridge);
                SkColor c1, c2;
                if (is_top_left) {
                    c1 = ridge ? lighten(b.color, 0.2f) : darken(b.color, 0.2f);
                    c2 = ridge ? darken(b.color, 0.2f) : lighten(b.color, 0.2f);
                } else {
                    c1 = ridge ? darken(b.color, 0.2f) : lighten(b.color, 0.2f);
                    c2 = ridge ? lighten(b.color, 0.2f) : darken(b.color, 0.2f);
                }
                SkRRect r1 = outer_rr;
                SkRRect r2 = outer_rr;
                r2.inset(sw / 2.0f, sw / 2.0f);
                SkRRect r3 = outer_rr;
                r3.inset(sw, sw);
                p.setColor(c1);
                m_canvas->drawDRRect(r1, r2, p);
                p.setColor(c2);
                m_canvas->drawDRRect(r2, r3, p);
            } else {
                // solid, inset, outset
                if (b.style == litehtml::border_style_inset ||
                    b.style == litehtml::border_style_outset) {
                    bool outset = (b.style == litehtml::border_style_outset);
                    if (is_top_left) {
                        p.setColor(outset ? lighten(b.color, 0.2f) : darken(b.color, 0.2f));
                    } else {
                        p.setColor(outset ? darken(b.color, 0.2f) : lighten(b.color, 0.2f));
                    }
                }
                SkRect ir = SkRect::MakeXYWH(x + lw, y + tw, w - lw - rw, h - tw - bw);
                SkRRect inner_rr;
                if (ir.width() > 0 && ir.height() > 0) {
                    SkVector rads[4] = {{std::max(0.0f, (float)borders.radius.top_left_x - lw),
                                         std::max(0.0f, (float)borders.radius.top_left_y - tw)},
                                        {std::max(0.0f, (float)borders.radius.top_right_x - rw),
                                         std::max(0.0f, (float)borders.radius.top_right_y - tw)},
                                        {std::max(0.0f, (float)borders.radius.bottom_right_x - rw),
                                         std::max(0.0f, (float)borders.radius.bottom_right_y - bw)},
                                        {std::max(0.0f, (float)borders.radius.bottom_left_x - lw),
                                         std::max(0.0f, (float)borders.radius.bottom_left_y - bw)}};
                    inner_rr.setRectRadii(ir, rads);
                    m_canvas->drawDRRect(outer_rr, inner_rr, p);
                } else
                    m_canvas->drawRRect(outer_rr, p);
            }
            m_canvas->restore();
        };

        // Top
        draw_side(borders.top,
                  SkPathBuilder()
                      .moveTo(x, y)
                      .lineTo(x + w, y)
                      .lineTo(x + w - rw, y + tw)
                      .lineTo(center.fX, center.fY)
                      .lineTo(x + lw, y + tw)
                      .close()
                      .detach(),
                  true, tw);
        // Bottom
        draw_side(borders.bottom,
                  SkPathBuilder()
                      .moveTo(x, y + h)
                      .lineTo(x + w, y + h)
                      .lineTo(x + w - rw, y + h - bw)
                      .lineTo(center.fX, center.fY)
                      .lineTo(x + lw, y + h - bw)
                      .close()
                      .detach(),
                  false, bw);
        // Left
        draw_side(borders.left,
                  SkPathBuilder()
                      .moveTo(x, y)
                      .lineTo(x + lw, y + tw)
                      .lineTo(center.fX, center.fY)
                      .lineTo(x + lw, y + h - bw)
                      .lineTo(x, y + h)
                      .close()
                      .detach(),
                  true, lw);
        // Right
        draw_side(borders.right,
                  SkPathBuilder()
                      .moveTo(x + w, y)
                      .lineTo(x + w - rw, y + tw)
                      .lineTo(center.fX, center.fY)
                      .lineTo(x + w - rw, y + h - bw)
                      .lineTo(x + w, y + h)
                      .close()
                      .detach(),
                  false, rw);

        m_canvas->restore();
    }
}

litehtml::pixel_t container_skia::pt_to_px(float pt) const { return pt * 96.0f / 72.0f; }
litehtml::pixel_t container_skia::get_default_font_size() const { return 16.0f; }
const char *container_skia::get_default_font_name() const { return "sans-serif"; }

void container_skia::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker) {
    if (!m_canvas) return;

    if (!marker.image.empty()) {
        std::string url = marker.image;
        auto it = m_context.imageCache.find(url);
        if (it != m_context.imageCache.end() && it->second.skImage) {
            SkRect dst = SkRect::MakeXYWH((float)marker.pos.x, (float)marker.pos.y,
                                          (float)marker.pos.width, (float)marker.pos.height);
            SkPaint p;
            p.setAntiAlias(true);
            if (m_tagging) {
                image_draw_info draw;
                draw.url = url;
                litehtml::background_layer layer;
                layer.border_box = marker.pos;
                layer.clip_box = marker.pos;
                layer.origin_box = marker.pos;
                draw.layer = layer;
                draw.opacity = get_current_opacity();
                m_usedImageDraws.push_back(draw);
                int index = (int)m_usedImageDraws.size();
                p.setColor(SkColorSetARGB(255, 1, 0, (index & 0xFF)));
                m_canvas->drawRect(dst, p);
            } else {
                m_canvas->drawImageRect(it->second.skImage, dst,
                                        SkSamplingOptions(SkFilterMode::kLinear), &p);
            }
        }
        return;
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    litehtml::web_color color = marker.color;
    paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    if (m_tagging) {
        paint.setAlphaf(paint.getAlphaf() * get_current_opacity());
    }

    SkRect rect = SkRect::MakeXYWH((float)marker.pos.x, (float)marker.pos.y,
                                   (float)marker.pos.width, (float)marker.pos.height);

    switch (marker.marker_type) {
        case litehtml::list_style_type_circle: {
            paint.setStyle(SkPaint::kStroke_Style);
            float strokeWidth = std::max(1.0f, (float)marker.pos.width * 0.1f);
            paint.setStrokeWidth(strokeWidth);
            rect.inset(strokeWidth / 2.0f, strokeWidth / 2.0f);
            m_canvas->drawOval(rect, paint);
            break;
        }
        case litehtml::list_style_type_disc: {
            paint.setStyle(SkPaint::kFill_Style);
            m_canvas->drawOval(rect, paint);
            break;
        }
        case litehtml::list_style_type_square: {
            paint.setStyle(SkPaint::kFill_Style);
            m_canvas->drawRect(rect, paint);
            break;
        }
        default:
            break;
    }
}

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
    if (!url.empty() && m_resourceManager) {
        std::string lowerUrl = url;
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
        if (lowerUrl.find(".woff2") != std::string::npos ||
            lowerUrl.find(".woff") != std::string::npos ||
            lowerUrl.find(".ttf") != std::string::npos ||
            lowerUrl.find(".otf") != std::string::npos ||
            lowerUrl.find(".ttc") != std::string::npos) {
            m_resourceManager->request(url, "", ResourceType::Font);
        } else {
            m_resourceManager->request(url, url, ResourceType::Css);
        }
    } else {
        m_context.fontManager.scanFontFaces(text);
    }
}

void container_skia::set_clip(const litehtml::position &pos,
                              const litehtml::border_radiuses &bdr_radius) {
    if (m_canvas) {
        m_canvas->save();
        m_canvas->clipRRect(make_rrect(pos, bdr_radius), true);
    }
    m_clips.push_back({pos, bdr_radius});
}
void container_skia::del_clip() {
    if (m_canvas) m_canvas->restore();
    if (!m_clips.empty()) m_clips.pop_back();
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

void container_skia::push_layer(litehtml::uint_ptr hdc, float opacity) {
    m_opacity_stack.push_back(opacity);
    if (m_canvas) {
        if (opacity < 1.0f && !m_tagging) {
            SkPaint paint;
            paint.setAlphaf(opacity);
            m_canvas->saveLayer(nullptr, &paint);
        } else {
            m_canvas->save();
        }
    }
}

void container_skia::pop_layer(litehtml::uint_ptr hdc) {
    if (!m_opacity_stack.empty()) {
        m_opacity_stack.pop_back();
    }
    if (m_canvas) {
        m_canvas->restore();
    }
}

litehtml::element::ptr container_skia::create_element(
    const char *tag_name, const litehtml::string_map &attributes,
    const std::shared_ptr<litehtml::document> &doc) {
    std::string tag = tag_name;
    if (tag == "table") {
        return std::make_shared<litehtml::el_table>(doc);
    }
    if (tag == "tr") {
        return std::make_shared<litehtml::el_tr>(doc);
    }
    if (tag == "td" || tag == "th") {
        return std::make_shared<litehtml::el_td>(doc);
    }
    if (tag == "svg") {
        return std::make_shared<litehtml::el_svg>(doc);
    }
    return nullptr;
}
