#include <emscripten/emscripten.h>
#include "satoru_context.h"
#include "svg_renderer.h"
#include "png_renderer.h"
#include <cstdio>

static SatoruContext g_context;

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void init_engine() {
        printf("DEBUG: init_engine called\n");
        fflush(stdout);
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
}
