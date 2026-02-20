#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "api/satoru_api.h"

using namespace emscripten;

extern "C" {

EMSCRIPTEN_KEEPALIVE
SatoruInstance *create_instance() { return api_create_instance(); }

EMSCRIPTEN_KEEPALIVE
void destroy_instance(SatoruInstance *inst) { api_destroy_instance(inst); }
}

// Helper to convert JS Uint8Array to std::vector<uint8_t>
std::vector<uint8_t> val_to_vector(val data) {
    unsigned len = data["length"].as<unsigned>();
    std::vector<uint8_t> vec(len);
    if (len > 0) {
        val memoryView = val(typed_memory_view(len, vec.data()));
        memoryView.call<void>("set", data);
    }
    return vec;
}

// EMSCRIPTEN_BINDINGS helper
val render_val(SatoruInstance *inst, val htmls, int width, int height, int format,
               bool svgTextToPaths) {
    if (!inst) return val::null();

    std::vector<std::string> html_vector;
    if (htmls.isArray()) {
        auto l = htmls["length"].as<unsigned>();
        for (unsigned i = 0; i < l; ++i) {
            html_vector.push_back(htmls[i].as<std::string>());
        }
    } else {
        html_vector.push_back(htmls.as<std::string>());
    }

    RenderOptions options;
    options.svgTextToPaths = svgTextToPaths;

    int size = 0;
    const uint8_t *data =
        api_render(inst, html_vector, width, height, (RenderFormat)format, options, size);
    if (!data || size == 0) return val::null();

    return val(typed_memory_view(size, data));
}

void add_resource_val(SatoruInstance *inst, std::string url, int type, val data) {
    if (!inst) return;
    auto vec = val_to_vector(data);
    api_add_resource(inst, url, type, vec);
}

void load_font_val(SatoruInstance *inst, std::string name, val data) {
    if (!inst) return;
    auto vec = val_to_vector(data);
    api_load_font(inst, name, vec);
}

void scan_css_val(SatoruInstance *inst, std::string css) {
    if (!inst) return;
    api_scan_css(inst, css);
}

void load_image_val(SatoruInstance *inst, std::string name, std::string data_url, int width,
                    int height) {
    if (!inst) return;
    api_load_image(inst, name, data_url, width, height);
}

void collect_resources_val(SatoruInstance *inst, std::string html, int width) {
    if (!inst) return;
    api_collect_resources(inst, html, width);
}

std::string get_pending_resources_val(SatoruInstance *inst) {
    if (!inst) return "";
    return api_get_pending_resources(inst);
}

void init_document_val(SatoruInstance *inst, std::string html, int width) {
    if (!inst) return;
    api_init_document(inst, html.c_str(), width);
}

void layout_document_val(SatoruInstance *inst, int width) {
    if (!inst) return;
    api_layout_document(inst, width);
}

val render_from_state_val(SatoruInstance *inst, int width, int height, int format,
                          bool svgTextToPaths) {
    if (!inst) return val::null();
    RenderOptions options;
    options.svgTextToPaths = svgTextToPaths;
    int size = 0;
    const uint8_t *data =
        api_render_from_state(inst, width, height, (RenderFormat)format, options, size);
    if (!data || size == 0) return val::null();
    return val(typed_memory_view(size, data));
}

EMSCRIPTEN_BINDINGS(satoru) {
    class_<SatoruInstance>("SatoruInstance");

    function("create_instance", &create_instance, allow_raw_pointers());
    function("destroy_instance", &destroy_instance, allow_raw_pointers());
    function("render", &render_val, allow_raw_pointers());
    function("collect_resources", &collect_resources_val, allow_raw_pointers());
    function("get_pending_resources", &get_pending_resources_val, allow_raw_pointers());
    function("add_resource", &add_resource_val, allow_raw_pointers());
    function("scan_css", &scan_css_val, allow_raw_pointers());
    function("load_font", &load_font_val, allow_raw_pointers());
    function("load_image", &load_image_val, allow_raw_pointers());
    function("set_log_level", &api_set_log_level);

    function("init_document", &init_document_val, allow_raw_pointers());
    function("layout_document", &layout_document_val, allow_raw_pointers());
    function("render_from_state", &render_from_state_val, allow_raw_pointers());
}
