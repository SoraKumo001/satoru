#include "container_skia.h"
#include "utils.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"
#include <algorithm>

container_skia::container_skia(int w, int h, SkCanvas* canvas,
                             sk_sp<SkFontMgr>& fontMgr,
                             std::map<std::string, sk_sp<SkTypeface>>& typefaceCache,
                             sk_sp<SkTypeface>& defaultTypeface,
                             std::map<std::string, image_info>& imageCache)
    : m_width(w), m_height(h), m_canvas(canvas),
      m_fontMgr(fontMgr), m_typefaceCache(typefaceCache),
      m_defaultTypeface(defaultTypeface), m_imageCache(imageCache) {}

litehtml::uint_ptr container_skia::create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) {
    std::string cleanedName = clean_font_name(faceName);
    sk_sp<SkTypeface> typeface;
    auto it = m_typefaceCache.find(cleanedName);
    if (it != m_typefaceCache.end()) typeface = it->second;
    
    if (!typeface) {
        if (m_defaultTypeface) typeface = m_defaultTypeface;
        else typeface = m_fontMgr->legacyMakeTypeface(nullptr, SkFontStyle(weight, SkFontStyle::kNormal_Width, (SkFontStyle::Slant)italic));
    }
    
    if (!typeface) return 0;

    SkFont* font = new SkFont(typeface, (float)size);
    font->setSubpixel(true);
    font->setEdging(SkFont::Edging::kAntiAlias);
    
    if (fm) {
        SkFontMetrics skFm;
        font->getMetrics(&skFm);
        fm->ascent = (int)-skFm.fAscent;
        fm->descent = (int)skFm.fDescent;
        fm->height = (int)(-skFm.fAscent + skFm.fDescent + skFm.fLeading);
        fm->x_height = (int)skFm.fXHeight;
        if (fm->height <= 0) fm->height = size;
    }
    return (litehtml::uint_ptr)font;
}

void container_skia::delete_font(litehtml::uint_ptr hFont) {
    delete (SkFont*)hFont;
}

int container_skia::text_width(const char* text, litehtml::uint_ptr hFont) {
    SkFont* font = (SkFont*)hFont;
    if (!text || !font) return 0;
    return (int)font->measureText(text, strlen(text), SkTextEncoding::kUTF8);
}

void container_skia::draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) {
    if (!m_canvas) return;
    SkFont* font = (SkFont*)hFont;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
    
    SkFontMetrics skFm;
    font->getMetrics(&skFm);
    float baseline_y = (float)pos.y - skFm.fAscent;
    m_canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, (float)pos.x, baseline_y, *font, paint);
}

void container_skia::draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) {
    if (!m_canvas) return;
    for(const auto& paint : bg) {
        SkRect rect = SkRect::MakeXYWH((float)paint.border_box.x, (float)paint.border_box.y, (float)paint.border_box.width, (float)paint.border_box.height);
        
        if (paint.color.alpha > 0) {
            SkPaint skPaint;
            skPaint.setAntiAlias(true);
            skPaint.setColor(SkColorSetARGB(paint.color.alpha, paint.color.red, paint.color.green, paint.color.blue));
            
            if (paint.border_radius.top_left_x > 0 || paint.border_radius.top_right_x > 0) {
                SkVector radii[4] = {
                    {(float)paint.border_radius.top_left_x, (float)paint.border_radius.top_left_y},
                    {(float)paint.border_radius.top_right_x, (float)paint.border_radius.top_right_y},
                    {(float)paint.border_radius.bottom_right_x, (float)paint.border_radius.bottom_right_y},
                    {(float)paint.border_radius.bottom_left_x, (float)paint.border_radius.bottom_left_y}
                };
                SkRRect rrect;
                rrect.setRectRadii(rect, radii);
                m_canvas->drawRRect(rrect, skPaint);
            } else {
                m_canvas->drawRect(rect, skPaint);
            }
        }

        if (!paint.image.empty()) {
            std::string url = paint.image;
            if (url.find("url('") == 0) url = url.substr(5, url.length() - 7);
            else if (url.find("url(") == 0) url = url.substr(4, url.length() - 5);
            
            auto it = m_imageCache.find(url);
            if (it != m_imageCache.end() && it->second.image) {
                m_canvas->save();
                m_canvas->clipRect(rect);
                m_canvas->drawImageRect(it->second.image, rect, SkSamplingOptions(), nullptr);
                m_canvas->restore();
            }
        }
    }
}

void container_skia::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) {
    if (!m_canvas) return;
    auto draw_b = [&](float x1, float y1, float x2, float y2, const litehtml::border& b) {
        if (b.width > 0) {
            SkPaint p;
            p.setAntiAlias(true);
            p.setStyle(SkPaint::kStroke_Style);
            p.setStrokeWidth((float)b.width);
            p.setColor(SkColorSetARGB(b.color.alpha, b.color.red, b.color.green, b.color.blue));
            m_canvas->drawLine(x1, y1, x2, y2, p);
        }
    };
    if (borders.top.width > 0) draw_b((float)draw_pos.x, (float)draw_pos.y, (float)draw_pos.right(), (float)draw_pos.y, borders.top);
    if (borders.bottom.width > 0) draw_b((float)draw_pos.x, (float)draw_pos.bottom(), (float)draw_pos.right(), (float)draw_pos.bottom(), borders.bottom);
    if (borders.left.width > 0) draw_b((float)draw_pos.x, (float)draw_pos.y, (float)draw_pos.x, (float)draw_pos.bottom(), borders.left);
    if (borders.right.width > 0) draw_b((float)draw_pos.right(), (float)draw_pos.y, (float)draw_pos.right(), (float)draw_pos.bottom(), borders.right);
}

int container_skia::pt_to_px(int pt) const { return (int)(pt * 1.33333333f); }
int container_skia::get_default_font_size() const { return 16; }
const char* container_skia::get_default_font_name() const { return "sans-serif"; }

void container_skia::get_image_size(const char* src, const char* baseurl, litehtml::size& sz) {
    std::string key = src;
    auto it = m_imageCache.find(key);
    if (it != m_imageCache.end()) {
        sz.width = it->second.width;
        sz.height = it->second.height;
    }
}

void container_skia::get_client_rect(litehtml::position& client) const {
    client.x = 0; client.y = 0; client.width = m_width; client.height = m_height;
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

void container_skia::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) {
    if (!m_canvas) return;
    m_canvas->save();
    SkRect rect = SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height);
    m_canvas->clipRect(rect);
}

void container_skia::del_clip() {
    if (!m_canvas) return;
    m_canvas->restore();
}
