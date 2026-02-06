#include <emscripten/emscripten.h>
#include "satoru_context.h"
#include "svg_renderer.h"
#include "png_renderer.h"
#include "container_skia.h"
#include "litehtml.h"
#include <litehtml/master_css.h>
#include "include/core/SkCanvas.h"
#include "include/core/SkBitmap.h"
#include <cstdio>

static SatoruContext g_context;

static sk_sp<SkData> g_last_png_data;

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void init_engine() {
        g_context.init();
    }

    EMSCRIPTEN_KEEPALIVE
    void load_font(const char* name, const uint8_t* data, int size) {
        g_context.loadFont(name, data, size);
    }

    EMSCRIPTEN_KEEPALIVE
    void load_image(const char* name, const char* data_url, int width, int height) {
        g_context.loadImage(name, data_url, width, height);
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_images() {
        g_context.clearImages();
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_fonts() {
        g_context.clearFonts();
    }

    EMSCRIPTEN_KEEPALIVE
    const char* html_to_svg(const char* html, int width, int height) {
        static std::string static_out;
        static_out = renderHtmlToSvg(html, width, height, g_context);
        return static_out.c_str();
    }

    EMSCRIPTEN_KEEPALIVE
    const char* html_to_png(const char* html, int width, int height) {
        static std::string static_out;
        static_out = renderHtmlToPng(html, width, height, g_context);
        return static_out.c_str();
    }

    EMSCRIPTEN_KEEPALIVE
    const uint8_t* html_to_png_binary(const char* html, int width, int height) {
        g_last_png_data = renderHtmlToPngBinary(html, width, height, g_context);
        if (g_last_png_data) {
            return (const uint8_t*)g_last_png_data->data();
        }
        return nullptr;
    }

    EMSCRIPTEN_KEEPALIVE
    int get_png_size() {
        return g_last_png_data ? (int)g_last_png_data->size() : 0;
    }

    // New verification tool
    EMSCRIPTEN_KEEPALIVE
    uint32_t get_pixel_color(const char* html, int width, int x, int y) {
        container_skia container(width, 1000, nullptr, g_context, false);
        std::string css = litehtml::master_css;
        css += "\nbr { display: -litehtml-br !important; }\n* { box-sizing: border-box; }\n";
        
        litehtml::document::ptr doc = litehtml::document::createFromString(html, &container, css.c_str());
        if (!doc) return 0;
        doc->render(width);
        int h = (int)doc->height();
        if (h < 1) h = 1;

        SkBitmap bitmap;
        bitmap.allocN32Pixels(width, h);
        bitmap.eraseColor(SK_ColorTRANSPARENT);
        SkCanvas canvas(bitmap);
        container.set_canvas(&canvas);
        litehtml::position clip(0, 0, width, h);
        doc->draw(0, 0, 0, &clip);

        if (x < 0 || x >= width || y < 0 || y >= h) return 0;
        return (uint32_t)bitmap.getColor(x, y);
    }
}
