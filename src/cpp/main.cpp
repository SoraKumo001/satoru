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

EMSCRIPTEN_KEEPALIVE
const char *collect_resources(SatoruInstance *inst, const char *html, int width) {
    api_collect_resources(inst, html, width);
    return nullptr;
}

EMSCRIPTEN_KEEPALIVE
const char *get_pending_resources(SatoruInstance *inst) {
    auto json = api_get_pending_resources(inst);
    char *res = (char *)malloc(json.length() + 1);
    std::strcpy(res, json.c_str());
    return res;
}

EMSCRIPTEN_KEEPALIVE
void add_resource(SatoruInstance *inst, const char *url, int type, const uint8_t *data, int size) {
    api_add_resource(inst, url, type, data, size);
}

EMSCRIPTEN_KEEPALIVE
void scan_css(SatoruInstance *inst, const char *css) { api_scan_css(inst, css); }

EMSCRIPTEN_KEEPALIVE
void clear_css(SatoruInstance *inst) { api_clear_css(inst); }

EMSCRIPTEN_KEEPALIVE
void load_font(SatoruInstance *inst, const char *name, const uint8_t *data, int size) {
    api_load_font(inst, name, data, size);
}

EMSCRIPTEN_KEEPALIVE
void clear_fonts(SatoruInstance *inst) { api_clear_fonts(inst); }

EMSCRIPTEN_KEEPALIVE
void load_image(SatoruInstance *inst, const char *name, const char *data_url, int width,
                int height) {
    api_load_image(inst, name, data_url, width, height);
}

EMSCRIPTEN_KEEPALIVE
void clear_images(SatoruInstance *inst) { api_clear_images(inst); }
}

// EMSCRIPTEN_BINDINGS helper
val render_val(size_t inst_ptr, val htmls, int width, int height, int format) {
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

    int size = 0;
    const uint8_t *data =
        api_render(inst, html_vector, width, height, (RenderFormat)format, size);
    if (!data || size == 0) return val::null();

    return val(typed_memory_view(size, data));
}

EMSCRIPTEN_BINDINGS(satoru) {
    function("create_instance", &create_instance, allow_raw_pointers());
    function("destroy_instance", &destroy_instance, allow_raw_pointers());
    function("render", &render_val);
    function("collect_resources", &collect_resources, allow_raw_pointers());
    function("get_pending_resources", &get_pending_resources, allow_raw_pointers());
    function("add_resource", &add_resource, allow_raw_pointers());
    function("scan_css", &scan_css, allow_raw_pointers());
    function("clear_css", &clear_css, allow_raw_pointers());
    function("load_font", &load_font, allow_raw_pointers());
    function("clear_fonts", &clear_fonts, allow_raw_pointers());
    function("load_image", &load_image, allow_raw_pointers());
    function("clear_images", &clear_images, allow_raw_pointers());
}
