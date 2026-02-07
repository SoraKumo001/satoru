#include <emscripten.h>
#include <emscripten/bind.h>

#include <cstring>
#include <sstream>
#include <string>

#include "core/container_skia.h"
#include "core/satoru_context.h"
#include "litehtml.h"
#include "renderers/png_renderer.h"
#include "renderers/svg_renderer.h"

SatoruContext g_context;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void init_engine() { g_context.init(); }

EMSCRIPTEN_KEEPALIVE
const char *html_to_svg(const char *html, int width, int height) {
    std::string svg = renderHtmlToSvg(html, width, height, g_context);
    return strdup(svg.c_str());
}

EMSCRIPTEN_KEEPALIVE
const char *html_to_png(const char *html, int width, int height) {
    std::string dataUrl = renderHtmlToPng(html, width, height, g_context);
    return strdup(dataUrl.c_str());
}

EMSCRIPTEN_KEEPALIVE
const char *html_to_png_binary(const char *html, int width, int height) {
    auto data = renderHtmlToPngBinary(html, width, height, g_context);
    if (!data) return nullptr;

    std::vector<uint8_t> pngData(data->bytes(), data->bytes() + data->size());
    g_context.set_last_png(std::move(pngData));

    if (g_context.get_last_png().empty()) return nullptr;
    return (const char *)g_context.get_last_png().data();
}

EMSCRIPTEN_KEEPALIVE
int get_png_size() { return (int)g_context.get_last_png().size(); }

EMSCRIPTEN_KEEPALIVE
const char *get_required_fonts(const char *html, int width) {
    container_skia container(width, 0, nullptr, g_context);
    auto doc = litehtml::document::createFromString(html, &container);
    doc->render(width);

    const auto &missing = container.get_missing_fonts();
    std::stringstream ss;
    bool first = true;
    for (const auto &f : missing) {
        if (!first) ss << ",";
        ss << f.family;
        std::string url = container.get_font_url(f.family, f.weight, f.slant);
        if (!url.empty()) {
            ss << "|" << url;
        }
        first = false;
    }
    return strdup(ss.str().c_str());
}

EMSCRIPTEN_KEEPALIVE
void load_font(const char *name, const uint8_t *data, int size) {
    g_context.load_font(name, data, size);
}

EMSCRIPTEN_KEEPALIVE
void clear_fonts() { g_context.clear_fonts(); }

EMSCRIPTEN_KEEPALIVE
void load_image(const char *name, const char *data_url, int width, int height) {
    g_context.load_image(name, data_url, width, height);
}

EMSCRIPTEN_KEEPALIVE
void clear_images() { g_context.clear_images(); }
}
