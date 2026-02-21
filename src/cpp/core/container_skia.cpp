#include "container_skia.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "bridge/magic_tags.h"
#include "core/text/text_layout.h"
#include "core/text/text_renderer.h"
#include "el_svg.h"
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
#include "include/core/SkSurface.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "litehtml/css_parser.h"
#include "litehtml/el_table.h"
#include "litehtml/el_td.h"
#include "litehtml/el_tr.h"
#include "text_utils.h"
#include "utils/skia_utils.h"
#include "utils/skunicode_satoru.h"

namespace litehtml {
vector<css_token_vector> parse_comma_separated_list(const css_token_vector &tokens);
}

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
}  // namespace

container_skia::container_skia(int w, int h, SkCanvas *canvas, SatoruContext &context,
                               ResourceManager *rm, bool tagging)
    : m_canvas(canvas),
      m_width(w),
      m_height(h),
      m_context(context),
      m_resourceManager(rm),
      m_tagging(tagging),
      m_last_bidi_level(-1),
      m_last_base_level(-1) {
    m_asciiUsed.resize(128, false);
    m_textBatcher = new satoru::TextBatcher(&m_context, m_canvas);
}

container_skia::~container_skia() {
    if (m_textBatcher) {
        m_textBatcher->flush();
        delete m_textBatcher;
    }
}

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

    font_info *fi = new font_info;
    fi->desc = desc;
    fi->fake_bold = fake_bold;

    // Check direction from element's property (if available)
    fi->is_rtl = false;

    for (auto &typeface : typefaces) {
        SkFont *font = m_context.fontManager.createSkFont(typeface, (float)desc.size, desc.weight);
        if (font) {
            fi->fonts.push_back(font);
        }
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
    float ascent = -skfm.fAscent;
    float descent = skfm.fDescent;
    float leading = skfm.fLeading;

    if (fm) {
        float css_line_height = ascent + descent + leading;
        if (css_line_height <= 0) css_line_height = (float)desc.size * 1.2f;

        fm->font_size = (float)desc.size;
        fm->ascent = ascent;
        fm->descent = descent;
        fm->height = css_line_height;
        fm->x_height = skfm.fXHeight;
        fm->ch_width = (litehtml::pixel_t)fi->fonts[0]->measureText("0", 1, SkTextEncoding::kUTF8);
    }
    float css_line_height = ascent + descent + leading;
    if (css_line_height <= 0) css_line_height = (float)desc.size * 1.2f;

    fi->fm_ascent = (int)(ascent + (css_line_height - (ascent + descent)) / 2.0f + 1.0f);
    fi->fm_height = (int)css_line_height;
    return (litehtml::uint_ptr)fi;
}

void container_skia::delete_font(litehtml::uint_ptr hFont) {
    font_info *fi = (font_info *)hFont;
    if (fi) {
        for (auto font : fi->fonts) delete font;
        delete fi;
    }
}

litehtml::pixel_t container_skia::text_width(const char *text, litehtml::uint_ptr hFont,
                                             litehtml::direction dir) {
    font_info *fi = (font_info *)hFont;
    if (fi) {
        fi->is_rtl = (dir == litehtml::direction_rtl);
    }
    return (litehtml::pixel_t)satoru::TextLayout::measureText(
               &m_context, text, fi, -1.0, m_resourceManager ? &m_usedCodepoints : nullptr)
        .width;
}

void container_skia::draw_text(litehtml::uint_ptr hdc, const char *text, litehtml::uint_ptr hFont,
                               litehtml::web_color color, const litehtml::position &pos,
                               litehtml::text_overflow overflow, litehtml::direction dir) {
    if (!m_canvas) return;
    font_info *fi = (font_info *)hFont;
    if (!fi || fi->fonts.empty()) return;

    litehtml::position actual_pos = pos;
    if (overflow == litehtml::text_overflow_ellipsis && !m_clips.empty()) {
        actual_pos.width =
            std::min(pos.width, (litehtml::pixel_t)(m_clips.back().first.right() - pos.x));
    }

    satoru::TextRenderer::drawText(&m_context, m_canvas, text, fi, color, actual_pos, overflow, dir,
                                   m_tagging, get_current_opacity(), m_usedTextShadows,
                                   m_usedTextDraws, m_resourceManager ? &m_usedCodepoints : nullptr,
                                   m_textBatcher);
}

void container_skia::draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector &shadows,
                                     const litehtml::position &pos,
                                     const litehtml::border_radiuses &radius, bool inset) {
    if (!m_canvas) return;
    flush();
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
            info.opacity = get_current_opacity();
            m_usedShadows.push_back(info);
            int index = (int)m_usedShadows.size();
            SkPaint p;
            p.setColor(SkColorSetARGB(255, 0, (uint8_t)satoru::MagicTag::Shadow, (index & 0xFF)));
            m_canvas->drawRRect(make_rrect(pos, radius), p);
        }
        return;
    }
    for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
        const auto &s = *it;
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
    flush();
    if (m_tagging) {
        image_draw_info draw;
        draw.url = url;
        draw.layer = layer;
        draw.opacity = 1.0f;
        if (!m_clips.empty()) {
            draw.has_clip = true;
            draw.clip_pos = m_clips.back().first;
            draw.clip_radius = m_clips.back().second;
        } else {
            draw.has_clip = false;
        }
        m_usedImageDraws.push_back(draw);
        int index = (int)m_usedImageDraws.size();
        SkPaint p;
        p.setColor(
            SkColorSetARGB(255, 1, (uint8_t)satoru::MagicTagExtended::ImageDraw, (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
        auto it = m_context.imageCache.find(url);
        if (it != m_context.imageCache.end() && it->second.skImage) {
            SkPaint p;
            p.setAntiAlias(true);

            m_canvas->save();
            m_canvas->clipRRect(make_rrect(layer.border_box, layer.border_radius), true);

            SkRect dst =
                SkRect::MakeXYWH((float)layer.origin_box.x, (float)layer.origin_box.y,
                                 (float)layer.origin_box.width, (float)layer.origin_box.height);

            if (layer.repeat == litehtml::background_repeat_no_repeat) {
                m_canvas->drawImageRect(it->second.skImage, dst,
                                        SkSamplingOptions(SkFilterMode::kLinear), &p);
            } else {
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
            }
            m_canvas->restore();
        }
    }
}

void container_skia::draw_solid_fill(litehtml::uint_ptr hdc,
                                     const litehtml::background_layer &layer,
                                     const litehtml::web_color &color) {
    if (!m_canvas) return;
    flush();
    SkPaint p;
    p.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    p.setAntiAlias(true);
    m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
}

void container_skia::draw_linear_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::linear_gradient &gradient) {
    if (!m_canvas) return;
    flush();
    if (m_tagging) {
        linear_gradient_info info;
        info.layer = layer;
        info.gradient = gradient;
        info.opacity = 1.0f;
        m_usedLinearGradients.push_back(info);
        int index = (int)m_usedLinearGradients.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, (uint8_t)satoru::MagicTagExtended::LinearGradient,
                                  (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
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
}

void container_skia::draw_radial_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::radial_gradient &gradient) {
    if (!m_canvas) return;
    flush();
    if (m_tagging) {
        radial_gradient_info info;
        info.layer = layer;
        info.gradient = gradient;
        info.opacity = 1.0f;
        m_usedRadialGradients.push_back(info);
        int index = (int)m_usedRadialGradients.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, (uint8_t)satoru::MagicTagExtended::RadialGradient,
                                  (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
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
}

void container_skia::draw_conic_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
    const litehtml::background_layer::conic_gradient &gradient) {
    if (!m_canvas) return;
    flush();
    if (m_tagging) {
        conic_gradient_info info;
        info.layer = layer;
        info.gradient = gradient;
        info.opacity = 1.0f;
        m_usedConicGradients.push_back(info);
        int index = (int)m_usedConicGradients.size();
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, (uint8_t)satoru::MagicTagExtended::ConicGradient,
                                  (index & 0xFF)));
        m_canvas->drawRRect(make_rrect(layer.border_box, layer.border_radius), p);
    } else {
        SkPoint center = SkPoint::Make((float)gradient.position.x, (float)gradient.position.y);
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        for (size_t i = 0; i < gradient.color_points.size(); ++i) {
            const auto &stop = gradient.color_points[i];
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
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

#include "api/satoru_api.h"
#include "bridge/bridge_types.h"

void container_skia::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders &borders,
                                  const litehtml::position &draw_pos, bool root) {
    if (!m_canvas) return;
    flush();

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
            SkRRect outer = rr;
            outer.inset((float)borders.top.width / 6.0f, (float)borders.top.width / 6.0f);
            m_canvas->drawRRect(outer, p);
            SkRRect inner = rr;
            inner.inset((float)borders.top.width * 5.0f / 6.0f,
                        (float)borders.top.width * 5.0f / 6.0f);
            m_canvas->drawRRect(inner, p);
        } else {
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

                SkPath p1 =
                    SkPathBuilder().addRRect(r1).addRRect(r2, SkPathDirection::kCCW).detach();
                SkPath p2 =
                    SkPathBuilder().addRRect(r2).addRRect(r3, SkPathDirection::kCCW).detach();

                p.setColor(c1);
                m_canvas->drawPath(p1, p);
                p.setColor(c2);
                m_canvas->drawPath(p2, p);
            } else {
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

                    SkPath path = SkPathBuilder()
                                      .addRRect(outer_rr)
                                      .addRRect(inner_rr, SkPathDirection::kCCW)
                                      .detach();
                    m_canvas->drawPath(path, p);
                } else
                    m_canvas->drawRRect(outer_rr, p);
            }
            m_canvas->restore();
        };

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

int container_skia::get_bidi_level(const char *text, int base_level) {
    if (m_last_base_level != base_level) {
        m_last_bidi_level = base_level;
        m_last_base_level = base_level;
    }
    return m_context.getUnicodeService().getBidiLevel(text, base_level, &m_last_bidi_level);
}

void container_skia::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker) {
    if (!m_canvas) return;
    flush();

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
                p.setColor(SkColorSetARGB(255, 1, (uint8_t)satoru::MagicTagExtended::ImageDraw,
                                          (index & 0xFF)));
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
    if (m_resourceManager && src && *src)
        m_resourceManager->request(src, src, ResourceType::Image, redraw_on_ready);
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
        flush();
        if (m_tagging) {
            clip_info info;
            info.pos = pos;
            info.radius = bdr_radius;
            m_usedClips.push_back(info);
            int index = (int)m_usedClips.size();

            SkPaint p;
            p.setColor(SkColorSetARGB(255, 0, (uint8_t)satoru::MagicTag::ClipPush, (index & 0xFF)));
            m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
            m_canvas->save();
        } else {
            m_canvas->save();
            m_canvas->clipRRect(make_rrect(pos, bdr_radius), true);
        }
    }
    m_clips.push_back({pos, bdr_radius});
}
void container_skia::del_clip() {
    if (m_canvas) {
        flush();
        m_canvas->restore();
        if (m_tagging) {
            SkPaint p;
            p.setColor(SkColorSetARGB(255, 0, (uint8_t)satoru::MagicTag::ClipPop, 0));
            m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
        }
    }
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

void container_skia::split_text(const char *text, const std::function<void(const char *)> &on_word,
                                const std::function<void(const char *)> &on_space) {
    satoru::TextLayout::splitText(&m_context, text, on_word, on_space);
}

void container_skia::push_layer(litehtml::uint_ptr hdc, float opacity) {
    m_opacity_stack.push_back(opacity);
    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(SkColorSetARGB(255, 0, (uint8_t)satoru::MagicTag::LayerPush,
                                      (uint8_t)(opacity * 255.0f)));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
            m_canvas->save();
        } else if (opacity < 1.0f) {
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
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(SkColorSetARGB(255, 0, (uint8_t)satoru::MagicTag::LayerPop, 0));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        }
        m_canvas->restore();
    }
}

void container_skia::push_transform(litehtml::uint_ptr hdc,
                                    const litehtml::css_token_vector &transform,
                                    const litehtml::css_token_vector &origin,
                                    const litehtml::position &pos) {
    if (!m_canvas) return;
    flush();

    m_canvas->save();

    float ox = pos.x + pos.width * 0.5f;
    float oy = pos.y + pos.height * 0.5f;

    if (!origin.empty()) {
        int n = 0;
        for (const auto &tok : origin) {
            if (tok.type == litehtml::WHITESPACE) continue;
            litehtml::css_length len;
            if (len.from_token(tok, litehtml::f_length_percentage,
                               "left;right;top;bottom;center")) {
                if (n == 0)
                    ox = len.calc_percent(pos.width) + pos.x;
                else if (n == 1)
                    oy = len.calc_percent(pos.height) + pos.y;
                n++;
            }
        }
    }

    m_canvas->translate(ox, oy);

    for (const auto &tok : transform) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            auto args = litehtml::parse_comma_separated_list(tok.value);
            std::vector<float> vals;
            for (const auto &arg : args) {
                if (!arg.empty() &&
                    (arg[0].type == litehtml::NUMBER || arg[0].type == litehtml::DIMENSION ||
                     arg[0].type == litehtml::PERCENTAGE)) {
                    float v = arg[0].n.number;
                    if (arg[0].type == litehtml::PERCENTAGE) {
                        if (name.find("translate") != std::string::npos) {
                            v = v * (vals.empty() ? pos.width : pos.height) / 100.0f;
                        } else {
                            v /= 100.0f;
                        }
                    }
                    vals.push_back(v);
                }
            }

            if (name == "matrix" && vals.size() >= 6) {
                SkMatrix m;
                m.setAll(vals[0], vals[2], vals[4], vals[1], vals[3], vals[5], 0, 0, 1);
                m_canvas->concat(m);
            } else if (name == "translate" || name == "translate3d") {
                m_canvas->translate(vals.size() > 0 ? vals[0] : 0, vals.size() > 1 ? vals[1] : 0);
            } else if (name == "translatex") {
                m_canvas->translate(vals.size() > 0 ? vals[0] : 0, 0);
            } else if (name == "translatey") {
                m_canvas->translate(0, vals.size() > 0 ? vals[0] : 0);
            } else if (name == "scale" || name == "scale3d") {
                float sx = vals.size() > 0 ? vals[0] : 1;
                float sy = vals.size() > 1 ? vals[1] : sx;
                m_canvas->scale(sx, sy);
            } else if (name == "scalex") {
                m_canvas->scale(vals.size() > 0 ? vals[0] : 1, 1);
            } else if (name == "scaley") {
                m_canvas->scale(1, vals.size() > 0 ? vals[0] : 1);
            } else if (name == "rotate" || name == "rotatez") {
                if (!vals.empty()) m_canvas->rotate(vals[0]);
            } else if (name == "skew" && !vals.empty()) {
                float kx = vals[0];
                float ky = vals.size() > 1 ? vals[1] : 0;
                m_canvas->skew(tanf(kx * 3.14159f / 180.0f), tanf(ky * 3.14159f / 180.0f));
            } else if (name == "skewx" && !vals.empty()) {
                m_canvas->skew(tanf(vals[0] * 3.14159f / 180.0f), 0);
            } else if (name == "skewy" && !vals.empty()) {
                m_canvas->skew(0, tanf(vals[0] * 3.14159f / 180.0f));
            }
        }
    }

    m_canvas->translate(-ox, -oy);
}

void container_skia::pop_transform(litehtml::uint_ptr hdc) {
    if (m_canvas) {
        flush();
        m_canvas->restore();
    }
}

void container_skia::push_filter(litehtml::uint_ptr hdc, const litehtml::css_token_vector &filter) {
    if (!m_canvas || filter.empty()) return;
    flush();

    if (m_tagging) {
        filter_info info;
        info.tokens = filter;
        info.opacity = get_current_opacity();
        m_usedFilters.push_back(info);
        int index = (int)m_usedFilters.size();

        SkPaint p;
        p.setColor(SkColorSetARGB(255, 0, (uint8_t)satoru::MagicTag::FilterPush, (index & 0xFF)));

        SkRect rect;
        if (!m_clips.empty()) {
            rect = SkRect::MakeXYWH((float)m_clips.back().first.x, (float)m_clips.back().first.y,
                                    (float)m_clips.back().first.width,
                                    (float)m_clips.back().first.height);
        } else {
            rect = SkRect::MakeWH((float)m_width, (float)m_height);
        }

        m_canvas->drawRect(rect, p);
        m_canvas->save();
        return;
    }

    sk_sp<SkImageFilter> last_filter = nullptr;

    for (const auto &tok : filter) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            auto args = litehtml::parse_comma_separated_list(tok.value);

            if (name == "blur") {
                if (!args.empty() && !args[0].empty()) {
                    litehtml::css_length len;
                    len.from_token(args[0][0], litehtml::f_length | litehtml::f_positive);
                    float sigma = len.val();
                    if (sigma > 0) {
                        last_filter = SkImageFilters::Blur(sigma, sigma, last_filter);
                    }
                }
            } else if (name == "drop-shadow") {
                if (!args.empty()) {
                    float dx = 0, dy = 0, blur = 0;
                    litehtml::web_color color = litehtml::web_color::black;
                    int i = 0;
                    for (const auto &t : args[0]) {
                        if (t.type == litehtml::WHITESPACE) continue;
                        if (i == 0) {
                            litehtml::css_length l;
                            l.from_token(t, litehtml::f_length);
                            dx = l.val();
                        } else if (i == 1) {
                            litehtml::css_length l;
                            l.from_token(t, litehtml::f_length);
                            dy = l.val();
                        } else if (i == 2) {
                            litehtml::css_length l;
                            l.from_token(t, litehtml::f_length | litehtml::f_positive);
                            blur = l.val();
                        } else if (i == 3) {
                            litehtml::parse_color(t, color, nullptr);
                        }
                        i++;
                    }
                    if (dx != 0 || dy != 0 || blur > 0) {
                        last_filter = SkImageFilters::DropShadow(
                            dx, dy, blur * 0.5f, blur * 0.5f,
                            SkColorSetARGB(color.alpha, color.red, color.green, color.blue),
                            last_filter);
                    }
                }
            }
        }
    }

    if (last_filter) {
        SkPaint paint;
        paint.setImageFilter(last_filter);
        m_canvas->saveLayer(nullptr, &paint);
    } else {
        m_canvas->save();
    }
}

void container_skia::pop_filter(litehtml::uint_ptr hdc) {
    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(SkColorSetARGB(255, 0, (uint8_t)satoru::MagicTag::FilterPop, 0));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        }
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
