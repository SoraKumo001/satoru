#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>
#include <vector>

#include "api/satoru_api.h"

using namespace emscripten;

extern "C" {

EMSCRIPTEN_KEEPALIVE
SatoruInstance* create_instance() { return api_create_instance(); }

EMSCRIPTEN_KEEPALIVE
void destroy_instance(SatoruInstance* inst) { api_destroy_instance(inst); }
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
val render_val(SatoruInstance* inst, val htmls, int width, int height, int format,
               val options_val) {
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
    if (options_val.hasOwnProperty("svgTextToPaths")) {
        options.svgTextToPaths = options_val["svgTextToPaths"].as<bool>();
    }
    if (options_val.hasOwnProperty("outputWidth")) {
        options.outputWidth = options_val["outputWidth"].as<int>();
    }
    if (options_val.hasOwnProperty("outputHeight")) {
        options.outputHeight = options_val["outputHeight"].as<int>();
    }
    if (options_val.hasOwnProperty("fitType")) {
        options.fitType = options_val["fitType"].as<int>();
    }
    if (options_val.hasOwnProperty("cropX")) {
        options.cropX = options_val["cropX"].as<int>();
    }
    if (options_val.hasOwnProperty("cropY")) {
        options.cropY = options_val["cropY"].as<int>();
    }
    if (options_val.hasOwnProperty("cropWidth")) {
        options.cropWidth = options_val["cropWidth"].as<int>();
    }
    if (options_val.hasOwnProperty("cropHeight")) {
        options.cropHeight = options_val["cropHeight"].as<int>();
    }
    if (options_val.hasOwnProperty("backgroundColor")) {
        options.backgroundColor = options_val["backgroundColor"].as<uint32_t>();
    }
    if (options_val.hasOwnProperty("fitPositionX")) {
        options.fitPositionX = options_val["fitPositionX"].as<float>();
    }
    if (options_val.hasOwnProperty("fitPositionY")) {
        options.fitPositionY = options_val["fitPositionY"].as<float>();
    }
    if (options_val.hasOwnProperty("mediaType")) {
        options.mediaType = options_val["mediaType"].as<int>();
    }

    int size = 0;
    const uint8_t* data =
        api_render(inst, html_vector, width, height, (RenderFormat)format, options, size);
    if (!data || size == 0) return val::null();

    return val(typed_memory_view(size, data));
}

void add_resource_val(SatoruInstance* inst, std::string url, int type, val data) {
    if (!inst) return;
    auto vec = val_to_vector(data);
    api_add_resource(inst, url, type, vec);
}

void load_font_val(SatoruInstance* inst, std::string name, val data) {
    if (!inst) return;
    auto vec = val_to_vector(data);
    api_load_font(inst, name, vec);
}

void scan_css_val(SatoruInstance* inst, std::string css) {
    if (!inst) return;
    api_scan_css(inst, css);
}

void load_image_val(SatoruInstance* inst, std::string name, std::string data_url, int width,
                    int height) {
    if (!inst) return;
    api_load_image(inst, name, data_url, width, height);
}

void load_image_pixels_val(SatoruInstance* inst, std::string name, int width, int height,
                           val pixels, std::string data_url) {
    if (!inst) return;
    auto vec = val_to_vector(pixels);
    inst->load_image_pixels(name, width, height, vec, data_url);
}

void collect_resources_val(SatoruInstance* inst, std::string html, int width, int height) {
    if (!inst) return;
    api_collect_resources(inst, html, width, height);
}

std::string get_pending_resources_val(SatoruInstance* inst) {
    if (!inst) return "";
    return api_get_pending_resources(inst);
}

void init_document_val(SatoruInstance* inst, std::string html, int width, int height) {
    if (!inst) return;
    api_init_document(inst, html.c_str(), width, height);
}

void layout_document_val(SatoruInstance* inst, int width) {
    if (!inst) return;
    api_layout_document(inst, width);
}

val render_from_state_val(SatoruInstance* inst, int width, int height, int format,
                          val options_val) {
    if (!inst) return val::null();
    RenderOptions options;
    if (options_val.hasOwnProperty("svgTextToPaths")) {
        options.svgTextToPaths = options_val["svgTextToPaths"].as<bool>();
    }
    if (options_val.hasOwnProperty("outputWidth")) {
        options.outputWidth = options_val["outputWidth"].as<int>();
    }
    if (options_val.hasOwnProperty("outputHeight")) {
        options.outputHeight = options_val["outputHeight"].as<int>();
    }
    if (options_val.hasOwnProperty("fitType")) {
        options.fitType = options_val["fitType"].as<int>();
    }
    if (options_val.hasOwnProperty("cropX")) {
        options.cropX = options_val["cropX"].as<int>();
    }
    if (options_val.hasOwnProperty("cropY")) {
        options.cropY = options_val["cropY"].as<int>();
    }
    if (options_val.hasOwnProperty("cropWidth")) {
        options.cropWidth = options_val["cropWidth"].as<int>();
    }
    if (options_val.hasOwnProperty("cropHeight")) {
        options.cropHeight = options_val["cropHeight"].as<int>();
    }
    if (options_val.hasOwnProperty("backgroundColor")) {
        options.backgroundColor = options_val["backgroundColor"].as<uint32_t>();
    }
    if (options_val.hasOwnProperty("fitPositionX")) {
        options.fitPositionX = options_val["fitPositionX"].as<float>();
    }
    if (options_val.hasOwnProperty("fitPositionY")) {
        options.fitPositionY = options_val["fitPositionY"].as<float>();
    }
    if (options_val.hasOwnProperty("mediaType")) {
        options.mediaType = options_val["mediaType"].as<int>();
    }
    int size = 0;
    const uint8_t* data =
        api_render_from_state(inst, width, height, (RenderFormat)format, options, size);
    if (!data || size == 0) return val::null();
    return val(typed_memory_view(size, data));
}

val merge_pdfs_val(SatoruInstance* inst, val pdfs) {
    if (!inst || !pdfs.isArray()) return val::null();

    unsigned len = pdfs["length"].as<unsigned>();
    std::vector<sk_sp<SkData>> pdf_vector;
    for (unsigned i = 0; i < len; ++i) {
        val pdf_val = pdfs[i];
        unsigned pdf_len = pdf_val["length"].as<unsigned>();
        std::vector<uint8_t> vec(pdf_len);
        val memoryView = val(typed_memory_view(pdf_len, vec.data()));
        memoryView.call<void>("set", pdf_val);
        pdf_vector.push_back(SkData::MakeWithCopy(vec.data(), vec.size()));
    }

    int out_size = 0;
    const uint8_t* data = api_merge_pdfs(inst, pdf_vector, out_size);
    if (!data || out_size == 0) return val::null();

    return val(typed_memory_view(out_size, data));
}

void set_font_map_val(SatoruInstance* inst, val fontMap) {
    if (!inst) return;
    std::map<std::string, std::string> map;
    val keys = val::global("Object").call<val>("keys", fontMap);
    unsigned len = keys["length"].as<unsigned>();
    for (unsigned i = 0; i < len; ++i) {
        std::string key = keys[i].as<std::string>();
        map[key] = fontMap[key].as<std::string>();
    }
    api_set_font_map(inst, map);
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
    function("load_image_pixels", &load_image_pixels_val, allow_raw_pointers());
    function("set_font_map", &set_font_map_val, allow_raw_pointers());
    function("set_log_level", &api_set_log_level);

    function("init_document", &init_document_val, allow_raw_pointers());
    function("layout_document", &layout_document_val, allow_raw_pointers());
    function("render_from_state", &render_from_state_val, allow_raw_pointers());
    function("merge_pdfs", &merge_pdfs_val, allow_raw_pointers());
}
