#ifndef SATORU_API_H
#define SATORU_API_H

#include <cstdint>
#include <string>
#include <vector>

#include "../bridge/bridge_types.h"

void satoru_log(LogLevel level, const char *message);

void api_init_engine();
std::string api_html_to_svg(const char *html, int width, int height);
std::string api_html_to_png(const char *html, int width, int height);
const uint8_t *api_html_to_png_binary(const char *html, int width, int height, int &out_size);
const uint8_t *api_html_to_pdf_binary(const char *html, int width, int height, int &out_size);
int api_get_last_png_size();
int api_get_last_pdf_size();
std::string api_collect_resources(const char *html, int width);
void api_add_resource(const char *url, int type, const uint8_t *data, int size);
void api_scan_css(const char *css);
void api_load_font(const char *name, const uint8_t *data, int size);
void api_clear_fonts();
void api_load_image(const char *name, const char *data_url, int width, int height);
void api_clear_images();

#endif  // SATORU_API_H
