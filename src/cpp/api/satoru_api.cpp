#include "satoru_api.h"

#include <emscripten.h>

#include <cstring>

#include "core/container_skia.h"
#include "core/master_css.h"
#include "core/resource_manager.h"
#include "core/satoru_context.h"
#include "renderers/pdf_renderer.h"
#include "renderers/png_renderer.h"
#include "renderers/webp_renderer.h"
#include "renderers/svg_renderer.h"

EM_JS(void, satoru_log_js, (int level, const char *message), {
    if (Module.onLog) {
        Module.onLog(level, UTF8ToString(message));
    }
});

void satoru_log(LogLevel level, const char *message) { satoru_log_js((int)level, message); }

EM_JS(void, satoru_request_resource_js, (void *handle, const char *url, int type, const char *name),
      {
          if (Module.onRequestResource) {
              Module.onRequestResource(handle, UTF8ToString(url), type, UTF8ToString(name));
          }
      });

static std::string get_full_master_css(SatoruInstance *inst) {
    return std::string(litehtml::master_css) + "\n" + satoru_master_css + "\n" +
           inst->context.getExtraCss();
}

SatoruInstance *api_create_instance() { return new SatoruInstance(); }

void api_destroy_instance(SatoruInstance *inst) { delete inst; }

std::string api_html_to_svg(SatoruInstance *inst, const char *html, int width, int height) {
    return renderHtmlToSvg(html, width, height, inst->context, get_full_master_css(inst).c_str());
}

const uint8_t *api_html_to_png(SatoruInstance *inst, const char *html, int width, int height,
                               int &out_size) {
    auto data =
        renderHtmlToPng(html, width, height, inst->context, get_full_master_css(inst).c_str());
    if (!data) {
        out_size = 0;
        return nullptr;
    }
    out_size = (int)data->size();
    inst->context.set_last_png(std::move(data));
    return inst->context.get_last_png()->bytes();
}

const uint8_t *api_html_to_webp(SatoruInstance *inst, const char *html, int width, int height,
                                int &out_size) {
    auto data =
        renderHtmlToWebp(html, width, height, inst->context, get_full_master_css(inst).c_str());
    if (!data) {
        out_size = 0;
        return nullptr;
    }
    out_size = (int)data->size();
    inst->context.set_last_webp(std::move(data));
    return inst->context.get_last_webp()->bytes();
}

const uint8_t *api_html_to_pdf(SatoruInstance *inst, const char *html, int width, int height,
                               int &out_size) {
    std::vector<std::string> htmls;
    htmls.push_back(std::string(html));
    auto data =
        renderHtmlsToPdf(htmls, width, height, inst->context, get_full_master_css(inst).c_str());
    if (!data) {
        out_size = 0;
        return nullptr;
    }
    out_size = (int)data->size();
    inst->context.set_last_pdf(std::move(data));
    return inst->context.get_last_pdf()->bytes();
}

const uint8_t *api_htmls_to_pdf(SatoruInstance *inst, const std::vector<std::string> &htmls,
                                int width, int height, int &out_size) {
    auto data =
        renderHtmlsToPdf(htmls, width, height, inst->context, get_full_master_css(inst).c_str());
    if (!data) {
        out_size = 0;
        return nullptr;
    }
    out_size = (int)data->size();
    inst->context.set_last_pdf(std::move(data));
    return inst->context.get_last_pdf()->bytes();
}

int api_get_last_png_size(SatoruInstance *inst) {
    return (int)inst->context.get_last_png()->size();
}

int api_get_last_webp_size(SatoruInstance *inst) {
    return (int)inst->context.get_last_webp()->size();
}

int api_get_last_pdf_size(SatoruInstance *inst) {
    return (int)inst->context.get_last_pdf()->size();
}

void api_collect_resources(SatoruInstance *inst, const char *html, int width) {
    if (inst->discovery_container) delete inst->discovery_container;
    inst->discovery_container =
        new container_skia(width, 1000, nullptr, inst->context, &inst->resourceManager, false);

    std::string master_css_full = get_full_master_css(inst);

    if (!inst->context.getExtraCss().empty()) {
        inst->context.fontManager.scanFontFaces(inst->context.getExtraCss());
    }
    inst->context.fontManager.scanFontFaces(html);

    auto doc = litehtml::document::createFromString(html, inst->discovery_container,
                                                    master_css_full.c_str());
    if (doc) doc->render(width);

    const auto &usedCodepoints = inst->discovery_container->get_used_codepoints();
    const auto &requestedAttribs = inst->discovery_container->get_requested_font_attributes();

    for (const auto &req : requestedAttribs) {
        std::vector<std::string> urls = inst->context.fontManager.getFontUrls(
            req.family, req.weight, req.slant, &usedCodepoints);
        for (const auto &url : urls) {
            inst->resourceManager.request(url, req.family, ResourceType::Font);
        }
    }

    auto requests = inst->resourceManager.getPendingRequests();
    for (const auto &req : requests) {
        int typeInt = 1;
        if (req.type == ResourceType::Image) typeInt = 2;
        if (req.type == ResourceType::Css) typeInt = 3;
        satoru_request_resource_js(inst, req.url.c_str(), typeInt, req.name.c_str());
    }
}

void api_add_resource(SatoruInstance *inst, const char *url, int type, const uint8_t *data,
                      int size) {
    inst->resourceManager.add(url, data, size, (ResourceType)type);
}

void api_scan_css(SatoruInstance *inst, const char *css) {
    inst->context.addCss(css);
    inst->context.fontManager.scanFontFaces(css);
}

void api_clear_css(SatoruInstance *inst) {
    inst->context.clearCss();
    inst->resourceManager.clear(ResourceType::Css);
}

void api_load_font(SatoruInstance *inst, const char *name, const uint8_t *data, int size) {
    inst->context.load_font(name, data, size);
}

void api_clear_fonts(SatoruInstance *inst) {
    inst->context.clearFonts();
    inst->resourceManager.clear(ResourceType::Font);
}

void api_load_image(SatoruInstance *inst, const char *name, const char *data_url, int width,
                    int height) {
    inst->context.load_image(name, data_url, width, height);
}

void api_clear_images(SatoruInstance *inst) {
    inst->context.clearImages();
    inst->resourceManager.clear(ResourceType::Image);
}
