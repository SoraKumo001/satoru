#include <emscripten.h>
#include <emscripten/bind.h>

#include <cstdlib>
#include <cstring>

#include "api/satoru_api.h"

using namespace emscripten;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void init_engine() { api_init_engine(); }

static const char *string_to_heap(const std::string &str) {
    char *res = (char *)malloc(str.length() + 1);
    std::strcpy(res, str.c_str());
    return res;
}

EMSCRIPTEN_KEEPALIVE
const char *html_to_svg(const char *html, int width, int height) {
    return string_to_heap(api_html_to_svg(html, width, height));
}

EMSCRIPTEN_KEEPALIVE
const char *html_to_png(const char *html, int width, int height) {
    return string_to_heap(api_html_to_png(html, width, height));
}

EMSCRIPTEN_KEEPALIVE
const uint8_t *html_to_png_binary(const char *html, int width, int height) {
    int size = 0;
    return api_html_to_png_binary(html, width, height, size);
}

EMSCRIPTEN_KEEPALIVE
int get_png_size() { return api_get_last_png_size(); }

EMSCRIPTEN_KEEPALIVE
const uint8_t *html_to_pdf_binary(const char *html, int width, int height) {
    int size = 0;
    return api_html_to_pdf_binary(html, width, height, size);
}

EMSCRIPTEN_KEEPALIVE
int get_pdf_size() { return api_get_last_pdf_size(); }

EMSCRIPTEN_KEEPALIVE
const char *collect_resources(const char *html, int width) {
    api_collect_resources(html, width);
    return nullptr;
}

EMSCRIPTEN_KEEPALIVE
const char *get_required_fonts(const char *html, int width) {
    collect_resources(html, width);
    return nullptr;
}


EMSCRIPTEN_KEEPALIVE
void add_resource(const char *url, int type, const uint8_t *data, int size) {
    api_add_resource(url, type, data, size);
}

EMSCRIPTEN_KEEPALIVE
void scan_css(const char *css) { api_scan_css(css); }

EMSCRIPTEN_KEEPALIVE
void load_font(const char *name, const uint8_t *data, int size) { api_load_font(name, data, size); }

EMSCRIPTEN_KEEPALIVE
void clear_fonts() { api_clear_fonts(); }

EMSCRIPTEN_KEEPALIVE
void load_image(const char *name, const char *data_url, int width, int height) {
    api_load_image(name, data_url, width, height);
}

EMSCRIPTEN_KEEPALIVE
void clear_images() { api_clear_images(); }
}

EMSCRIPTEN_BINDINGS(satoru) {
    function("init_engine", &init_engine);
    function("html_to_svg", &html_to_svg, allow_raw_pointers());
    function("html_to_png", &html_to_png, allow_raw_pointers());
    function("html_to_png_binary", &html_to_png_binary, allow_raw_pointers());
    function("get_png_size", &get_png_size);
    function("html_to_pdf_binary", &html_to_pdf_binary, allow_raw_pointers());
    function("get_pdf_size", &get_pdf_size);
    function("collect_resources", &collect_resources, allow_raw_pointers());
    function("get_required_fonts", &get_required_fonts, allow_raw_pointers());
    function("add_resource", &add_resource, allow_raw_pointers());
    function("scan_css", &scan_css, allow_raw_pointers());
    function("load_font", &load_font, allow_raw_pointers());
    function("clear_fonts", &clear_fonts);
    function("load_image", &load_image, allow_raw_pointers());
    function("clear_images", &clear_images);
}