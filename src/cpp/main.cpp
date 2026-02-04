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
#include "include/svg/SkSVGCanvas.h"
#include "include/core/SkStream.h"
#include <string>
#include <vector>
#include <map>

static sk_sp<SkFontMgr> g_fontMgr;
static std::map<std::string, sk_sp<SkTypeface>> g_typefaceCache;

class container_skia : public litehtml::document_container {
    SkCanvas* m_canvas;
    int m_width;
public:
    container_skia(int w, SkCanvas* canvas) : m_width(w), m_canvas(canvas) {}

    litehtml::uint_ptr create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override {
        sk_sp<SkTypeface> typeface;
        auto it = g_typefaceCache.find(faceName);
        if (it != g_typefaceCache.end()) {
            typeface = it->second;
        }
        
        SkFont* font = new SkFont(typeface, (float)size);
        
        if (fm && font) {
            SkFontMetrics skFm;
            font->getMetrics(&skFm);
            fm->ascent = -skFm.fAscent;
            fm->descent = skFm.fDescent;
            fm->height = -skFm.fAscent + skFm.fDescent + skFm.fLeading;
            fm->x_height = skFm.fXHeight;
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
        SkFont* font = (SkFont*)hFont;
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SkColorSetARGB(color.alpha, color.red, color.green, color.blue));
        
        SkFontMetrics skFm;
        font->getMetrics(&skFm);
        m_canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, pos.x, pos.y - skFm.fAscent, *font, paint);
    }

    void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) override {
        for(const auto& paint : bg) {
            SkPaint skPaint;
            skPaint.setColor(SkColorSetARGB(paint.color.alpha, paint.color.red, paint.color.green, paint.color.blue));
            m_canvas->drawRect(SkRect::MakeXYWH(paint.border_box.x, paint.border_box.y, paint.border_box.width, paint.border_box.height), skPaint);
        }
    }

    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override {
        auto draw_b = [&](float x1, float y1, float x2, float y2, const litehtml::border& b) {
            if (b.width > 0) {
                SkPaint p;
                p.setStyle(SkPaint::kStroke_Style);
                p.setStrokeWidth(b.width);
                p.setColor(SkColorSetARGB(b.color.alpha, b.color.red, b.color.green, b.color.blue));
                m_canvas->drawLine(x1, y1, x2, y2, p);
            }
        };
        draw_b(draw_pos.x, draw_pos.y, draw_pos.x + draw_pos.width, draw_pos.y, borders.top);
        draw_b(draw_pos.x, draw_pos.y + draw_pos.height, draw_pos.x + draw_pos.width, draw_pos.y + draw_pos.height, borders.bottom);
        draw_b(draw_pos.x, draw_pos.y, draw_pos.x, draw_pos.y + draw_pos.height, borders.left);
        draw_b(draw_pos.x + draw_pos.width, draw_pos.y, draw_pos.x + draw_pos.width, draw_pos.y + draw_pos.height, borders.right);
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
    void get_media_features(litehtml::media_features& features) const override {}
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
        g_fontMgr = SkFontMgr::RefDefault();
    }

    EMSCRIPTEN_KEEPALIVE
    void load_font(const char* name, const uint8_t* data, int size) {
        sk_sp<SkData> skData = SkData::MakeWithCopy(data, size);
        g_typefaceCache[name] = g_fontMgr->makeFromData(skData);
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_fonts() {
        g_typefaceCache.clear();
    }

    EMSCRIPTEN_KEEPALIVE
    const char* html_to_svg(const char* html, int width, int height) {
        // Measure first to get height
        {
            SkBitmap dummy;
            dummy.allocN32Pixels(width, 100);
            SkCanvas measureCanvas(dummy);
            container_skia measureContainer(width, &measureCanvas);
            litehtml::document::ptr doc = litehtml::document::createFromString(html, &measureContainer, "");
            doc->render(width);
            height = doc->height();
        }

        // Real rendering to SVG
        SkDynamicMemoryWStream stream;
        SkRect bounds = SkRect::MakeWH(width, height);
        std::unique_ptr<SkCanvas> canvas = SkSVGCanvas::Make(bounds, &stream);

        container_skia container(width, canvas.get());
        litehtml::document::ptr doc = litehtml::document::createFromString(html, &container, "");
        doc->render(width);
        
        litehtml::position clip(0, 0, width, height);
        doc->draw(0, 0, 0, &clip);

        // Delete canvas to flush SVG stream
        canvas.reset();

        static std::string svg_out;
        sk_sp<SkData> data = stream.detachAsData();
        svg_out.assign((const char*)data->data(), data->size());

        return svg_out.c_str();
    }
}
