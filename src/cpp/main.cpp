#include <emscripten.h>
#include <emscripten/bind.h>

#include <iostream>
#include <regex>

#include "core/container_skia.h"
#include "core/resource_manager.h"
#include "core/satoru_context.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "libs/litehtml/include/litehtml.h"
#include "renderers/png_renderer.h"
#include "renderers/svg_renderer.h"

using namespace emscripten;

SatoruContext g_context;
ResourceManager *g_resourceManager = nullptr;
container_skia *g_discovery_container = nullptr;

const char *satoru_master_css =
    "html, body { display: block; margin: 0; padding: 0; }\n"
    "p { display: block; margin: 1em 0; }\n"
    "b, strong { font-weight: bold; }\n"
    "i, em { font-style: italic; }\n"
    "u { text-decoration: underline; }\n"
    "del, s { text-decoration: line-through; }\n"
    "sup { vertical-align: super; font-size: smaller; }\n"
    "sub { vertical-align: sub; font-size: smaller; }\n"
    "h1 { display: block; font-size: 2em; font-weight: bold; margin: 0.67em 0; }\n"
    "h2 { display: block; font-size: 1.5em; font-weight: bold; margin: 0.83em 0; }\n"
    "h3 { display: block; font-size: 1.17em; font-weight: bold; margin: 1em 0; }\n"
    "h4 { display: block; font-size: 1em; font-weight: bold; margin: 1.33em 0; }\n"
    "h5 { display: block; font-size: .83em; font-weight: bold; margin: 1.67em 0; }\n"
    "h6 { display: block; font-size: .67em; font-weight: bold; margin: 2.33em 0; }\n";

extern "C" {

EMSCRIPTEN_KEEPALIVE
void init_engine() {
    g_context.init();
    if (!g_resourceManager) {
        g_resourceManager = new ResourceManager(g_context);
    }
}

EMSCRIPTEN_KEEPALIVE
const char *html_to_svg(const char *html, int width, int height) {
    std::string css = std::string(litehtml::master_css) + "\n" + satoru_master_css + "\n" +
                      g_context.getExtraCss();
    std::string result = renderHtmlToSvg(html, width, height, g_context, css.c_str());
    char *c_result = (char *)malloc(result.length() + 1);
    strcpy(c_result, result.c_str());
    return c_result;
}

EMSCRIPTEN_KEEPALIVE
const char *html_to_png(const char *html, int width, int height) {
    std::string css = std::string(litehtml::master_css) + "\n" + satoru_master_css + "\n" +
                      g_context.getExtraCss();
    std::string result = renderHtmlToPng(html, width, height, g_context, css.c_str());
    char *c_result = (char *)malloc(result.length() + 1);
    strcpy(c_result, result.c_str());
    return c_result;
}

EMSCRIPTEN_KEEPALIVE
const uint8_t *html_to_png_binary(const char *html, int width, int height) {
    std::string css = std::string(litehtml::master_css) + "\n" + satoru_master_css + "\n" +
                      g_context.getExtraCss();
    auto data = renderHtmlToPngBinary(html, width, height, g_context, css.c_str());
    if (!data) return nullptr;
    std::vector<uint8_t> bytes(data->size());
    memcpy(bytes.data(), data->data(), data->size());
    g_context.set_last_png(std::move(bytes));
    return g_context.get_last_png().data();
}

EMSCRIPTEN_KEEPALIVE
int get_png_size() { return (int)g_context.get_last_png().size(); }

EMSCRIPTEN_KEEPALIVE
const char *collect_resources(const char *html, int width) {
    if (!g_resourceManager) {
        g_context.init();
        g_resourceManager = new ResourceManager(g_context);
    }
    if (g_discovery_container) delete g_discovery_container;
    g_discovery_container =
        new container_skia(width, 1000, nullptr, g_context, g_resourceManager, false);

    std::string master_css_full = std::string(litehtml::master_css) + "\n" + satoru_master_css +
                                  "\n" + g_context.getExtraCss();

    // Scan already added CSS to the new container
    if (!g_context.getExtraCss().empty()) {
        g_discovery_container->scan_font_faces(g_context.getExtraCss());
    }

    auto doc =
        litehtml::document::createFromString(html, g_discovery_container, master_css_full.c_str());
    if (doc) doc->render(width);
    std::string output = "";
    auto requests = g_resourceManager->getPendingRequests();
    for (const auto &req : requests) {
        if (!output.empty()) output += ";;";
        int typeInt = 1;
        if (req.type == ResourceType::Image) typeInt = 2;
        if (req.type == ResourceType::Css) typeInt = 3;
        output += req.url + "|" + std::to_string(typeInt) + "|" + req.name;
    }
    char *c_result = (char *)malloc(output.length() + 1);
    strcpy(c_result, output.c_str());
    return c_result;
}

EMSCRIPTEN_KEEPALIVE
const char *get_required_fonts(const char *html, int width) {
    return collect_resources(html, width);
}

EMSCRIPTEN_KEEPALIVE
void add_resource(const char *url, int type, const uint8_t *data, int size) {
    if (g_resourceManager) g_resourceManager->add(url, data, size, (ResourceType)type);
}

EMSCRIPTEN_KEEPALIVE
void scan_css(const char *css) {
    if (g_discovery_container) g_discovery_container->scan_font_faces(css);
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