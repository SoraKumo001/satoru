#include "satoru_api.h"

#include <emscripten.h>

#include <cstring>
#include <iomanip>
#include <sstream>

#include "core/container_skia.h"
#include "core/master_css.h"
#include "core/resource_manager.h"
#include "core/satoru_context.h"
#include "renderers/pdf_renderer.h"
#include "renderers/png_renderer.h"
#include "renderers/svg_renderer.h"
#include "renderers/webp_renderer.h"

// --- Logging ---
EM_JS(void, satoru_log_js, (int level, const char *message), {
    if (Module.onLog) {
        Module.onLog(level, UTF8ToString(message));
    }
});

void satoru_log(LogLevel level, const char *message) { satoru_log_js((int)level, message); }

// --- Helpers ---
namespace {
std::string json_escape(const std::string &s) {
    std::ostringstream o;
    for (auto c : s) {
        if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f')) {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
        } else {
            o << c;
        }
    }
    return o.str();
}

typedef sk_sp<SkData> SkDataPtr;

template <typename F>
const uint8_t *render_and_store(SatoruInstance *inst, F render_func,
                                void (SatoruContext::*setter)(sk_sp<SkData>), int &out_size) {
    auto data = render_func();
    if (!data) {
        out_size = 0;
        return nullptr;
    }
    out_size = (int)data->size();
    const uint8_t *bytes = data->bytes();
    (inst->context.*setter)(std::move(data));
    return bytes;
}
}  // namespace

// --- SatoruInstance Implementation ---

SatoruInstance::SatoruInstance() : resourceManager(context) { context.init(); }

SatoruInstance::~SatoruInstance() {}

std::string SatoruInstance::get_full_master_css() const {
    return std::string(litehtml::master_css) + "\n" + satoru_master_css + "\n" +
           context.getExtraCss();
}

void SatoruInstance::init_document(const char *html, int width) {
    render_container = std::make_unique<container_skia>(width, 32767, nullptr, context,
                                                       &resourceManager, false);

    std::string css = get_full_master_css() + "\nbr { display: -litehtml-br !important; }\n";
    doc = litehtml::document::createFromString(html, render_container.get(), css.c_str());
}

void SatoruInstance::layout_document(int width) {
    if (doc) {
        doc->render(width);
        if (render_container) {
            render_container->set_height(doc->height());
        }
    }
}

static void scan_image_sizes(litehtml::element::ptr el, SatoruContext &context) {
    if (!el) return;
    const char *tag = el->get_tagName();
    if (tag && strcmp(tag, "img") == 0) {
        const char *src = el->get_attr("src");
        const char *w_str = el->get_attr("width");
        const char *h_str = el->get_attr("height");
        if (src && w_str && h_str) {
            int w = atoi(w_str);
            int h = atoi(h_str);
            if (w > 0 && h > 0) {
                int curr_w, curr_h;
                if (!context.get_image_size(src, curr_w, curr_h) || (curr_w == 0 && curr_h == 0)) {
                    context.loadImage(src, nullptr, w, h);
                }
            }
        }
    }
    for (auto &child : el->children()) {
        scan_image_sizes(child, context);
    }
}

void SatoruInstance::collect_resources(const std::string &html, int width) {
    discovery_container = std::make_unique<container_skia>(width, 32767, nullptr, context,
                                                          &resourceManager, false);

    context.fontManager.scanFontFaces(html.c_str());

    auto temp_doc = litehtml::document::createFromString(html.c_str(), discovery_container.get(),
                                                         get_full_master_css().c_str());
    if (temp_doc) {
        temp_doc->render(width);
        scan_image_sizes(temp_doc->root(), context);
    }

    const auto &usedCodepoints = discovery_container->get_used_codepoints();
    const auto &requestedAttribs = discovery_container->get_requested_font_attributes();

    for (const auto &req : requestedAttribs) {
        std::vector<std::string> urls =
            context.fontManager.getFontUrls(req.family, req.weight, req.slant, &usedCodepoints);
        for (const auto &url : urls) {
            resourceManager.request(url, req.family, ResourceType::Font);
        }
    }
}

void SatoruInstance::add_resource(const std::string &url, ResourceType type,
                                  const std::vector<uint8_t> &data) {
    resourceManager.add(url.c_str(), data.data(), (int)data.size(), type);
}

void SatoruInstance::scan_css(const std::string &css) {
    context.addCss(css.c_str());
    context.fontManager.scanFontFaces(css.c_str());
}

void SatoruInstance::load_font(const std::string &name, const std::vector<uint8_t> &data) {
    context.load_font(name.c_str(), data.data(), (int)data.size());
}

void SatoruInstance::load_image(const std::string &name, const std::string &data_url, int width,
                                int height) {
    context.load_image(name.c_str(), data_url.c_str(), width, height);
}

std::string SatoruInstance::get_pending_resources_json() {
    auto requests = resourceManager.getPendingRequests();
    if (requests.empty()) return "";

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < requests.size(); ++i) {
        const auto &req = requests[i];
        std::string typeStr = "font";
        if (req.type == ResourceType::Image)
            typeStr = "image";
        else if (req.type == ResourceType::Css)
            typeStr = "css";

        ss << "{\"url\":\"" << json_escape(req.url) << "\",\"name\":\"" << json_escape(req.name)
           << "\",\"type\":\"" << typeStr << "\",\"redraw_on_ready\":"
           << (req.redraw_on_ready ? "true" : "false") << "}";
        if (i < requests.size() - 1) ss << ",";
    }
    ss << "]";
    return ss.str();
}

// --- API Functions (Wrappers) ---

SatoruInstance *api_create_instance() { return new SatoruInstance(); }

void api_destroy_instance(SatoruInstance *inst) { delete inst; }

std::string api_html_to_svg(SatoruInstance *inst, const char *html, int width, int height,
                            const RenderOptions &options) {
    return renderHtmlToSvg(html, width, height, inst->context,
                           inst->get_full_master_css().c_str(), options);
}

const uint8_t *api_html_to_png(SatoruInstance *inst, const char *html, int width, int height,
                               const RenderOptions &options, int &out_size) {
    return render_and_store(
        inst,
        [&]() {
            return renderHtmlToPng(html, width, height, inst->context,
                                   inst->get_full_master_css().c_str());
        },
        &SatoruContext::set_last_png, out_size);
}

const uint8_t *api_html_to_webp(SatoruInstance *inst, const char *html, int width, int height,
                                const RenderOptions &options, int &out_size) {
    return render_and_store(
        inst,
        [&]() {
            return renderHtmlToWebp(html, width, height, inst->context,
                                    inst->get_full_master_css().c_str());
        },
        &SatoruContext::set_last_webp, out_size);
}

const uint8_t *api_html_to_pdf(SatoruInstance *inst, const char *html, int width, int height,
                               const RenderOptions &options, int &out_size) {
    return render_and_store(
        inst,
        [&]() {
            std::vector<std::string> htmls = {html};
            return renderHtmlsToPdf(htmls, width, height, inst->context,
                                    inst->get_full_master_css().c_str());
        },
        &SatoruContext::set_last_pdf, out_size);
}

const uint8_t *api_htmls_to_pdf(SatoruInstance *inst, const std::vector<std::string> &htmls,
                                int width, int height, const RenderOptions &options,
                                int &out_size) {
    return render_and_store(
        inst,
        [&]() {
            return renderHtmlsToPdf(htmls, width, height, inst->context,
                                    inst->get_full_master_css().c_str());
        },
        &SatoruContext::set_last_pdf, out_size);
}

const uint8_t *api_render(SatoruInstance *inst, const std::vector<std::string> &htmls, int width,
                          int height, RenderFormat format, const RenderOptions &options,
                          int &out_size) {
    if (htmls.empty()) {
        out_size = 0;
        return nullptr;
    }

    switch (format) {
        case RenderFormat::SVG: {
            std::string svg = api_html_to_svg(inst, htmls[0].c_str(), width, height, options);
            auto data = SkData::MakeWithCopy(svg.c_str(), svg.length());
            inst->context.set_last_svg(std::move(data));
            out_size = (int)inst->context.get_last_svg()->size();
            return inst->context.get_last_svg()->bytes();
        }
        case RenderFormat::PNG:
            return api_html_to_png(inst, htmls[0].c_str(), width, height, options, out_size);
        case RenderFormat::WebP:
            return api_html_to_webp(inst, htmls[0].c_str(), width, height, options, out_size);
        case RenderFormat::PDF:
            return api_htmls_to_pdf(inst, htmls, width, height, options, out_size);
    }
    out_size = 0;
    return nullptr;
}

int api_get_last_png_size(SatoruInstance *inst) { return (int)inst->context.get_last_png_size(); }
int api_get_last_webp_size(SatoruInstance *inst) { return (int)inst->context.get_last_webp_size(); }
int api_get_last_pdf_size(SatoruInstance *inst) { return (int)inst->context.get_last_pdf_size(); }
int api_get_last_svg_size(SatoruInstance *inst) { return (int)inst->context.get_last_svg_size(); }

void api_collect_resources(SatoruInstance *inst, const std::string &html, int width) {
    inst->collect_resources(html, width);
}

void api_add_resource(SatoruInstance *inst, const std::string &url, int type,
                      const std::vector<uint8_t> &data) {
    inst->add_resource(url, (ResourceType)type, data);
}

void api_scan_css(SatoruInstance *inst, const std::string &css) { inst->scan_css(css); }

void api_load_font(SatoruInstance *inst, const std::string &name,
                   const std::vector<uint8_t> &data) {
    inst->load_font(name, data);
}

void api_load_image(SatoruInstance *inst, const std::string &name, const std::string &data_url,
                    int width, int height) {
    inst->load_image(name, data_url, width, height);
}

std::string api_get_pending_resources(SatoruInstance *inst) {
    return inst->get_pending_resources_json();
}

void api_init_document(SatoruInstance *inst, const char *html, int width) {
    inst->init_document(html, width);
}

void api_layout_document(SatoruInstance *inst, int width) { inst->layout_document(width); }

const uint8_t *api_render_from_state(SatoruInstance *inst, int width, int height,
                                     RenderFormat format, const RenderOptions &options,
                                     int &out_size) {
    if (!inst->doc) {
        out_size = 0;
        return nullptr;
    }

    switch (format) {
        case RenderFormat::SVG: {
            std::string svg = renderDocumentToSvg(inst, width, height, options);
            auto data = SkData::MakeWithCopy(svg.c_str(), svg.length());
            inst->context.set_last_svg(std::move(data));
            out_size = (int)inst->context.get_last_svg()->size();
            return inst->context.get_last_svg()->bytes();
        }
        case RenderFormat::PNG:
            return render_and_store(inst, [&]() { return renderDocumentToPng(inst, width, height, options); },
                                    &SatoruContext::set_last_png, out_size);
        case RenderFormat::WebP:
            return render_and_store(inst, [&]() { return renderDocumentToWebp(inst, width, height, options); },
                                    &SatoruContext::set_last_webp, out_size);
        case RenderFormat::PDF:
            return render_and_store(inst, [&]() { return renderDocumentToPdf(inst, width, height, options); },
                                    &SatoruContext::set_last_pdf, out_size);
    }
    out_size = 0;
    return nullptr;
}
