#include <emscripten/emscripten.h>
#include <litehtml/litehtml.h>
#include "include/core/SkCanvas.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkSurface.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"
#include "include/ports/SkFontMgr_empty.h"
#include "include/svg/SkSVGCanvas.h"
#include "include/core/SkStream.h"
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <algorithm>

static sk_sp<SkFontMgr> g_fontMgr;
static std::map<std::string, sk_sp<SkTypeface>> g_typefaceCache;
static sk_sp<SkTypeface> g_defaultTypeface;

// Stronger helper to remove all kinds of quotes and extra characters
std::string clean_font_name(const char* name) {
    if (!name) return "";
    std::string s = name;
    std::string result;
    for(char c : s) {
        if (c != '\'' && c != '"' && c != ' ' && c != '\\') {
            result += c;
        }
    }
    // Case-insensitive lookup support
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

class container_skia : public litehtml::document_container {
    SkCanvas* m_canvas;
    int m_width;
public:
    container_skia(int w, SkCanvas* canvas) : m_width(w), m_canvas(canvas) {}

    litehtml::uint_ptr create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override {
        std::string cleanedName = clean_font_name(faceName);
        
        sk_sp<SkTypeface> typeface;
        auto it = g_typefaceCache.find(cleanedName);
        if (it != g_typefaceCache.end()) {
            typeface = it->second;
        }
        
        if (!typeface) {
            if (g_defaultTypeface) {
                typeface = g_defaultTypeface;
            } else {
                typeface = g_fontMgr->legacyMakeTypeface(nullptr, SkFontStyle(weight, SkFontStyle::kNormal_Width, (SkFontStyle::Slant)italic));
            }
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

    void delete_font(litehtml::uint_ptr hFont) override {
        delete (SkFont*)hFont;
    }

    int text_width(const char* text, litehtml::uint_ptr hFont) override {
        SkFont* font = (SkFont*)hFont;
        if (!text || !font) return 0;
        return (int)font->measureText(text, strlen(text), SkTextEncoding::kUTF8);
    }

    void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override {
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

    void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) override {
        if (!m_canvas) return;
        for(const auto& paint : bg) {
            SkPaint skPaint;
            skPaint.setAntiAlias(true);
            skPaint.setColor(SkColorSetARGB(paint.color.alpha, paint.color.red, paint.color.green, paint.color.blue));
            
            SkRect rect = SkRect::MakeXYWH((float)paint.border_box.x, (float)paint.border_box.y, (float)paint.border_box.width, (float)paint.border_box.height);
            
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
    }

    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override {
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

    int pt_to_px(int pt) const override { return pt; }
    int get_default_font_size() const override { return 16; }
    const char* get_default_font_name() const override { return "sans-serif"; }
    void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override {}
    void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override {}
    void set_caption(const char* caption) override {}
    void set_base_url(const char* base_url) override {}
    void on_anchor_click(const char* url, const litehtml::element::ptr& el) override {}
    void set_cursor(const char* cursor) override {}
    void transform_text(litehtml::string& text, litehtml::text_transform tt) override {}
    void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override {}
    void get_client_rect(litehtml::position& client) const override { client.width = m_width; client.height = 10000; }
    litehtml::element::ptr create_element(const char* tag_name, const litehtml::string_map& attributes, const litehtml::document::ptr& doc) override { return nullptr; }
    void get_media_features(litehtml::media_features& features) const override {
        features.type = litehtml::media_type_screen;
        features.width = m_width;
        features.height = 10000;
        features.device_width = m_width;
        features.device_height = 10000;
        features.color = 8;
        features.monochrome = 0;
        features.color_index = 256;
        features.resolution = 96;
    }
    void get_language(litehtml::string& language, litehtml::string& culture) const override {}
    void link(const litehtml::document::ptr& doc, const litehtml::element::ptr& el) override {}
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override {}
    void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override {}
    void del_clip() override {}
};

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void init_engine() {
        SkGraphics::Init();
        g_fontMgr = SkFontMgr_New_Custom_Empty();
    }

    EMSCRIPTEN_KEEPALIVE
    void load_font(const char* name, const uint8_t* data, int size) {
        std::string cleanedName = clean_font_name(name);
        sk_sp<SkData> skData = SkData::MakeWithCopy(data, size);
        sk_sp<SkTypeface> typeface = g_fontMgr->makeFromData(skData);
        if (typeface) {
            g_typefaceCache[cleanedName] = typeface;
            if (!g_defaultTypeface) g_defaultTypeface = typeface;
        }
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_fonts() {
        g_typefaceCache.clear();
        g_defaultTypeface = nullptr;
    }

    EMSCRIPTEN_KEEPALIVE
    const char* html_to_svg(const char* html, int width, int height) {
        const char* default_css = "html,body { margin: 0; padding: 0; display: block; } div { box-sizing: border-box; }";
        
        container_skia measureContainer(width, nullptr);
        litehtml::document::ptr doc = litehtml::document::createFromString(html, &measureContainer, default_css);
        doc->render(width);
        
        int content_height = (height > 0) ? height : doc->height();
        if (content_height < 10) content_height = 500;

        SkDynamicMemoryWStream stream;
        SkRect bounds = SkRect::MakeWH((float)width, (float)content_height);
        std::unique_ptr<SkCanvas> canvas = SkSVGCanvas::Make(bounds, &stream);

        container_skia container(width, canvas.get());
        doc = litehtml::document::createFromString(html, &container, default_css);
        doc->render(width);
        
        litehtml::position clip(0, 0, width, content_height);
        doc->draw(0, 0, 0, &clip);

        canvas.reset();

        static std::string svg_out;
        sk_sp<SkData> data = stream.detachAsData();
        svg_out.assign((const char*)data->data(), data->size());

        return svg_out.c_str();
    }
}
