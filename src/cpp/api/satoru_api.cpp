#include "satoru_api.h"

#include <emscripten.h>

#include <cstring>
#include <sstream>

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

const uint8_t *api_render(SatoruInstance *inst, const std::vector<std::string> &htmls, int width,
                          int height, RenderFormat format, int &out_size) {
    if (htmls.empty()) {
        out_size = 0;
        return nullptr;
    }

    switch (format) {
        case RenderFormat::SVG: {
            std::string svg = api_html_to_svg(inst, htmls[0].c_str(), width, height);
            auto data = SkData::MakeWithCopy(svg.c_str(), svg.length());
            inst->context.set_last_svg(std::move(data));
            out_size = (int)inst->context.get_last_svg()->size();
            return inst->context.get_last_svg()->bytes();
        }
        case RenderFormat::PNG:
            return api_html_to_png(inst, htmls[0].c_str(), width, height, out_size);
        case RenderFormat::WebP:
            return api_html_to_webp(inst, htmls[0].c_str(), width, height, out_size);
        case RenderFormat::PDF:
            return api_htmls_to_pdf(inst, htmls, width, height, out_size);
    }
    out_size = 0;
    return nullptr;
}

int api_get_last_png_size(SatoruInstance *inst) {
    auto data = inst->context.get_last_png();
    return data ? (int)data->size() : 0;
}

int api_get_last_webp_size(SatoruInstance *inst) {
    auto data = inst->context.get_last_webp();
    return data ? (int)data->size() : 0;
}

int api_get_last_pdf_size(SatoruInstance *inst) {
    auto data = inst->context.get_last_pdf();
    return data ? (int)data->size() : 0;
}

int api_get_last_svg_size(SatoruInstance *inst) {
    auto data = inst->context.get_last_svg();
    return data ? (int)data->size() : 0;
}

void api_collect_resources(SatoruInstance *inst, const std::string &html, int width) {
    if (inst->discovery_container) delete inst->discovery_container;
    inst->discovery_container =
        new container_skia(width, 1000, nullptr, inst->context, &inst->resourceManager, false);

    std::string master_css_full = get_full_master_css(inst);

    if (!inst->context.getExtraCss().empty()) {
        inst->context.fontManager.scanFontFaces(inst->context.getExtraCss());
    }
    inst->context.fontManager.scanFontFaces(html.c_str());

    auto doc = litehtml::document::createFromString(html.c_str(), inst->discovery_container,
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
}

void api_add_resource(SatoruInstance *inst, const std::string &url, int type,
                      const std::vector<uint8_t> &data) {
    inst->resourceManager.add(url.c_str(), data.data(), (int)data.size(), (ResourceType)type);
}

void api_scan_css(SatoruInstance *inst, const std::string &css) {
    inst->context.addCss(css.c_str());
    inst->context.fontManager.scanFontFaces(css.c_str());
}

void api_clear_css(SatoruInstance *inst) {
    inst->context.clearCss();
    inst->resourceManager.clear(ResourceType::Css);
}

void api_load_font(SatoruInstance *inst, const std::string &name, const std::vector<uint8_t> &data) {
    inst->context.load_font(name.c_str(), data.data(), (int)data.size());
}

void api_clear_fonts(SatoruInstance *inst) {
    inst->context.clearFonts();
    inst->resourceManager.clear(ResourceType::Font);
}

void api_load_image(SatoruInstance *inst, const std::string &name, const std::string &data_url,
                    int width, int height) {
    inst->context.load_image(name.c_str(), data_url.c_str(), width, height);
}

void api_clear_images(SatoruInstance *inst) {
    inst->context.clearImages();
    inst->resourceManager.clear(ResourceType::Image);
}

std::string api_get_pending_resources(SatoruInstance *inst) {
    auto requests = inst->resourceManager.getPendingRequests();
    if (requests.empty()) return "";

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < requests.size(); ++i) {
        const auto &req = requests[i];
        int typeInt = 1;
        if (req.type == ResourceType::Image) typeInt = 2;
        if (req.type == ResourceType::Css) typeInt = 3;

        ss << "{\"url\":\"" << req.url << "\",\"name\":\"" << req.name << "\",\"type\":" << typeInt
           << "}";
        if (i < requests.size() - 1) ss << ",";
    }
    ss << "]";
    return ss.str();
}
