#include "container_skia.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "bridge/magic_tags.h"
#include "core/text/text_layout.h"
#include "core/text/text_renderer.h"
#include "el_svg.h"
#include "include/core/SkBitmap.h"
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
#include "include/effects/SkColorMatrix.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/effects/SkRuntimeEffect.h"
#include "litehtml/css_parser.h"
#include "litehtml/el_table.h"
#include "litehtml/el_td.h"
#include "litehtml/el_tr.h"
#include "litehtml/render_item.h"
#include "text_utils.h"
#include "utils/skia_utils.h"
#include "utils/skunicode_satoru.h"

namespace litehtml {
vector<css_token_vector> parse_comma_separated_list(const css_token_vector& tokens);
}

void container_skia::push_backdrop_filter(litehtml::uint_ptr hdc,
                                          const std::shared_ptr<litehtml::render_item>& el) {
    if (!m_canvas || el->src_el()->css().get_backdrop_filter().empty()) return;
    flush();

    if (m_tagging) {
        backdrop_filter_info info;
        info.tokens = el->src_el()->css().get_backdrop_filter();

        litehtml::position el_pos_abs = el->get_placement();
        info.box_pos.x = (int)(el_pos_abs.x - el->padding_left() - el->border_left());
        info.box_pos.y = (int)(el_pos_abs.y - el->padding_top() - el->border_top());
        info.box_pos.width = (int)(el->pos().width + el->padding_left() + el->padding_right() +
                                   el->border_left() + el->border_right());
        info.box_pos.height = (int)(el->pos().height + el->padding_top() + el->padding_bottom() +
                                    el->border_top() + el->border_bottom());

        info.box_radius = el->src_el()->css().get_borders().radius.calc_percents(
            info.box_pos.width, info.box_pos.height);
        info.opacity = get_current_opacity();

        m_usedBackdropFilters.push_back(info);
        int index = (int)m_usedBackdropFilters.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::BackdropFilterPush, index));

        // Draw a tiny rectangle with magic color to signal the push
        m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
        return;
    }

    sk_sp<SkImageFilter> last_filter = nullptr;

    for (const auto& tok : el->src_el()->css().get_backdrop_filter()) {
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
            } else if (name == "brightness") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    float mat[20] = {amount, 0, 0,      0, 0, 0, amount, 0, 0, 0,
                                     0,      0, amount, 0, 0, 0, 0,      0, 1, 0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "contrast") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    float intercept = -(0.5f * amount) + 0.5f;
                    SkColorMatrix cm;
                    float mat[20] = {amount, 0, 0,      0, intercept, 0, amount, 0, 0, intercept,
                                     0,      0, amount, 0, intercept, 0, 0,      0, 1, 0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "grayscale") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 0.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    cm.setSaturation(1.0f - amount);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "sepia") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 0.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    float mat[20] = {0.393f + 0.607f * (1.0f - amount),
                                     0.769f - 0.769f * (1.0f - amount),
                                     0.189f - 0.189f * (1.0f - amount),
                                     0,
                                     0,
                                     0.349f - 0.349f * (1.0f - amount),
                                     0.686f + 0.314f * (1.0f - amount),
                                     0.168f - 0.168f * (1.0f - amount),
                                     0,
                                     0,
                                     0.272f - 0.272f * (1.0f - amount),
                                     0.534f - 0.534f * (1.0f - amount),
                                     0.131f + 0.869f * (1.0f - amount),
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     1,
                                     0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "saturate") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    cm.setSaturation(amount);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "hue-rotate") {
                if (!args.empty() && !args[0].empty()) {
                    float angle = 0.0f;
                    if (args[0][0].type == litehtml::DIMENSION) {
                        angle = args[0][0].n.number;
                    }
                    // SkColorMatrix doesn't have direct hue rotation helpers, use setRowMajor with
                    // rotation matrix For brevity, using a standard hue rotation matrix calculation
                    // (simplified)
                    float c = cosf(angle * SK_ScalarPI / 180.0f);
                    float s = sinf(angle * SK_ScalarPI / 180.0f);
                    SkColorMatrix cm;
                    float mat[20] = {0.213f + c * 0.787f - s * 0.213f,
                                     0.715f - c * 0.715f - s * 0.715f,
                                     0.072f - c * 0.072f + s * 0.928f,
                                     0,
                                     0,
                                     0.213f - c * 0.213f + s * 0.143f,
                                     0.715f + c * 0.285f + s * 0.140f,
                                     0.072f - c * 0.072f - s * 0.283f,
                                     0,
                                     0,
                                     0.213f - c * 0.213f - s * 0.787f,
                                     0.715f - c * 0.715f + s * 0.715f,
                                     0.072f + c * 0.928f + s * 0.072f,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     1,
                                     0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "invert") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 0.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    SkColorMatrix cm;
                    float mat[20] = {1.0f - 2.0f * amount,
                                     0,
                                     0,
                                     0,
                                     amount,
                                     0,
                                     1.0f - 2.0f * amount,
                                     0,
                                     0,
                                     amount,
                                     0,
                                     0,
                                     1.0f - 2.0f * amount,
                                     0,
                                     amount,
                                     0,
                                     0,
                                     0,
                                     1,
                                     0};
                    cm.setRowMajor(mat);
                    last_filter =
                        SkImageFilters::ColorFilter(SkColorFilters::Matrix(cm), last_filter);
                }
            } else if (name == "opacity") {
                if (!args.empty() && !args[0].empty()) {
                    float amount = 1.0f;
                    if (args[0][0].type == litehtml::NUMBER ||
                        args[0][0].type == litehtml::PERCENTAGE) {
                        amount = args[0][0].n.number /
                                 (args[0][0].type == litehtml::PERCENTAGE ? 100.0f : 1.0f);
                    }
                    last_filter = SkImageFilters::ColorFilter(
                        SkColorFilters::Blend(SkColorSetARGB((U8CPU)(amount * 255), 255, 255, 255),
                                              SkBlendMode::kDstIn),
                        last_filter);
                }
            }
        }
    }

    if (last_filter) {
        litehtml::position el_pos_abs = el->get_placement();
        litehtml::position border_box;
        border_box.x = (int)(el_pos_abs.x - el->padding_left() - el->border_left());
        border_box.y = (int)(el_pos_abs.y - el->padding_top() - el->border_top());
        border_box.width = (int)(el->pos().width + el->padding_left() + el->padding_right() +
                                 el->border_left() + el->border_right());
        border_box.height = (int)(el->pos().height + el->padding_top() + el->padding_bottom() +
                                  el->border_top() + el->border_bottom());

        litehtml::border_radiuses bdr_radius =
            el->src_el()->css().get_borders().radius.calc_percents(border_box.width,
                                                                   border_box.height);

        m_canvas->save();
        m_canvas->clipRRect(make_rrect(border_box, bdr_radius), true);

        SkCanvas::SaveLayerRec layer_rec(nullptr, nullptr, last_filter.get(), 0);
        m_canvas->saveLayer(layer_rec);
    } else {
        // Save twice to balance the two restores in pop
        m_canvas->save();
        m_canvas->save();
    }
}

void container_skia::pop_backdrop_filter(litehtml::uint_ptr hdc) {
    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::BackdropFilterPop));
            m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
            return;
        }
        m_canvas->restore();  // for saveLayer
        m_canvas->restore();  // for clip's save
    }
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

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n'\"");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n'\"");
    return s.substr(start, end - start + 1);
}

SkRRect get_background_rrect(const litehtml::background_layer& layer) {
    litehtml::position intersect_box = layer.border_box.intersect(layer.clip_box);
    if (intersect_box.width <= 0 || intersect_box.height <= 0) {
        return SkRRect::MakeEmpty();
    }

    litehtml::border_radiuses rad = layer.border_radius;
    float offset_l = std::max(0.0f, intersect_box.x - layer.border_box.x);
    float offset_t = std::max(0.0f, intersect_box.y - layer.border_box.y);
    float offset_r = std::max(0.0f, layer.border_box.right() - intersect_box.right());
    float offset_b = std::max(0.0f, layer.border_box.bottom() - intersect_box.bottom());

    rad.top_left_x = std::max(0.0f, rad.top_left_x - offset_l);
    rad.top_left_y = std::max(0.0f, rad.top_left_y - offset_t);
    rad.top_right_x = std::max(0.0f, rad.top_right_x - offset_r);
    rad.top_right_y = std::max(0.0f, rad.top_right_y - offset_t);
    rad.bottom_right_x = std::max(0.0f, rad.bottom_right_x - offset_r);
    rad.bottom_right_y = std::max(0.0f, rad.bottom_right_y - offset_b);
    rad.bottom_left_x = std::max(0.0f, rad.bottom_left_x - offset_l);
    rad.bottom_left_y = std::max(0.0f, rad.bottom_left_y - offset_b);

    return make_rrect(intersect_box, rad);
}
}  // namespace

container_skia::container_skia(int w, int h, SkCanvas* canvas, SatoruContext& context,
                               ResourceManager* rm, bool tagging)
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

litehtml::uint_ptr container_skia::create_font(const litehtml::font_description& desc,
                                               const litehtml::document* doc,
                                               litehtml::font_metrics* fm) {
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
        for (auto& tf : tfs) {
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

    font_info* fi = new font_info;
    fi->desc = desc;
    fi->fake_bold = fake_bold;

    // Check direction from element's property (if available)
    fi->is_rtl = false;

    for (auto& typeface : typefaces) {
        SkFont* font = m_context.fontManager.createSkFont(typeface, (float)desc.size, desc.weight);
        if (font) {
            fi->fonts.push_back(font);
        }
    }

    // Add global fallbacks
    for (auto& tf : m_context.fontManager.getFallbackTypefaces()) {
        bool duplicate = false;
        for (auto& existing : fi->fonts) {
            if (existing->getTypeface()->uniqueID() == tf->uniqueID()) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            SkFont* font = m_context.fontManager.createSkFont(tf, (float)desc.size, desc.weight);
            if (font) {
                fi->fonts.push_back(font);
            }
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
        auto typeface = fi->fonts[0]->getTypeface();
        // Use getMatchedWeight to get the INTENDED weight (handles subset fonts with broken
        // metadata)
        int actual_weight =
            m_context.fontManager.getMatchedWeight(sk_ref_sp(typeface), desc.family);

        // If the matched weight is sufficient, disable fake_bold
        if (actual_weight >= desc.weight) {
            fi->fake_bold = false;
        } else {
            // Check if it's a variable font by looking for variation axes
            int count = typeface->getVariationDesignPosition(
                SkSpan<SkFontArguments::VariationPosition::Coordinate>());
            if (count > 0) {
                // If it's a variable font, we trust that createSkFont handled the weight axis
                fi->fake_bold = false;
            }
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
    fi->fm_ascent_raw = ascent;
    fi->fm_height = (int)css_line_height;

    font_request req;
    req.family = desc.family;
    req.weight = desc.weight;
    req.slant = slant;
    m_createdFonts[req].push_back(fi);

    return (litehtml::uint_ptr)fi;
}

void container_skia::delete_font(litehtml::uint_ptr hFont) {
    font_info* fi = (font_info*)hFont;
    if (fi) {
        for (auto& entry : m_createdFonts) {
            auto& v = entry.second;
            v.erase(std::remove(v.begin(), v.end(), fi), v.end());
        }
        for (auto font : fi->fonts) delete font;
        delete fi;
    }
}

litehtml::pixel_t container_skia::text_width(const char* text, litehtml::uint_ptr hFont,
                                             litehtml::direction dir, litehtml::writing_mode mode) {
    font_info* fi = (font_info*)hFont;
    if (fi) {
        fi->is_rtl = (dir == litehtml::direction_rtl);
    }
    auto result = satoru::TextLayout::measureText(&m_context, text, fi, mode, -1.0,
                                                  m_resourceManager ? &m_usedCodepoints : nullptr);
    return (litehtml::pixel_t)result.width;
}

void container_skia::draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont,
                               litehtml::web_color color, const litehtml::position& pos,
                               litehtml::text_overflow overflow, litehtml::direction dir,
                               litehtml::writing_mode mode) {
    if (!m_canvas) return;
    font_info* fi = (font_info*)hFont;
    if (!fi || fi->fonts.empty()) return;

    litehtml::position actual_pos = pos;
    if (overflow == litehtml::text_overflow_ellipsis && !m_clips.empty()) {
        actual_pos.width =
            std::min(pos.width, (litehtml::pixel_t)(m_clips.back().first.right() - pos.x));
    }

    // background-clip: text support for PNG rendering
    // When text is transparent and there's a pending text-clip gradient,
    // use saveLayer + SrcIn compositing to fill text shapes with the gradient
    if (!m_tagging && color.alpha == 0 && !m_pending_text_clips.empty()) {
        for (const auto& tc : m_pending_text_clips) {
            const auto& clip = tc.layer.clip_box;
            bool overlaps = (actual_pos.x < clip.right() && actual_pos.right() > clip.x &&
                             actual_pos.y < clip.bottom() && actual_pos.bottom() > clip.y);
            if (overlaps) {
                flush();

                SkRect bounds = SkRect::MakeXYWH((float)clip.x, (float)clip.y, (float)clip.width,
                                                 (float)clip.height);
                m_canvas->saveLayer(bounds, nullptr);

                // Draw text as opaque white mask
                litehtml::web_color maskColor;
                maskColor.red = 255;
                maskColor.green = 255;
                maskColor.blue = 255;
                maskColor.alpha = 255;
                satoru::TextRenderer::drawText(
                    &m_context, m_canvas, text, fi, maskColor, actual_pos, overflow, dir, mode,
                    false, get_current_opacity(), m_usedTextShadows, m_usedTextDraws, m_usedGlyphs,
                    m_usedGlyphDraws, m_resourceManager ? &m_usedCodepoints : nullptr,
                    m_textBatcher);
                flush();

                // Draw gradient with SrcIn blend mode (only visible through text mask)
                const auto& gradient = tc.gradient;
                SkPoint pts[2] = {SkPoint::Make((float)gradient.start.x, (float)gradient.start.y),
                                  SkPoint::Make((float)gradient.end.x, (float)gradient.end.y)};
                std::vector<SkColor4f> colors;
                std::vector<float> positions;
                for (const auto& stop : gradient.color_points) {
                    colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                                      stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
                    positions.push_back(stop.offset);
                }
                SkGradient sk_grad(
                    SkGradient::Colors(SkSpan(colors), SkSpan(positions), SkTileMode::kClamp),
                    SkGradient::Interpolation());
                SkPaint gradPaint;
                gradPaint.setShader(SkShaders::LinearGradient(pts, sk_grad));
                gradPaint.setBlendMode(SkBlendMode::kSrcIn);
                gradPaint.setAntiAlias(true);
                m_canvas->drawRect(bounds, gradPaint);

                m_canvas->restore();

                // Resource manager pass for font subsetting
                if (fi && m_resourceManager) {
                    satoru::TextRenderer::drawText(
                        &m_context, nullptr, text, fi, maskColor, actual_pos, overflow, dir, mode,
                        false, get_current_opacity(), m_usedTextShadows, m_usedTextDraws,
                        m_usedGlyphs, m_usedGlyphDraws, &fi->used_codepoints, nullptr);
                }
                return;
            }
        }
    }

    satoru::TextRenderer::drawText(&m_context, m_canvas, text, fi, color, actual_pos, overflow, dir,
                                   mode, m_tagging, get_current_opacity(), m_usedTextShadows,
                                   m_usedTextDraws, m_usedGlyphs, m_usedGlyphDraws,
                                   m_resourceManager ? &m_usedCodepoints : nullptr, m_textBatcher);
    if (fi && m_resourceManager) {
        satoru::TextRenderer::drawText(&m_context, nullptr, text, fi, color, actual_pos, overflow,
                                       dir, mode, m_tagging, get_current_opacity(),
                                       m_usedTextShadows, m_usedTextDraws, m_usedGlyphs,
                                       m_usedGlyphDraws, &fi->used_codepoints, nullptr);
    }
}

void container_skia::draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector& shadows,
                                     const litehtml::position& pos,
                                     const litehtml::border_radiuses& radius, bool inset) {
    if (!m_canvas) return;
    flush();
    if (m_tagging) {
        for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
            const auto& s = *it;
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
            p.setColor(make_magic_color(satoru::MagicTag::Shadow, index));
            m_canvas->drawRRect(make_rrect(pos, radius), p);
        }
        return;
    }
    for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
        const auto& s = *it;
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

void container_skia::draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer& layer,
                                const std::string& url, const std::string& base_url,
                                litehtml::object_fit fit,
                                const litehtml::css_token_vector& object_position) {
    if (!m_canvas) return;
    flush();
    if (m_tagging) {
        image_draw_info draw;
        draw.url = url;
        draw.layer = layer;
        draw.opacity = 1.0f;
        draw.object_fit = fit;
        draw.object_position = object_position;

        // Use background layer's clip_box as primary clipping
        draw.has_clip = true;
        litehtml::position intersect_box = layer.border_box.intersect(layer.clip_box);
        draw.clip_pos = intersect_box;

        float offset_l = std::max(0.0f, intersect_box.x - layer.border_box.x);
        float offset_t = std::max(0.0f, intersect_box.y - layer.border_box.y);
        float offset_r = std::max(0.0f, layer.border_box.right() - intersect_box.right());
        float offset_b = std::max(0.0f, layer.border_box.bottom() - intersect_box.bottom());

        draw.clip_radius = layer.border_radius;
        draw.clip_radius.top_left_x = std::max(0.0f, draw.clip_radius.top_left_x - offset_l);
        draw.clip_radius.top_left_y = std::max(0.0f, draw.clip_radius.top_left_y - offset_t);
        draw.clip_radius.top_right_x = std::max(0.0f, draw.clip_radius.top_right_x - offset_r);
        draw.clip_radius.top_right_y = std::max(0.0f, draw.clip_radius.top_right_y - offset_t);
        draw.clip_radius.bottom_right_x =
            std::max(0.0f, draw.clip_radius.bottom_right_x - offset_r);
        draw.clip_radius.bottom_right_y =
            std::max(0.0f, draw.clip_radius.bottom_right_y - offset_b);
        draw.clip_radius.bottom_left_x = std::max(0.0f, draw.clip_radius.bottom_left_x - offset_l);
        draw.clip_radius.bottom_left_y = std::max(0.0f, draw.clip_radius.bottom_left_y - offset_b);

        int index = (int)m_usedImageDraws.size() + 1;
        draw.tag_index = index;
        m_usedImageDraws.push_back(draw);

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTagExtended::ImageDraw, index));

        // Fill the clip_box area with magic color
        m_canvas->drawRect(
            SkRect::MakeXYWH((float)layer.clip_box.x, (float)layer.clip_box.y,
                             (float)layer.clip_box.width, (float)layer.clip_box.height),
            p);
    } else {
        auto it = m_context.imageCache.find(url);
        if (it != m_context.imageCache.end() && it->second.skImage) {
            SkPaint p;
            p.setAntiAlias(true);

            m_canvas->save();
            // Clip to layer.clip_box which respects background-clip
            m_canvas->clipRRect(get_background_rrect(layer), true);

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
                                     const litehtml::background_layer& layer,
                                     const litehtml::web_color& color) {
    if (!m_canvas) return;
    flush();
    SkPaint p;
    p.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    p.setAntiAlias(true);
    m_canvas->drawRRect(get_background_rrect(layer), p);
}

void container_skia::draw_linear_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer& layer,
    const litehtml::background_layer::linear_gradient& gradient) {
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
        if (layer.is_text_clip) {
            p.setColor(make_magic_color(satoru::MagicTagExtended::TextClipLinearGradient, index));
        } else {
            p.setColor(make_magic_color(satoru::MagicTagExtended::LinearGradient, index));
        }
        m_canvas->drawRRect(get_background_rrect(layer), p);
    } else {
        if (layer.origin_box.width <= 0 || layer.origin_box.height <= 0) return;

        // Store text-clip gradients for compositing in draw_text
        if (layer.is_text_clip) {
            pending_text_clip tc;
            tc.layer = layer;
            tc.gradient = gradient;
            m_pending_text_clips.push_back(tc);
            return;
        }

        SkBitmap tileBitmap;
        tileBitmap.allocN32Pixels((int)layer.origin_box.width, (int)layer.origin_box.height);
        SkCanvas tileCanvas(tileBitmap);
        tileCanvas.clear(SK_ColorTRANSPARENT);

        SkPoint pts[2] = {SkPoint::Make((float)gradient.start.x - (float)layer.origin_box.x,
                                        (float)gradient.start.y - (float)layer.origin_box.y),
                          SkPoint::Make((float)gradient.end.x - (float)layer.origin_box.x,
                                        (float)gradient.end.y - (float)layer.origin_box.y)};
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        for (const auto& stop : gradient.color_points) {
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
            pos.push_back(stop.offset);
        }

        SkGradient sk_grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                           SkGradient::Interpolation());
        SkPaint p;
        p.setShader(SkShaders::LinearGradient(pts, sk_grad));
        p.setAntiAlias(true);
        tileCanvas.drawRect(
            SkRect::MakeWH((float)layer.origin_box.width, (float)layer.origin_box.height), p);

        SkTileMode tileX = SkTileMode::kRepeat;
        SkTileMode tileY = SkTileMode::kRepeat;
        if (layer.repeat == litehtml::background_repeat_no_repeat) {
            tileX = tileY = SkTileMode::kDecal;
        } else if (layer.repeat == litehtml::background_repeat_repeat_x) {
            tileY = SkTileMode::kDecal;
        } else if (layer.repeat == litehtml::background_repeat_repeat_y) {
            tileX = SkTileMode::kDecal;
        }

        SkMatrix matrix;
        matrix.setTranslate((float)layer.origin_box.x, (float)layer.origin_box.y);

        SkPaint repeatPaint;
        repeatPaint.setShader(tileBitmap.makeShader(tileX, tileY, SkSamplingOptions(), &matrix));
        repeatPaint.setAntiAlias(true);

        m_canvas->save();
        m_canvas->clipRRect(get_background_rrect(layer), true);
        m_canvas->drawRect(
            SkRect::MakeXYWH((float)layer.clip_box.x, (float)layer.clip_box.y,
                             (float)layer.clip_box.width, (float)layer.clip_box.height),
            repeatPaint);
        m_canvas->restore();
    }
}

void container_skia::draw_radial_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer& layer,
    const litehtml::background_layer::radial_gradient& gradient) {
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
        p.setColor(make_magic_color(satoru::MagicTagExtended::RadialGradient, index));
        m_canvas->drawRRect(get_background_rrect(layer), p);
    } else {
        if (layer.origin_box.width <= 0 || layer.origin_box.height <= 0) return;

        SkBitmap tileBitmap;
        tileBitmap.allocN32Pixels((int)layer.origin_box.width, (int)layer.origin_box.height);
        SkCanvas tileCanvas(tileBitmap);
        tileCanvas.clear(SK_ColorTRANSPARENT);

        SkPoint center = SkPoint::Make((float)gradient.position.x - (float)layer.origin_box.x,
                                       (float)gradient.position.y - (float)layer.origin_box.y);
        float rx = (float)gradient.radius.x, ry = (float)gradient.radius.y;
        if (rx <= 0 || ry <= 0) return;
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        for (const auto& stop : gradient.color_points) {
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
            pos.push_back(stop.offset);
        }
        SkMatrix matrix;
        matrix.setScale(1.0f, ry / rx, center.x(), center.y());

        SkGradient sk_grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                           SkGradient::Interpolation());
        SkPaint p;
        p.setShader(SkShaders::RadialGradient(center, rx, sk_grad, &matrix));
        p.setAntiAlias(true);
        tileCanvas.drawRect(
            SkRect::MakeWH((float)layer.origin_box.width, (float)layer.origin_box.height), p);

        SkTileMode tileX = SkTileMode::kRepeat;
        SkTileMode tileY = SkTileMode::kRepeat;
        if (layer.repeat == litehtml::background_repeat_no_repeat) {
            tileX = tileY = SkTileMode::kDecal;
        } else if (layer.repeat == litehtml::background_repeat_repeat_x) {
            tileY = SkTileMode::kDecal;
        } else if (layer.repeat == litehtml::background_repeat_repeat_y) {
            tileX = SkTileMode::kDecal;
        }

        SkMatrix repeatMatrix;
        repeatMatrix.setTranslate((float)layer.origin_box.x, (float)layer.origin_box.y);

        SkPaint repeatPaint;
        repeatPaint.setShader(
            tileBitmap.makeShader(tileX, tileY, SkSamplingOptions(), &repeatMatrix));
        repeatPaint.setAntiAlias(true);

        m_canvas->save();
        m_canvas->clipRRect(get_background_rrect(layer), true);
        m_canvas->drawRect(
            SkRect::MakeXYWH((float)layer.clip_box.x, (float)layer.clip_box.y,
                             (float)layer.clip_box.width, (float)layer.clip_box.height),
            repeatPaint);
        m_canvas->restore();
    }
}

void container_skia::draw_conic_gradient(
    litehtml::uint_ptr hdc, const litehtml::background_layer& layer,
    const litehtml::background_layer::conic_gradient& gradient) {
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
        p.setColor(make_magic_color(satoru::MagicTagExtended::ConicGradient, index));
        m_canvas->drawRRect(get_background_rrect(layer), p);
    } else {
        if (layer.origin_box.width <= 0 || layer.origin_box.height <= 0) return;

        SkBitmap tileBitmap;
        tileBitmap.allocN32Pixels((int)layer.origin_box.width, (int)layer.origin_box.height);
        SkCanvas tileCanvas(tileBitmap);
        tileCanvas.clear(SK_ColorTRANSPARENT);

        SkPoint center = SkPoint::Make((float)gradient.position.x - (float)layer.origin_box.x,
                                       (float)gradient.position.y - (float)layer.origin_box.y);
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        for (size_t i = 0; i < gradient.color_points.size(); ++i) {
            const auto& stop = gradient.color_points[i];
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
            for (auto& p : pos) p /= max_val;
            pos.back() = 1.0f;
        }

        SkGradient sk_grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                           SkGradient::Interpolation());
        SkMatrix matrix;
        matrix.setRotate(gradient.angle - 90.0f, center.x(), center.y());
        SkPaint p;
        p.setShader(SkShaders::SweepGradient(center, sk_grad, &matrix));
        p.setAntiAlias(true);
        tileCanvas.drawRect(
            SkRect::MakeWH((float)layer.origin_box.width, (float)layer.origin_box.height), p);

        SkTileMode tileX = SkTileMode::kRepeat;
        SkTileMode tileY = SkTileMode::kRepeat;
        if (layer.repeat == litehtml::background_repeat_no_repeat) {
            tileX = tileY = SkTileMode::kDecal;
        } else if (layer.repeat == litehtml::background_repeat_repeat_x) {
            tileY = SkTileMode::kDecal;
        } else if (layer.repeat == litehtml::background_repeat_repeat_y) {
            tileX = SkTileMode::kDecal;
        }

        SkMatrix repeatMatrix;
        repeatMatrix.setTranslate((float)layer.origin_box.x, (float)layer.origin_box.y);

        SkPaint repeatPaint;
        repeatPaint.setShader(
            tileBitmap.makeShader(tileX, tileY, SkSamplingOptions(), &repeatMatrix));
        repeatPaint.setAntiAlias(true);

        m_canvas->save();
        m_canvas->clipRRect(get_background_rrect(layer), true);
        m_canvas->drawRect(
            SkRect::MakeXYWH((float)layer.clip_box.x, (float)layer.clip_box.y,
                             (float)layer.clip_box.width, (float)layer.clip_box.height),
            repeatPaint);
        m_canvas->restore();
    }
}

#include "api/satoru_api.h"
#include "bridge/bridge_types.h"

void container_skia::draw_border_image(litehtml::uint_ptr hdc,
                                       const litehtml::border_image& border_image,
                                       const litehtml::borders& borders,
                                       const litehtml::position& draw_pos, bool root) {
    if (!m_canvas) return;
    flush();

    if (m_tagging) {
        border_image_info info;
        info.border_image = border_image;
        info.borders = borders;
        info.draw_pos = draw_pos;
        info.opacity = get_current_opacity();
        m_usedBorderImages.push_back(info);
        int index = (int)m_usedBorderImages.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTagExtended::BorderImage, index));
        m_canvas->drawRect(SkRect::MakeXYWH((float)draw_pos.x, (float)draw_pos.y,
                                            (float)draw_pos.width, (float)draw_pos.height),
                           p);
        return;
    }

    sk_sp<SkImage> img;
    if (border_image.source.type == litehtml::image::type_url) {
        auto it = m_context.imageCache.find(border_image.source.url);
        if (it == m_context.imageCache.end() || !it->second.skImage) return;
        img = it->second.skImage;
    } else if (border_image.source.type == litehtml::image::type_gradient) {
        // For border-image, the gradient's intrinsic size is the border image area.
        int w = draw_pos.width;
        int h = draw_pos.height;
        if (w <= 0 || h <= 0) return;

        SkBitmap bitmap;
        if (!bitmap.tryAllocN32Pixels(w, h)) return;
        SkCanvas canvas(bitmap);
        canvas.clear(SK_ColorTRANSPARENT);

        // We need a temporary container_skia to use its gradient drawing methods,
        // but we can also just use the current one with a different canvas.
        SkCanvas* old_canvas = m_canvas;
        m_canvas = &canvas;

        litehtml::background_layer layer;
        layer.origin_box = litehtml::position(0, 0, w, h);
        layer.border_box = layer.origin_box;
        layer.clip_box = layer.origin_box;

        if (border_image.source.m_gradient.m_type == litehtml::_linear_gradient_ ||
            border_image.source.m_gradient.m_type == litehtml::_repeating_linear_gradient_) {
            auto grad = litehtml::background::get_linear_gradient(border_image.source.m_gradient,
                                                                  layer.origin_box);
            if (grad) draw_linear_gradient(0, layer, *grad);
        } else if (border_image.source.m_gradient.m_type == litehtml::_radial_gradient_ ||
                   border_image.source.m_gradient.m_type == litehtml::_repeating_radial_gradient_) {
            auto grad = litehtml::background::get_radial_gradient(border_image.source.m_gradient,
                                                                  layer.origin_box);
            if (grad) draw_radial_gradient(0, layer, *grad);
        } else if (border_image.source.m_gradient.m_type == litehtml::_conic_gradient_ ||
                   border_image.source.m_gradient.m_type == litehtml::_repeating_conic_gradient_) {
            auto grad = litehtml::background::get_conic_gradient(border_image.source.m_gradient,
                                                                 layer.origin_box);
            if (grad) draw_conic_gradient(0, layer, *grad);
        }

        m_canvas = old_canvas;
        img = bitmap.asImage();
    }

    if (!img) return;
    float img_w = (float)img->width();
    float img_h = (float)img->height();

    // Calculate slice pixel values
    float s_t = border_image.slice[0].calc_percent((int)img_h);
    float s_r = border_image.slice[1].calc_percent((int)img_w);
    float s_b = border_image.slice[2].calc_percent((int)img_h);
    float s_l = border_image.slice[3].calc_percent((int)img_w);

    // border-image-slice values are edges, so they can't exceed image dimensions
    if (s_t + s_b > img_h) {
        float scale = img_h / (s_t + s_b);
        s_t *= scale;
        s_b *= scale;
    }
    if (s_l + s_r > img_w) {
        float scale = img_w / (s_l + s_r);
        s_l *= scale;
        s_r *= scale;
    }

    // Calculate output width pixel values (using border-image-width)
    auto calc_width = [&](const litehtml::css_length& w, int border_w, int total) {
        if (w.is_predefined()) return (float)border_w;
        if (w.units() == litehtml::css_units_none) return w.val() * border_w;
        return (float)w.calc_percent(total);
    };

    float w_t = calc_width(border_image.width[0], borders.top.width, draw_pos.height);
    float w_r = calc_width(border_image.width[1], borders.right.width, draw_pos.width);
    float w_b = calc_width(border_image.width[2], borders.bottom.width, draw_pos.height);
    float w_l = calc_width(border_image.width[3], borders.left.width, draw_pos.width);

    // Calculate outset
    auto calc_outset = [&](const litehtml::css_length& len, int border_w, int total) {
        if (len.units() == litehtml::css_units_none) return len.val() * border_w;
        return (float)len.calc_percent(total);
    };

    float o_t = calc_outset(border_image.outset[0], borders.top.width, draw_pos.height);
    float o_r = calc_outset(border_image.outset[1], borders.right.width, draw_pos.width);
    float o_b = calc_outset(border_image.outset[2], borders.bottom.width, draw_pos.height);
    float o_l = calc_outset(border_image.outset[3], borders.left.width, draw_pos.width);

    SkRect dst_full =
        SkRect::MakeXYWH((float)draw_pos.x - o_l, (float)draw_pos.y - o_t,
                         (float)draw_pos.width + o_l + o_r, (float)draw_pos.height + o_t + o_b);

    // Source rects (9-slice)
    SkRect src[9];
    src[0] = SkRect::MakeXYWH(0, 0, s_l, s_t);                                  // top-left
    src[1] = SkRect::MakeXYWH(s_l, 0, img_w - s_l - s_r, s_t);                  // top
    src[2] = SkRect::MakeXYWH(img_w - s_r, 0, s_r, s_t);                        // top-right
    src[3] = SkRect::MakeXYWH(0, s_t, s_l, img_h - s_t - s_b);                  // left
    src[4] = SkRect::MakeXYWH(s_l, s_t, img_w - s_l - s_r, img_h - s_t - s_b);  // center
    src[5] = SkRect::MakeXYWH(img_w - s_r, s_t, s_r, img_h - s_t - s_b);        // right
    src[6] = SkRect::MakeXYWH(0, img_h - s_b, s_l, s_b);                        // bottom-left
    src[7] = SkRect::MakeXYWH(s_l, img_h - s_b, img_w - s_l - s_r, s_b);        // bottom
    src[8] = SkRect::MakeXYWH(img_w - s_r, img_h - s_b, s_r, s_b);              // bottom-right

    // Destination rects
    SkRect dst[9];
    dst[0] = SkRect::MakeXYWH(dst_full.left(), dst_full.top(), w_l, w_t);
    dst[1] =
        SkRect::MakeXYWH(dst_full.left() + w_l, dst_full.top(), dst_full.width() - w_l - w_r, w_t);
    dst[2] = SkRect::MakeXYWH(dst_full.right() - w_r, dst_full.top(), w_r, w_t);
    dst[3] =
        SkRect::MakeXYWH(dst_full.left(), dst_full.top() + w_t, w_l, dst_full.height() - w_t - w_b);
    dst[4] = SkRect::MakeXYWH(dst_full.left() + w_l, dst_full.top() + w_t,
                              dst_full.width() - w_l - w_r, dst_full.height() - w_t - w_b);
    dst[5] = SkRect::MakeXYWH(dst_full.right() - w_r, dst_full.top() + w_t, w_r,
                              dst_full.height() - w_t - w_b);
    dst[6] = SkRect::MakeXYWH(dst_full.left(), dst_full.bottom() - w_b, w_l, w_b);
    dst[7] = SkRect::MakeXYWH(dst_full.left() + w_l, dst_full.bottom() - w_b,
                              dst_full.width() - w_l - w_r, w_b);
    dst[8] = SkRect::MakeXYWH(dst_full.right() - w_r, dst_full.bottom() - w_b, w_r, w_b);

    SkPaint p;
    p.setAntiAlias(true);
    p.setAlphaf(get_current_opacity());

    auto draw_piece = [&](int idx, litehtml::border_image_repeat rep_h,
                          litehtml::border_image_repeat rep_v) {
        if (src[idx].width() <= 0 || src[idx].height() <= 0 || dst[idx].width() <= 0 ||
            dst[idx].height() <= 0)
            return;

        if (rep_h == litehtml::border_image_repeat_stretch &&
            rep_v == litehtml::border_image_repeat_stretch) {
            m_canvas->drawImageRect(img, src[idx], dst[idx],
                                    SkSamplingOptions(SkFilterMode::kLinear), &p,
                                    SkCanvas::kFast_SrcRectConstraint);
        } else {
            m_canvas->save();
            m_canvas->clipRect(dst[idx]);

            SkTileMode tm_h = (rep_h == litehtml::border_image_repeat_stretch)
                                  ? SkTileMode::kClamp
                                  : SkTileMode::kRepeat;
            SkTileMode tm_v = (rep_v == litehtml::border_image_repeat_stretch)
                                  ? SkTileMode::kClamp
                                  : SkTileMode::kRepeat;

            // Tile size in destination
            float tile_w, tile_h;
            if (idx == 1 || idx == 7) {  // top, bottom
                tile_h = dst[idx].height();
                tile_w = src[idx].width() * (tile_h / src[idx].height());
            } else if (idx == 3 || idx == 5) {  // left, right
                tile_w = dst[idx].width();
                tile_h = src[idx].height() * (tile_w / src[idx].width());
            } else {                                      // center
                tile_w = src[idx].width() * (w_t / s_t);  // use top border width as scale reference
                tile_h = src[idx].height() * (w_l / s_l);
            }

            if (rep_h == litehtml::border_image_repeat_round) {
                int count = (int)std::max(1.0f, std::round(dst[idx].width() / tile_w));
                tile_w = dst[idx].width() / count;
            }
            if (rep_v == litehtml::border_image_repeat_round) {
                int count = (int)std::max(1.0f, std::round(dst[idx].height() / tile_h));
                tile_h = dst[idx].height() / count;
            }

            SkMatrix m;
            m.setScaleTranslate(tile_w / src[idx].width(), tile_h / src[idx].height(),
                                dst[idx].left(), dst[idx].top());
            // Adjust translation for source slice
            m.preTranslate(-src[idx].left(), -src[idx].top());

            p.setShader(img->makeShader(tm_h, tm_v, SkSamplingOptions(SkFilterMode::kLinear), &m));
            m_canvas->drawRect(dst[idx], p);
            p.setShader(nullptr);

            m_canvas->restore();
        }
    };

    // Corners
    for (int i : {0, 2, 6, 8}) {
        draw_piece(i, litehtml::border_image_repeat_stretch, litehtml::border_image_repeat_stretch);
    }

    // Sides
    draw_piece(1, border_image.repeat_h, litehtml::border_image_repeat_stretch);  // top
    draw_piece(7, border_image.repeat_h, litehtml::border_image_repeat_stretch);  // bottom
    draw_piece(3, litehtml::border_image_repeat_stretch, border_image.repeat_v);  // left
    draw_piece(5, litehtml::border_image_repeat_stretch, border_image.repeat_v);  // right

    // Center
    if (border_image.slice_fill) {
        draw_piece(4, border_image.repeat_h, border_image.repeat_v);
    }
}

void container_skia::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders,
                                  const litehtml::position& draw_pos, bool root) {
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

        auto draw_side = [&](const litehtml::border& b, const SkPath& quadrant, bool is_top_left,
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
const char* container_skia::get_default_font_name() const { return "sans-serif"; }

int container_skia::get_bidi_level(const char* text, int base_level) {
    if (m_last_base_level != base_level) {
        m_last_bidi_level = base_level;
        m_last_base_level = base_level;
    }
    return m_context.getUnicodeService().getBidiLevel(text, base_level, &m_last_bidi_level);
}

void container_skia::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) {
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
                p.setColor(make_magic_color(satoru::MagicTagExtended::ImageDraw, index));
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

void container_skia::load_image(const char* src, const char* baseurl, bool redraw_on_ready) {
    if (m_resourceManager && src && *src)
        m_resourceManager->request(src, src, ResourceType::Image, redraw_on_ready);
}
void container_skia::get_image_size(const char* src, const char* baseurl, litehtml::size& sz) {
    int w, h;
    if (m_context.get_image_size(src, w, h)) {
        sz.width = w;
        sz.height = h;
    } else {
        sz.width = 0;
        sz.height = 0;
    }
}
void container_skia::get_viewport(litehtml::position& viewport) const {
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = m_width;
    viewport.height = m_height;
}
void container_skia::transform_text(litehtml::string& text, litehtml::text_transform tt) {
    if (text.empty()) return;
    if (tt == litehtml::text_transform_uppercase)
        for (auto& c : text) c = (char)toupper(c);
    else if (tt == litehtml::text_transform_lowercase)
        for (auto& c : text) c = (char)tolower(c);
}
void container_skia::import_css(litehtml::string& text, const litehtml::string& url,
                                litehtml::string& baseurl) {
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

void container_skia::set_clip(const litehtml::position& pos,
                              const litehtml::border_radiuses& bdr_radius) {
    if (m_canvas) {
        flush();
        if (m_tagging) {
            clip_info info;
            info.pos = pos;
            info.radius = bdr_radius;
            m_usedClips.push_back(info);
            int index = (int)m_usedClips.size();

            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::ClipPush, index));
            m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
        } else {
            m_canvas->save();
            if (pos.width > 0 && pos.height > 0) {
                m_canvas->clipRRect(make_rrect(pos, bdr_radius), true);
            }
        }
    }
    m_clips.push_back({pos, bdr_radius});
}
void container_skia::del_clip() {
    if (m_clips.empty()) return;
    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::ClipPop));
            m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
        } else {
            m_canvas->restore();
        }
    }
    m_clips.pop_back();
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
    language = "en";
    culture = "en-US";
}

litehtml::string container_skia::resolve_color(const litehtml::string& color) const {
    if (color.empty()) return "";

    std::string name = color;
    if (name.size() > 4 && name.substr(0, 4) == "var(") {
        size_t pos = name.find('(');
        size_t end = name.find_last_of(')');
        if (pos != std::string::npos && end != std::string::npos && end > pos + 1) {
            name = name.substr(pos + 1, end - pos - 1);
            // Handle fallback if present (comma)
            size_t comma = name.find(',');
            if (comma != std::string::npos) {
                name = name.substr(0, comma);
            }
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t\r\n"));
            size_t last = name.find_last_not_of(" \t\r\n");
            if (last != std::string::npos) {
                name.erase(last + 1);
            }
        }
    }

    if (name.substr(0, 2) == "--") {
        if (m_doc && m_doc->root()) {
            litehtml::css_token_vector tokens;
            if (m_doc->root()->get_custom_property(litehtml::_id(name), tokens)) {
                return litehtml::get_repr(tokens);
            }
        }
    }
    return "";
}

void container_skia::split_text(const char* text, const std::function<void(const char*)>& on_word,
                                const std::function<void(const char*)>& on_space) {
    satoru::TextLayout::splitText(&m_context, text, on_word, on_space);
}

void container_skia::push_layer(litehtml::uint_ptr hdc, float opacity, litehtml::blend_mode bm) {
    m_opacity_stack.push_back(opacity);
    if (m_canvas) {
        flush();

        SkBlendMode sk_bm = SkBlendMode::kSrcOver;
        switch (bm) {
            case litehtml::blend_mode_multiply:
                sk_bm = SkBlendMode::kMultiply;
                break;
            case litehtml::blend_mode_screen:
                sk_bm = SkBlendMode::kScreen;
                break;
            case litehtml::blend_mode_overlay:
                sk_bm = SkBlendMode::kOverlay;
                break;
            case litehtml::blend_mode_darken:
                sk_bm = SkBlendMode::kDarken;
                break;
            case litehtml::blend_mode_lighten:
                sk_bm = SkBlendMode::kLighten;
                break;
            case litehtml::blend_mode_color_dodge:
                sk_bm = SkBlendMode::kColorDodge;
                break;
            case litehtml::blend_mode_color_burn:
                sk_bm = SkBlendMode::kColorBurn;
                break;
            case litehtml::blend_mode_hard_light:
                sk_bm = SkBlendMode::kHardLight;
                break;
            case litehtml::blend_mode_soft_light:
                sk_bm = SkBlendMode::kSoftLight;
                break;
            case litehtml::blend_mode_difference:
                sk_bm = SkBlendMode::kDifference;
                break;
            case litehtml::blend_mode_exclusion:
                sk_bm = SkBlendMode::kExclusion;
                break;
            case litehtml::blend_mode_hue:
                sk_bm = SkBlendMode::kHue;
                break;
            case litehtml::blend_mode_saturation:
                sk_bm = SkBlendMode::kSaturation;
                break;
            case litehtml::blend_mode_color:
                sk_bm = SkBlendMode::kColor;
                break;
            case litehtml::blend_mode_luminosity:
                sk_bm = SkBlendMode::kLuminosity;
                break;
            default:
                break;
        }

        if (m_tagging) {
            SkPaint p;
            if (bm == litehtml::blend_mode_normal) {
                p.setColor(make_magic_color(satoru::MagicTag::LayerPush, (int)(opacity * 255.0f)));
            } else {
                // Pack opacity (8-bit) and blend mode (4-bit) into index
                int packed = ((int)(opacity * 255.0f) << 4) | ((int)bm & 0x0F);
                p.setColor(make_magic_color(satoru::MagicTag::LayerPushBlend, packed));
            }
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        } else if (opacity < 1.0f || sk_bm != SkBlendMode::kSrcOver) {
            SkPaint paint;
            paint.setAlphaf(opacity);
            paint.setBlendMode(sk_bm);
            m_canvas->saveLayer(nullptr, &paint);
        } else {
            m_canvas->save();
        }
    }
}

void container_skia::pop_layer(litehtml::uint_ptr hdc) {
    if (m_opacity_stack.empty()) return;
    float opacity = m_opacity_stack.back();
    m_opacity_stack.pop_back();

    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::LayerPop));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        } else {
            m_canvas->restore();
        }
    }
}

void container_skia::push_transform(litehtml::uint_ptr hdc,
                                    const litehtml::css_token_vector& transform,
                                    const litehtml::css_token_vector& origin,
                                    const litehtml::position& pos) {
    if (!m_canvas) return;
    flush();

    m_canvas->save();
    m_transform_stack_depth++;

    float ox = pos.x + pos.width * 0.5f;
    float oy = pos.y + pos.height * 0.5f;

    if (!origin.empty()) {
        int n = 0;
        for (const auto& tok : origin) {
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

    for (const auto& tok : transform) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            std::vector<float> vals;
            for (const auto& arg_tok : tok.value) {
                if (arg_tok.type == litehtml::NUMBER || arg_tok.type == litehtml::DIMENSION ||
                    arg_tok.type == litehtml::PERCENTAGE) {
                    float v = arg_tok.n.number;
                    if (arg_tok.type == litehtml::PERCENTAGE) {
                        if (name.find("translate") != std::string::npos) {
                            v = v * (vals.empty() ? pos.width : pos.height) / 100.0f;
                        } else {
                            v /= 100.0f;
                        }
                    }
                    if (arg_tok.type == litehtml::DIMENSION &&
                        (litehtml::lowcase(arg_tok.unit) == "rad" ||
                         litehtml::lowcase(arg_tok.unit) == "turn")) {
                        if (litehtml::lowcase(arg_tok.unit) == "rad") {
                            v = v * 180.0f / 3.14159265f;
                        } else if (litehtml::lowcase(arg_tok.unit) == "turn") {
                            v = v * 360.0f;
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
    if (m_transform_stack_depth <= 0) return;
    m_transform_stack_depth--;

    if (m_canvas) {
        flush();
        m_canvas->restore();
    }
}

void container_skia::push_filter(litehtml::uint_ptr hdc, const litehtml::css_token_vector& filter) {
    if (!m_canvas || filter.empty()) return;
    flush();

    if (m_tagging) {
        filter_info info;
        info.tokens = filter;
        info.opacity = get_current_opacity();
        m_usedFilters.push_back(info);
        int index = (int)m_usedFilters.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::FilterPush, index));

        SkRect rect;
        if (!m_clips.empty()) {
            rect = SkRect::MakeXYWH((float)m_clips.back().first.x, (float)m_clips.back().first.y,
                                    (float)m_clips.back().first.width,
                                    (float)m_clips.back().first.height);
        } else {
            rect = SkRect::MakeWH((float)m_width, (float)m_height);
        }

        m_canvas->drawRect(rect, p);
        m_filter_stack_depth++;
        return;
    }

    sk_sp<SkImageFilter> last_filter = nullptr;

    for (const auto& filter_tok : filter) {
        if (filter_tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(filter_tok.name);
            auto args = litehtml::parse_comma_separated_list(filter_tok.value);

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
                    for (const auto& t : args[0]) {
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
        m_filter_stack_depth++;
    } else {
        m_canvas->save();
        m_filter_stack_depth++;
    }
}

void container_skia::pop_filter(litehtml::uint_ptr hdc) {
    if (m_filter_stack_depth <= 0) return;
    m_filter_stack_depth--;

    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::FilterPop));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        } else {
            m_canvas->restore();
        }
    }
}

void container_skia::push_mask(litehtml::uint_ptr hdc, const litehtml::css_token_vector& mask,
                               const litehtml::position& pos) {
    if (!m_canvas || mask.empty()) return;
    flush();

    if (m_tagging) {
        mask_info info;
        info.tokens = mask;
        info.pos = pos;
        m_usedMasks.push_back(info);
        int index = (int)m_usedMasks.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::MaskPush, index));

        SkRect rect;
        if (!m_clips.empty()) {
            rect = SkRect::MakeXYWH((float)m_clips.back().first.x, (float)m_clips.back().first.y,
                                    (float)m_clips.back().first.width,
                                    (float)m_clips.back().first.height);
        } else {
            rect = SkRect::MakeWH((float)m_width, (float)m_height);
        }

        m_canvas->drawRect(rect, p);
        m_mask_stack_depth++;
        return;
    }

    m_mask_stack.push_back({mask, pos});
    m_canvas->saveLayer(
        SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height), nullptr);
    m_mask_stack_depth++;
}

void container_skia::pop_mask(litehtml::uint_ptr hdc) {
    if (m_mask_stack_depth <= 0) return;
    m_mask_stack_depth--;

    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::MaskPop));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
            return;
        }

        auto mask_data = m_mask_stack.back();
        m_mask_stack.pop_back();

        const auto& mask_tokens = mask_data.first;
        const auto& pos = mask_data.second;

        // Start mask composite layer
        SkPaint mask_composite_paint;
        mask_composite_paint.setBlendMode(SkBlendMode::kDstIn);
        m_canvas->saveLayer(nullptr, &mask_composite_paint);

        auto layers = litehtml::parse_comma_separated_list(mask_tokens);

        for (const auto& layer_tokens : layers) {
            for (const auto& tok : layer_tokens) {
                if (tok.type == litehtml::CV_FUNCTION) {
                    std::string name = litehtml::lowcase(tok.name);
                    if (name == "url") {
                        if (!tok.value.empty()) {
                            std::string url = tok.value.front().str;
                            auto it = m_context.imageCache.find(url);
                            if (it != m_context.imageCache.end() && it->second.skImage) {
                                SkPaint p;
                                p.setAntiAlias(true);
                                m_canvas->drawImageRect(
                                    it->second.skImage,
                                    SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width,
                                                     (float)pos.height),
                                    SkSamplingOptions(SkFilterMode::kLinear), &p);
                            }
                        }
                    } else if (name == "linear-gradient" || name == "repeating-linear-gradient" ||
                               name == "radial-gradient" || name == "repeating-radial-gradient" ||
                               name == "conic-gradient" || name == "repeating-conic-gradient") {
                        litehtml::gradient g;
                        if (litehtml::parse_gradient(tok, g, nullptr)) {
                            litehtml::background_layer layer;
                            layer.origin_box = pos;
                            layer.border_box = pos;
                            layer.clip_box = pos;

                            if (name.find("linear") != std::string::npos) {
                                auto grad = litehtml::background::get_linear_gradient(g, pos);
                                if (grad) draw_linear_gradient(0, layer, *grad);
                            } else if (name.find("radial") != std::string::npos) {
                                auto grad = litehtml::background::get_radial_gradient(g, pos);
                                if (grad) draw_radial_gradient(0, layer, *grad);
                            } else {
                                auto grad = litehtml::background::get_conic_gradient(g, pos);
                                if (grad) draw_conic_gradient(0, layer, *grad);
                            }
                        }
                    }
                }
            }
        }

        m_canvas->restore();  // composite mask into content
        m_canvas->restore();  // composite content into main canvas
    }
}

void container_skia::on_unknown_property(const litehtml::string& name,
                                         const litehtml::css_token_vector& value) {
    if (name == "transition" || name == "animation" || name == "resize" ||
        name == "scrollbar-width" || name == "-webkit-box-flex" || name == "-ms-overflow-style" ||
        name == "-webkit-overflow-scrolling" || name == "-webkit-font-smoothing" ||
        name == "color-scheme" || name == "-webkit-text-size-adjust" || name == "line-break" ||
        name == "text-size-adjust") {
        return;
    }
    SATORU_LOG_WARN("Unsupported CSS property: %s: %s", name.c_str(),
                    litehtml::get_repr(value).c_str());
}

SkPath container_skia::parse_clip_path(const litehtml::css_token_vector& tokens,
                                       const litehtml::position& pos) {
    SkPathBuilder builder;
    if (tokens.empty()) return builder.detach();

    for (const auto& tok : tokens) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            auto args = litehtml::parse_comma_separated_list(tok.value);

            if (name == "circle") {
                float cx = (float)pos.x + (float)pos.width * 0.5f;
                float cy = (float)pos.y + (float)pos.height * 0.5f;
                float r = std::min((float)pos.width, (float)pos.height) * 0.5f;

                if (!args.empty()) {
                    if (!args[0].empty()) {
                        litehtml::css_length len;
                        if (len.from_token(args[0][0], litehtml::f_length_percentage)) {
                            r = len.calc_percent((float)std::sqrt((double)pos.width * pos.width +
                                                                  (double)pos.height * pos.height) /
                                                 (float)std::sqrt(2.0));
                        }
                    }
                    // "at <position>"
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (!args[i].empty() && args[i][0].ident() == "at") {
                            if (i + 1 < args.size() && !args[i + 1].empty()) {
                                litehtml::css_length lx;
                                lx.from_token(args[i + 1][0], litehtml::f_length_percentage);
                                cx = lx.calc_percent((float)pos.width) + (float)pos.x;
                                if (i + 2 < args.size() && !args[i + 2].empty()) {
                                    litehtml::css_length ly;
                                    ly.from_token(args[i + 2][0], litehtml::f_length_percentage);
                                    cy = ly.calc_percent((float)pos.height) + (float)pos.y;
                                }
                            }
                            break;
                        }
                    }
                }
                builder.addCircle(cx, cy, r);
            } else if (name == "ellipse") {
                float cx = (float)pos.x + (float)pos.width * 0.5f;
                float cy = (float)pos.y + (float)pos.height * 0.5f;
                float rx = (float)pos.width * 0.5f;
                float ry = (float)pos.height * 0.5f;

                if (!args.empty()) {
                    int at_idx = -1;
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (!args[i].empty() && args[i][0].ident() == "at") {
                            at_idx = (int)i;
                            break;
                        }
                    }

                    if (at_idx != 0 && !args[0].empty()) {
                        std::vector<litehtml::css_length> r_lengths;
                        for (const auto& t : args[0]) {
                            litehtml::css_length l;
                            if (l.from_token(t, litehtml::f_length_percentage)) {
                                r_lengths.push_back(l);
                            }
                        }
                        if (r_lengths.size() >= 2) {
                            rx = r_lengths[0].calc_percent((float)pos.width);
                            ry = r_lengths[1].calc_percent((float)pos.height);
                        }
                    }

                    if (at_idx != -1 && (size_t)at_idx + 1 < args.size()) {
                        litehtml::css_length lx;
                        if (!args[at_idx + 1].empty()) {
                            lx.from_token(args[at_idx + 1][0], litehtml::f_length_percentage);
                            cx = lx.calc_percent((float)pos.width) + (float)pos.x;
                        }
                        if ((size_t)at_idx + 2 < args.size() && !args[at_idx + 2].empty()) {
                            litehtml::css_length ly;
                            ly.from_token(args[at_idx + 2][0], litehtml::f_length_percentage);
                            cy = ly.calc_percent((float)pos.height) + (float)pos.y;
                        }
                    }
                }
                builder.addOval(SkRect::MakeLTRB(cx - rx, cy - ry, cx + rx, cy + ry));
            } else if (name == "inset") {
                float top = 0, right = 0, bottom = 0, left = 0;
                litehtml::border_radiuses b_radius;

                if (!args.empty()) {
                    std::vector<litehtml::css_length> lengths;
                    std::vector<litehtml::css_token_vector> round_args;
                    bool has_round = false;

                    for (size_t i = 0; i < args.size(); ++i) {
                        if (!args[i].empty() && args[i][0].ident() == "round") {
                            has_round = true;
                            for (size_t j = i; j < args.size(); ++j) {
                                round_args.push_back(args[j]);
                            }
                            break;
                        }
                        for (const auto& t : args[i]) {
                            litehtml::css_length l;
                            if (l.from_token(t, litehtml::f_length_percentage)) {
                                lengths.push_back(l);
                            }
                        }
                    }

                    if (lengths.size() == 1) {
                        top = right = bottom = left = lengths[0].calc_percent((float)pos.height);
                    } else if (lengths.size() == 2) {
                        top = bottom = lengths[0].calc_percent((float)pos.height);
                        right = left = lengths[1].calc_percent((float)pos.width);
                    } else if (lengths.size() == 3) {
                        top = lengths[0].calc_percent((float)pos.height);
                        right = left = lengths[1].calc_percent((float)pos.width);
                        bottom = lengths[2].calc_percent((float)pos.height);
                    } else if (lengths.size() >= 4) {
                        top = lengths[0].calc_percent((float)pos.height);
                        right = lengths[1].calc_percent((float)pos.width);
                        bottom = lengths[2].calc_percent((float)pos.height);
                        left = lengths[3].calc_percent((float)pos.width);
                    }

                    if (has_round && !round_args.empty()) {
                        std::vector<litehtml::css_length> r_lengths;
                        for (const auto& ra : round_args) {
                            for (const auto& t : ra) {
                                if (t.ident() == "round") continue;
                                litehtml::css_length l;
                                if (l.from_token(t, litehtml::f_length_percentage)) {
                                    r_lengths.push_back(l);
                                }
                            }
                        }
                        if (r_lengths.size() >= 1) {
                            float rr =
                                r_lengths[0].calc_percent((float)std::min(pos.width, pos.height));
                            b_radius.top_left_x = b_radius.top_left_y = (int)rr;
                            b_radius.top_right_x = b_radius.top_right_y = (int)rr;
                            b_radius.bottom_right_x = b_radius.bottom_right_y = (int)rr;
                            b_radius.bottom_left_x = b_radius.bottom_left_y = (int)rr;
                        }
                    }
                }
                SkRect r = SkRect::MakeLTRB((float)pos.x + left, (float)pos.y + top,
                                            (float)pos.x + (float)pos.width - right,
                                            (float)pos.y + (float)pos.height - bottom);
                if (b_radius.is_zero()) {
                    builder.addRect(r);
                } else {
                    builder.addRRect(make_rrect(litehtml::position((int)r.fLeft, (int)r.fTop,
                                                                   (int)r.width(), (int)r.height()),
                                                b_radius));
                }
            } else if (name == "polygon") {
                bool first = true;
                for (const auto& arg_tokens : args) {
                    if (arg_tokens.size() >= 2) {
                        litehtml::css_length lx, ly;
                        lx.from_token(arg_tokens[0], litehtml::f_length_percentage);
                        ly.from_token(arg_tokens[1], litehtml::f_length_percentage);
                        float px = lx.calc_percent((float)pos.width) + (float)pos.x;
                        float py = ly.calc_percent((float)pos.height) + (float)pos.y;
                        if (first) {
                            builder.moveTo(px, py);
                            first = false;
                        } else {
                            builder.lineTo(px, py);
                        }
                    }
                }
                builder.close();
            }
        }
    }
    return builder.detach();
}

void container_skia::push_clip_path(litehtml::uint_ptr hdc,
                                    const litehtml::css_token_vector& clip_path,
                                    const litehtml::position& pos) {
    if (!m_canvas || clip_path.empty()) return;
    flush();

    if (m_tagging) {
        clip_path_info info;
        info.tokens = clip_path;
        info.pos = pos;
        m_usedClipPaths.push_back(info);
        int index = (int)m_usedClipPaths.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::ClipPathPush, index));

        SkRect rect;
        if (!m_clips.empty()) {
            rect = SkRect::MakeXYWH((float)m_clips.back().first.x, (float)m_clips.back().first.y,
                                    (float)m_clips.back().first.width,
                                    (float)m_clips.back().first.height);
        } else {
            rect = SkRect::MakeWH((float)m_width, (float)m_height);
        }

        m_canvas->drawRect(rect, p);
        m_clip_path_stack_depth++;
        return;
    }

    SkPath path = container_skia::parse_clip_path(clip_path, pos);
    if (!path.isEmpty()) {
        m_canvas->save();
        m_canvas->clipPath(path, true);
        m_clip_path_stack_depth++;
    } else {
        m_canvas->save();
        m_clip_path_stack_depth++;
    }
}

void container_skia::pop_clip_path(litehtml::uint_ptr hdc) {
    if (m_clip_path_stack_depth <= 0) return;
    m_clip_path_stack_depth--;

    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::ClipPathPop));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        } else {
            m_canvas->restore();
        }
    }
}

litehtml::element::ptr container_skia::create_element(
    const char* tag_name, const litehtml::string_map& attributes,
    const std::shared_ptr<litehtml::document>& doc) {
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

std::map<font_request, std::set<char32_t>> container_skia::get_used_fonts_characters() const {
    std::map<font_request, std::set<char32_t>> res;
    for (const auto& entry : m_createdFonts) {
        auto& set = res[entry.first];
        for (auto fi : entry.second) {
            set.insert(fi->used_codepoints.begin(), fi->used_codepoints.end());
        }
    }
    return res;
}
