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

// EMSCRIPTEN_BINDINGS helper
val render_val(size_t inst_ptr, val htmls, int width, int height, int format, bool svgTextToPaths) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
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

void add_resource_val(size_t inst_ptr, std::string url, int type, val data) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    auto vec = convertJSArrayToNumberVector<uint8_t>(data);
    api_add_resource(inst, url, type, vec);
}

void load_font_val(size_t inst_ptr, std::string name, val data) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    auto vec = convertJSArrayToNumberVector<uint8_t>(data);
    api_load_font(inst, name, vec);
}

void scan_css_val(size_t inst_ptr, std::string css) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    api_scan_css(inst, css);
}

void load_image_val(size_t inst_ptr, std::string name, std::string data_url, int width, int height) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    api_load_image(inst, name, data_url, width, height);
}

void collect_resources_val(size_t inst_ptr, std::string html, int width) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    api_collect_resources(inst, html, width);
}

std::string get_pending_resources_val(size_t inst_ptr) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    return api_get_pending_resources(inst);
}

void init_document_val(size_t inst_ptr, std::string html, int width) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    api_init_document(inst, html.c_str(), width);
}

void layout_document_val(size_t inst_ptr, int width) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    api_layout_document(inst, width);
}

val serialize_layout_val(size_t inst_ptr) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    int size = 0;
    const float* data = api_serialize_layout(inst, size);
    if (!data || size == 0) return val::null();
    return val(typed_memory_view(size, data));
}

void deserialize_layout_val(size_t inst_ptr, val data) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    std::vector<float> vec = convertJSArrayToNumberVector<float>(data);
    api_deserialize_layout(inst, vec.data(), (int)vec.size());
}

val render_from_state_val(size_t inst_ptr, int width, int height, int format, bool svgTextToPaths) {
    SatoruInstance *inst = (SatoruInstance *)inst_ptr;
    RenderOptions options;
    options.svgTextToPaths = svgTextToPaths;
    int size = 0;
    const uint8_t *data = api_render_from_state(inst, width, height, (RenderFormat)format, options, size);
    if (!data || size == 0) return val::null();
    return val(typed_memory_view(size, data));
}

EMSCRIPTEN_BINDINGS(satoru) {
    function("create_instance", &create_instance, allow_raw_pointers());
    function("destroy_instance", &destroy_instance, allow_raw_pointers());
    function("render", &render_val);
    function("collect_resources", &collect_resources_val);
    function("get_pending_resources", &get_pending_resources_val);
    function("add_resource", &add_resource_val);
    function("scan_css", &scan_css_val);
    function("load_font", &load_font_val);
    function("load_image", &load_image_val);
    
    function("init_document", &init_document_val);
    function("layout_document", &layout_document_val);
    function("serialize_layout", &serialize_layout_val);
    function("deserialize_layout", &deserialize_layout_val);
    function("render_from_state", &render_from_state_val);
}
