#include "container_skia.h"
#include "utils.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"
#include "include/core/SkShader.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkCodec.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkData.h"
#include "src/base/SkUTF.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

container_skia::container_skia(int w, int h, SkCanvas* canvas,
                             sk_sp<SkFontMgr>& fontMgr,
                             std::map<std::string, sk_sp<SkTypeface>>& typefaceCache,
                             sk_sp<SkTypeface>& defaultTypeface,
                             std::vector<sk_sp<SkTypeface>>& fallbackTypefaces,
                             std::map<std::string, image_info>& imageCache)
    : m_width(w), m_height(h), m_canvas(canvas),
      m_fontMgr(fontMgr), m_typefaceCache(typefaceCache),
      m_defaultTypeface(defaultTypeface), m_fallbackTypefaces(fallbackTypefaces), 
      m_imageCache(imageCache) {}

litehtml::uint_ptr container_skia::create_font(const litehtml::font_description& desc, const litehtml::document* doc, litehtml::font_metrics* fm) {
    std::string cleanedName = clean_font_name(desc.family.c_str());
    sk_sp<SkTypeface> typeface;
    
    auto it = m_typefaceCache.find(cleanedName);
    if (it != m_typefaceCache.end()) typeface = it->second;
    
    if (!typeface) {
        if (m_defaultTypeface) typeface = m_defaultTypeface;
        else typeface = m_fontMgr->legacyMakeTypeface(nullptr, SkFontStyle(desc.weight, SkFontStyle::kNormal_Width, (SkFontStyle::Slant)desc.style));
    }
    
    if (!typeface) return 0;

    SkFont* font = new SkFont(typeface, (float)desc.size);
    font->setSubpixel(true);
    font->setEdging(SkFont::Edging::kAntiAlias); font->setHinting(SkFontHinting::kNone); font->setLinearMetrics(true);
    
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
            for (auto& fbTypeface : m_fallbackTypefaces) {
                if (fbTypeface->unicharToGlyph(uni) != 0) {
                    targetFont.setTypeface(fbTypeface);
                    break;
                }
            }
            total_width += targetFont.measureText(&uni, sizeof(SkUnichar), SkTextEncoding::kUTF32);
        }
    }
    
    // Using a tiny constant padding (0.1px) to avoid precision-based overflow, 
    // but avoiding ceil() to prevent cumulative layout shifts in words.
    return (litehtml::pixel_t)(total_width + 0.5f);
}

void container_skia::draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) {
    if (!m_canvas || !text || !hFont) return;
    font_info* fi = (font_info*)hFont;
    SkFont* baseFont = fi->font;
    
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    
    SkFontMetrics skFm;
    baseFont->getMetrics(&skFm);
    float baseline_y = (float)pos.y - skFm.fAscent;
    float current_x = (float)pos.x;

    auto draw_text_runs = [&](float offset_x, float offset_y, const SkPaint& p) {
        const char* ptr = text;
        const char* end = text + strlen(text);
        float cx = offset_x;
        
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
                m_canvas->drawSimpleText(run_start, ptr - run_start, SkTextEncoding::kUTF8, cx, offset_y, *baseFont, p);
                cx += baseFont->measureText(run_start, ptr - run_start, SkTextEncoding::kUTF8);
            } else {
                SkUnichar uni = SkUTF::NextUTF8(&ptr, end);
                SkFont targetFont = *baseFont;
                for (auto& fbTypeface : m_fallbackTypefaces) {
                    if (fbTypeface->unicharToGlyph(uni) != 0) {
                        targetFont.setTypeface(fbTypeface);
                        break;
                    }
                }
                m_canvas->drawSimpleText(&uni, sizeof(SkUnichar), SkTextEncoding::kUTF32, cx, offset_y, targetFont, p);
                cx += targetFont.measureText(&uni, sizeof(SkUnichar), SkTextEncoding::kUTF32);
            }
        }
        return cx - offset_x;
    };

    float final_width = draw_text_runs(current_x, baseline_y, paint);

    for (const auto& shadow : fi->desc.text_shadow) {
        SkPaint shadowPaint = paint;
        shadowPaint.setColor(SkColorSetARGB(shadow.color.alpha, shadow.color.red, shadow.color.green, shadow.color.blue));
        float blur = shadow.blur.val();
        if (blur > 0) {
            shadowPaint.setImageFilter(SkImageFilters::Blur(blur, blur, nullptr));
        }
        draw_text_runs(current_x + shadow.x.val(), baseline_y + shadow.y.val(), shadowPaint);
    }

    if (fi->desc.decoration_line != litehtml::text_decoration_line_none) {
        SkPaint decPaint = paint;
        if (fi->desc.decoration_color != litehtml::web_color::current_color) {
            decPaint.setColor(SkColorSetARGB(fi->desc.decoration_color.alpha, fi->desc.decoration_color.red, fi->desc.decoration_color.green, fi->desc.decoration_color.blue));
        }
        float thickness = (float)fi->desc.decoration_thickness.val();
        if (thickness <= 0) thickness = std::max(1.0f, (float)fi->desc.size / 15.0f);
        decPaint.setStrokeWidth(thickness);
        decPaint.setStyle(SkPaint::kStroke_Style);

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
    if (!m_canvas) return;

    SkRect rect = SkRect::MakeXYWH((float)draw_pos.x, (float)draw_pos.y, (float)draw_pos.width, (float)draw_pos.height);
    SkVector radii[4] = {
        {(float)borders.radius.top_left_x, (float)borders.radius.top_left_y},
        {(float)borders.radius.top_right_x, (float)borders.radius.top_right_y},
        {(float)borders.radius.bottom_right_x, (float)borders.radius.bottom_right_y},
        {(float)borders.radius.bottom_left_x, (float)borders.radius.bottom_left_y}
    };
    SkRRect rrect;
    rrect.setRectRadii(rect, radii);

    m_canvas->save();
    m_canvas->clipRRect(rrect, true);

    if (borders.top.width == borders.bottom.width && borders.top.width == borders.left.width && borders.top.width == borders.right.width &&
        borders.top.color == borders.bottom.color && borders.top.color == borders.left.color && borders.top.color == borders.right.color &&
        borders.top.style == borders.bottom.style && borders.top.style == borders.left.style && borders.top.style == borders.right.style)
    {
        if (borders.top.width > 0 && borders.top.style != litehtml::border_style_none) {
            SkPaint p;
            p.setAntiAlias(true);
            p.setStyle(SkPaint::kStroke_Style);
            p.setStrokeWidth((float)borders.top.width * 2.0f);
            p.setColor(SkColorSetARGB(borders.top.color.alpha, borders.top.color.red, borders.top.color.green, borders.top.color.blue));
            m_canvas->drawRRect(rrect, p);
        }
    }
    else {
        auto draw_b = [&](float x1, float y1, float x2, float y2, const litehtml::border& b) {
            if (b.width > 0 && b.style != litehtml::border_style_none) {
                SkPaint p;
                p.setAntiAlias(true);
                p.setStyle(SkPaint::kStroke_Style);
                p.setStrokeWidth((float)b.width * 2.0f);
                p.setColor(SkColorSetARGB(b.color.alpha, b.color.red, b.color.green, b.color.blue));
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
    if (!m_canvas || shadows.empty()) return;

    for (const auto& shadow : shadows) {
        if (shadow.inset != inset_pass) continue;

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SkColorSetARGB(shadow.color.alpha, shadow.color.red, shadow.color.green, shadow.color.blue));
        
        float blur = (float)shadow.blur.val();
        if (blur > 0) {
            paint.setImageFilter(SkImageFilters::Blur(blur, blur, nullptr));
        }

        SkRect rect = SkRect::MakeXYWH((float)pos.x + shadow.x.val(), (float)pos.y + shadow.y.val(), (float)pos.width, (float)pos.height);
        
        SkVector radii[4] = {
            {(float)radius.top_left_x, (float)radius.top_left_y},
            {(float)radius.top_right_x, (float)radius.top_right_y},
            {(float)radius.bottom_right_x, (float)radius.bottom_right_y},
            {(float)radius.bottom_left_x, (float)radius.bottom_left_y}
        };
        SkRRect rrect;
        rrect.setRectRadii(rect, radii);

        if (shadow.inset) {
            m_canvas->save();
            SkRRect clipRRect;
            clipRRect.setRectRadii(SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height), radii);
            m_canvas->clipRRect(clipRRect, true);
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setStrokeWidth(blur * 2);
            m_canvas->drawRRect(rrect, paint);
            m_canvas->restore();
        } else {
            m_canvas->drawRRect(rrect, paint);
        }
    }
}

void container_skia::draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::linear_gradient& gradient) {
    if (!m_canvas) return;

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
    if (!m_canvas) return;

    std::vector<SkColor4f> colors;
    std::vector<float> positions;
    for (const auto& pt : gradient.color_points) {
        colors.push_back({pt.color.red/255.0f, pt.color.green/255.0f, pt.color.blue/255.0f, pt.color.alpha/255.0f});
        positions.push_back(pt.offset);
    }

    SkPoint center = {(float)gradient.position.x, (float)gradient.position.y};
    float radius = (float)gradient.radius.x;

    SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(positions), SkTileMode::kClamp), SkGradient::Interpolation());
    auto shader = SkShaders::RadialGradient(center, radius, skGrad);
    
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
    if (!m_canvas) return;
    
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
    if (!m_canvas) return;
    
    auto it = m_imageCache.find(url);
    if (it != m_imageCache.end() && it->second.image) {
        sk_sp<SkImage> img = it->second.image;
        
        SkTileMode tileX = SkTileMode::kDecal;
        SkTileMode tileY = SkTileMode::kDecal;

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

        SkMatrix matrix;
        float scaleX = (float)layer.origin_box.width / (float)img->width();
        float scaleY = (float)layer.origin_box.height / (float)img->height();
        matrix.setScaleTranslate(scaleX, scaleY, (float)layer.origin_box.x, (float)layer.origin_box.y);

        auto shader = img->makeShader(tileX, tileY, SkSamplingOptions(), matrix);
        
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
}

void container_skia::load_image(const char* src, const char* baseurl, bool redraw_on_ready) {
    if (!src) return;
    std::string s_src = src;
    if (m_imageCache.find(s_src) != m_imageCache.end()) return;

    if (s_src.substr(0, 5) == "data:") {
        size_t comma_pos = s_src.find(',');
        if (comma_pos != std::string::npos) {
            std::string base64_data = s_src.substr(comma_pos + 1);
            std::vector<uint8_t> decoded = base64_decode(base64_data);
            if (!decoded.empty()) {
                sk_sp<SkData> data = SkData::MakeWithCopy(decoded.data(), decoded.size());
                std::unique_ptr<SkCodec> codec = SkPngDecoder::Decode(data, nullptr);
                if (codec) {
                    SkBitmap bitmap;
                    bitmap.allocPixels(codec->getInfo());
                    if (codec->getPixels(codec->getInfo(), bitmap.getPixels(), bitmap.rowBytes()) == SkCodec::kSuccess) {
                        image_info info;
                        info.image = bitmap.asImage();
                        info.width = info.image->width();
                        info.height = info.image->height();
                        m_imageCache[s_src] = info;
                    }
                }
            }
        }
    }
}

void container_skia::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) {
    if (!m_canvas) return;
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
    auto it = m_imageCache.find(key);
    if (it != m_imageCache.end()) {
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
