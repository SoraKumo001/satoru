#include "satoru_api.h"
#include "core/container_skia.h"
#include "core/resource_manager.h"
#include "core/satoru_context.h"
#include "core/master_css.h"
#include "renderers/png_renderer.h"
#include "renderers/svg_renderer.h"
#include <cstring>

SatoruContext g_context;
ResourceManager *g_resourceManager = nullptr;
container_skia *g_discovery_container = nullptr;

static std::string get_full_master_css() {
    return std::string(litehtml::master_css) + "\n" + satoru_master_css + "\n" + g_context.getExtraCss();
}

void api_init_engine() {
    g_context.init();
    if (!g_resourceManager) {
        g_resourceManager = new ResourceManager(g_context);
    }
}

std::string api_html_to_svg(const char *html, int width, int height) {
    return renderHtmlToSvg(html, width, height, g_context, get_full_master_css().c_str());
}

std::string api_html_to_png(const char *html, int width, int height) {
    return renderHtmlToPng(html, width, height, g_context, get_full_master_css().c_str());
}

const uint8_t *api_html_to_png_binary(const char *html, int width, int height, int &out_size) {
    auto data = renderHtmlToPngBinary(html, width, height, g_context, get_full_master_css().c_str());
    if (!data) {
        out_size = 0;
        return nullptr;
    }
    std::vector<uint8_t> bytes(data->size());
    memcpy(bytes.data(), data->data(), data->size());
    out_size = (int)bytes.size();
    g_context.set_last_png(std::move(bytes));
    return g_context.get_last_png().data();
}

int api_get_last_png_size() {
    return (int)g_context.get_last_png().size();
}

std::string api_collect_resources(const char *html, int width) {
    if (!g_resourceManager) {
        g_context.init();
        g_resourceManager = new ResourceManager(g_context);
    }
    if (g_discovery_container) delete g_discovery_container;
    g_discovery_container = new container_skia(width, 1000, nullptr, g_context, g_resourceManager, false);

    std::string master_css_full = get_full_master_css();

    if (!g_context.getExtraCss().empty()) {
        g_discovery_container->scan_font_faces(g_context.getExtraCss());
    }
    g_discovery_container->scan_font_faces(html);

    auto doc = litehtml::document::createFromString(html, g_discovery_container, master_css_full.c_str());
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
    return output;
}

void api_add_resource(const char *url, int type, const uint8_t *data, int size) {
    if (g_resourceManager) g_resourceManager->add(url, data, size, (ResourceType)type);
}

void api_scan_css(const char *css) {
    if (g_discovery_container) g_discovery_container->scan_font_faces(css);
}

void api_load_font(const char *name, const uint8_t *data, int size) {
    g_context.load_font(name, data, size);
}

void api_clear_fonts() {
    g_context.clear_fonts();
}

void api_load_image(const char *name, const char *data_url, int width, int height) {
    g_context.load_image(name, data_url, width, height);
}

void api_clear_images() {
    g_context.clear_images();
}
