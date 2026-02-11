#ifndef SATORU_API_H
#define SATORU_API_H

#include <cstdint>
#include <string>
#include <vector>

#include "../bridge/bridge_types.h"

void satoru_log(LogLevel level, const char *message);

struct SatoruInstance;

SatoruInstance *api_create_instance();
void api_destroy_instance(SatoruInstance *inst);

std::string api_html_to_svg(SatoruInstance *inst, const char *html, int width, int height);
const uint8_t *api_html_to_png(SatoruInstance *inst, const char *html, int width, int height,
                               int &out_size);
const uint8_t *api_html_to_pdf(SatoruInstance *inst, const char *html, int width, int height,
                               int &out_size);
int api_get_last_png_size(SatoruInstance *inst);
int api_get_last_pdf_size(SatoruInstance *inst);
void api_collect_resources(SatoruInstance *inst, const char *html, int width);
void api_add_resource(SatoruInstance *inst, const char *url, int type, const uint8_t *data,
                      int size);
void api_scan_css(SatoruInstance *inst, const char *css);
void api_clear_css(SatoruInstance *inst);
void api_load_font(SatoruInstance *inst, const char *name, const uint8_t *data, int size);
void api_clear_fonts(SatoruInstance *inst);
void api_load_image(SatoruInstance *inst, const char *name, const char *data_url, int width,
                    int height);
void api_clear_images(SatoruInstance *inst);

#endif  // SATORU_API_H
