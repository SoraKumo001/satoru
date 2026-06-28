#include "container_skia.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string_view>

#include "bridge/magic_tags.h"
#include "container_skia_helpers.h"
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

namespace {
char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c; }

bool starts_with_ascii_ci(std::string_view s, size_t pos, const char* needle) {
    size_t needle_len = std::strlen(needle);
    if (pos + needle_len > s.size()) return false;
    for (size_t i = 0; i < needle_len; ++i) {
        if (ascii_lower(s[pos + i]) != ascii_lower(needle[i])) return false;
    }
    return true;
}

bool contains_any_ascii_ci(std::string_view s, const char* const* needles, size_t needle_count) {
    for (size_t i = 0; i < s.size(); ++i) {
        for (size_t j = 0; j < needle_count; ++j) {
            if (starts_with_ascii_ci(s, i, needles[j])) return true;
        }
    }
    return false;
}

bool looks_like_font_url(std::string_view url) {
    static constexpr const char* needles[] = {".woff2", ".woff", ".ttf", ".otf", ".ttc"};
    return contains_any_ascii_ci(url, needles, sizeof(needles) / sizeof(needles[0]));
}

void collect_text_codepoints(SatoruContext& context, const char* text,
                             std::set<char32_t>& codepoints) {
    if (!text) return;
    const char* p = text;
    auto& unicode = context.getUnicodeService();
    while (*p) {
        char32_t cp = unicode.decodeUtf8(&p);
        if (cp != 0) codepoints.insert(cp);
    }
}

std::vector<char32_t> decode_text_codepoints(SatoruContext& context, const char* text) {
    std::vector<char32_t> codepoints;
    if (!text) return codepoints;
    const char* p = text;
    auto& unicode = context.getUnicodeService();
    while (*p) {
        char32_t cp = unicode.decodeUtf8(&p);
        if (cp != 0) codepoints.push_back(cp);
    }
    return codepoints;
}
}  // namespace

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
                               ResourceManager* rm, bool tagging, litehtml::media_type media_type)
    : m_canvas(canvas),
      m_width(w),
      m_height(h),
      m_context(context),
      m_resourceManager(rm),
      m_tagging(tagging),
      m_media_type(media_type),
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
    auto profile_start = std::chrono::high_resolution_clock::time_point{};
    if (m_context.layoutProfile.enabled) {
        profile_start = std::chrono::high_resolution_clock::now();
        m_context.layoutProfile.container_create_font_count++;
    }
    SkFontStyle::Slant slant = desc.style == litehtml::font_style_normal
                                   ? SkFontStyle::kUpright_Slant
                                   : SkFontStyle::kItalic_Slant;

    std::vector<sk_sp<SkTypeface>> typefaces;
    bool fake_bold = false;
    std::vector<std::string> requestedFamilies;

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
            requestedFamilies.push_back(family);
        }
    }

    if (typefaces.empty()) {
        m_missingFonts.insert({desc.family, desc.weight, slant});
        typefaces = m_context.get_typefaces("sans-serif", desc.weight, slant, fake_bold);
    }

    font_info* fi = new font_info;
    fi->desc = desc;
    fi->fake_bold = fake_bold;
    fi->selected_font_cache.reserve(256);
    fi->glyph_width_cache.reserve(256);

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

    fi->fake_italic = false;
    if (slant == SkFontStyle::kItalic_Slant) {
        fi->fake_italic = true;
    }

    if (!fi->fonts.empty()) {
        auto typeface = fi->fonts[0]->getTypeface();
        // Use getMatchedWeight to get the INTENDED weight (handles subset fonts with broken
        // metadata)
        int actual_weight =
            m_context.fontManager.getMatchedWeight(sk_ref_sp(typeface), desc.family);

        int actual_slant = m_context.fontManager.getMatchedSlant(sk_ref_sp(typeface), desc.family);

        if (actual_slant == SkFontStyle::kItalic_Slant) {
            fi->fake_italic = false;
        }

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

    if (requestedFamilies.empty()) {
        requestedFamilies.push_back(desc.family);
    }
    for (const auto& family : requestedFamilies) {
        font_request req;
        req.family = family;
        req.weight = desc.weight;
        req.slant = slant;
        fi->requests.push_back(req);
        m_createdFonts[req].push_back(fi);
    }

    if (m_context.layoutProfile.enabled) {
        auto profile_end = std::chrono::high_resolution_clock::now();
        m_context.layoutProfile.container_create_font_ms +=
            std::chrono::duration<double, std::milli>(profile_end - profile_start).count();
    }
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
    auto profile_start = std::chrono::high_resolution_clock::time_point{};
    if (m_context.layoutProfile.enabled) {
        profile_start = std::chrono::high_resolution_clock::now();
        m_context.layoutProfile.container_text_width_count++;
        std::string profile_key;
        profile_key.reserve(strlen(text ? text : "") + 64);
        profile_key.append(std::to_string(hFont));
        profile_key.push_back('|');
        profile_key.append(std::to_string((int)dir));
        profile_key.push_back('|');
        profile_key.append(std::to_string((int)mode));
        profile_key.push_back('|');
        if (text) profile_key.append(text);
        if (m_context.layoutProfile.container_text_width_keys.insert(profile_key).second) {
            m_context.layoutProfile.container_text_width_unique_count++;
        } else {
            m_context.layoutProfile.container_text_width_duplicate_count++;
        }
    }
    font_info* fi = (font_info*)hFont;
    if (fi) {
        fi->is_rtl = (dir == litehtml::direction_rtl);
        if (m_resourceManager && !fi->requests.empty()) {
            if (fi->requests.size() == 1) {
                collect_text_codepoints(m_context, text, m_measuredFontCodepoints[fi->requests[0]]);
            } else {
                auto codepoints = decode_text_codepoints(m_context, text);
                for (const auto& req : fi->requests) {
                    auto& measured = m_measuredFontCodepoints[req];
                    measured.insert(codepoints.begin(), codepoints.end());
                }
            }
        }
    }
    auto result = satoru::TextLayout::measureText(&m_context, text, fi, mode, -1.0,
                                                  m_resourceManager ? &m_usedCodepoints : nullptr);
    if (m_context.layoutProfile.enabled) {
        auto profile_end = std::chrono::high_resolution_clock::now();
        m_context.layoutProfile.container_text_width_ms +=
            std::chrono::duration<double, std::milli>(profile_end - profile_start).count();
    }
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
        const pending_text_clip* best_tc = nullptr;
        float max_overlap_area = 0;
        for (const auto& tc : m_pending_text_clips) {
            const auto& clip = tc.layer.clip_box;
            if (actual_pos.x < clip.right() && actual_pos.right() > clip.x &&
                actual_pos.y < clip.bottom() && actual_pos.bottom() > clip.y) {
                float ix = std::max((float)actual_pos.x, (float)clip.x);
                float iy = std::max((float)actual_pos.y, (float)clip.y);
                float ir = std::min((float)actual_pos.right(), (float)clip.right());
                float ib = std::min((float)actual_pos.bottom(), (float)clip.bottom());
                float area = (ir - ix) * (ib - iy);
                if (area > max_overlap_area) {
                    max_overlap_area = area;
                    best_tc = &tc;
                }
            }
        }
        if (best_tc) {
            const auto& tc = *best_tc;
            const auto& clip = tc.layer.clip_box;
            flush();

            SkRect bounds = SkRect::MakeXYWH((float)clip.x, (float)clip.y, (float)clip.width,
                                             (float)clip.height);
            float outset_amount = std::max(50.0f, (float)fi->desc.size * 0.5f);
            bounds.outset(outset_amount, outset_amount);
            m_canvas->saveLayer(bounds, nullptr);

            // Draw text as opaque white mask
            litehtml::web_color maskColor;
            maskColor.red = 255;
            maskColor.green = 255;
            maskColor.blue = 255;
            maskColor.alpha = 255;
            satoru::TextRenderer::drawText(
                &m_context, m_canvas, text, fi, maskColor, actual_pos, overflow, dir, mode, false,
                get_current_opacity(), m_usedTextShadows, m_usedTextDraws, m_usedGlyphs,
                m_usedGlyphDraws, m_resourceManager ? &m_usedCodepoints : nullptr, m_textBatcher);
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
                satoru::TextRenderer::drawText(&m_context, nullptr, text, fi, maskColor, actual_pos,
                                               overflow, dir, mode, false, get_current_opacity(),
                                               m_usedTextShadows, m_usedTextDraws, m_usedGlyphs,
                                               m_usedGlyphDraws, &fi->used_codepoints, nullptr);
            }
            return;
        }
    }

    satoru::TextRenderer::drawText(&m_context, m_canvas, text, fi, color, actual_pos, overflow, dir,
                                   mode, m_tagging, get_current_opacity(), m_usedTextShadows,
                                   m_usedTextDraws, m_usedGlyphs, m_usedGlyphDraws,
                                   m_resourceManager ? &m_usedCodepoints : nullptr, m_textBatcher);
    std::string s(text);
    if (!s.empty() && s.find_first_not_of(" \n\r\t") != std::string::npos) {
        if (!m_clips.empty()) {
            SkRect cb = m_canvas->getLocalClipBounds();
        }
    }
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

    if (!text || !*text) return base_level;
    const unsigned char c0 = static_cast<unsigned char>(text[0]);
    if (c0 < 0x80) {
        bool is_ascii_neutral = c0 <= 0x2F || (c0 >= 0x3A && c0 <= 0x40) ||
                                (c0 >= 0x5B && c0 <= 0x60) || (c0 >= 0x7B && c0 <= 0x7F);
        if (!is_ascii_neutral) {
            int level = (base_level == 1) ? 2 : 0;
            m_last_bidi_level = level;
            return level;
        }
    } else {
        const unsigned char c1 = static_cast<unsigned char>(text[1]);
        if ((c0 >= 0xE4 && c0 <= 0xE9) ||                // CJK Unified Ideographs (U+4E00-U+9FFF)
            (c0 == 0xE3 && c1 >= 0x81 && c1 <= 0x83) ||  // Hiragana/Katakana
            (c0 == 0xE3 && c1 >= 0x90) ||                // CJK Extension A (U+3400-U+3FFF)
            (c0 >= 0xEA && c0 <= 0xED)) {                // Hangul
            int level = (base_level == 1) ? 2 : 0;
            m_last_bidi_level = level;
            return level;
        }
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
        if (looks_like_font_url(url)) {
            m_resourceManager->request(url, "", ResourceType::Font);
        } else {
            m_resourceManager->request(url, url, ResourceType::Css);
        }
    } else {
        m_context.fontManager.scanFontFaces(text);
    }
}

void container_skia::get_media_features(litehtml::media_features& features) const {
    features.type = m_media_type;
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
    auto profile_start = std::chrono::high_resolution_clock::time_point{};
    if (m_context.layoutProfile.enabled) {
        profile_start = std::chrono::high_resolution_clock::now();
        m_context.layoutProfile.container_split_text_count++;
    }
    satoru::TextLayout::splitText(&m_context, text, on_word, on_space);
    if (m_context.layoutProfile.enabled) {
        auto profile_end = std::chrono::high_resolution_clock::now();
        m_context.layoutProfile.container_split_text_ms +=
            std::chrono::duration<double, std::milli>(profile_end - profile_start).count();
    }
}

SkBlendMode container_skia::to_skia_blend_mode(litehtml::blend_mode bm) {
    switch (bm) {
        case litehtml::blend_mode_multiply:
            return SkBlendMode::kMultiply;
        case litehtml::blend_mode_screen:
            return SkBlendMode::kScreen;
        case litehtml::blend_mode_overlay:
            return SkBlendMode::kOverlay;
        case litehtml::blend_mode_darken:
            return SkBlendMode::kDarken;
        case litehtml::blend_mode_lighten:
            return SkBlendMode::kLighten;
        case litehtml::blend_mode_color_dodge:
            return SkBlendMode::kColorDodge;
        case litehtml::blend_mode_color_burn:
            return SkBlendMode::kColorBurn;
        case litehtml::blend_mode_hard_light:
            return SkBlendMode::kHardLight;
        case litehtml::blend_mode_soft_light:
            return SkBlendMode::kSoftLight;
        case litehtml::blend_mode_difference:
            return SkBlendMode::kDifference;
        case litehtml::blend_mode_exclusion:
            return SkBlendMode::kExclusion;
        case litehtml::blend_mode_hue:
            return SkBlendMode::kHue;
        case litehtml::blend_mode_saturation:
            return SkBlendMode::kSaturation;
        case litehtml::blend_mode_color:
            return SkBlendMode::kColor;
        case litehtml::blend_mode_luminosity:
            return SkBlendMode::kLuminosity;
        default:
            return SkBlendMode::kSrcOver;
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

void container_skia::collect_used_font_characters(const font_request& req,
                                                  std::vector<char32_t>& out) const {
    auto it = m_createdFonts.find(req);
    if (it == m_createdFonts.end()) return;
    for (auto fi : it->second) {
        out.insert(out.end(), fi->used_codepoints.begin(), fi->used_codepoints.end());
    }
}

void container_skia::collect_measured_font_characters(const font_request& req,
                                                      std::vector<char32_t>& out) const {
    auto it = m_measuredFontCodepoints.find(req);
    if (it == m_measuredFontCodepoints.end()) return;
    out.insert(out.end(), it->second.begin(), it->second.end());
}

const std::set<char32_t>* container_skia::get_measured_font_codepoints(
    const font_request& req) const {
    auto it = m_measuredFontCodepoints.find(req);
    if (it == m_measuredFontCodepoints.end() || it->second.empty()) return nullptr;
    return &it->second;
}
