#include <emscripten.h>
#include <emscripten/bind.h>

#include <cstring>
#include <sstream>
#include <string>
#include <regex>

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
    if (!html) return "";

    std::string svg = renderHtmlToSvg(html, width, height, g_context);

    char *result = (char *)malloc(svg.length() + 1);
    strcpy(result, svg.c_str());
    return result;
}

EMSCRIPTEN_KEEPALIVE
const char *html_to_png(const char *html, int width, int height) {
    std::string dataUrl = renderHtmlToPng(html, width, height, g_context);
    char *result = (char *)malloc(dataUrl.length() + 1);
    strcpy(result, dataUrl.c_str());
    return result;
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

// Global pointer for discovery container
container_skia *g_discovery_container = nullptr;

EMSCRIPTEN_KEEPALIVE
const char *get_required_fonts(const char *html, int width) {
    if (g_discovery_container) delete g_discovery_container;
    g_discovery_container = new container_skia(width, 1000, nullptr, g_context, false);

    // Pre-scan HTML for <style> blocks to find @font-face rules
    // litehtml might not trigger import_css for internal styles
    if (html) {
        std::string htmlStr(html);
        std::regex styleRegex("<style[^>]*>([^<]*)</style>", std::regex::icase);
        auto words_begin = std::sregex_iterator(htmlStr.begin(), htmlStr.end(), styleRegex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::string cssContent = (*i)[1].str();
            g_discovery_container->scan_font_faces(cssContent);
        }
    }

    std::string css = litehtml::master_css;
    auto doc = litehtml::document::createFromString(html, g_discovery_container, css.c_str());
    if (doc) {
        doc->render(width);
    }

    std::string result = "";
    // Pass 1: External CSS found in HTML
    for (const auto &url : g_discovery_container->get_required_css()) {
        if (!result.empty()) result += ";;";
        result += "CSS:" + url;
    }

    // Pass 2: Fonts required during layout
    for (const auto &font : g_discovery_container->get_missing_fonts()) {
        if (!result.empty()) result += ";;";
        std::string url = g_discovery_container->get_font_url(font.family, font.weight, font.slant);
        result += "FONT:" + font.family + "|" + url;
    }

    char *c_result = (char *)malloc(result.length() + 1);
    strcpy(c_result, result.c_str());
    return c_result;
}

EMSCRIPTEN_KEEPALIVE
void scan_css(const char *css) {
    if (g_discovery_container) {
        g_discovery_container->scan_font_faces(css);
    }
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