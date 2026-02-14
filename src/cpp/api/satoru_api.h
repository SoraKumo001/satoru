#ifndef SATORU_API_H
#define SATORU_API_H

#include <cstdint>
#include <string>
#include <vector>

#include "../bridge/bridge_types.h"
#include "core/container_skia.h"
#include "core/resource_manager.h"
#include "core/satoru_context.h"

struct SatoruInstance {
    SatoruContext context;
    ResourceManager resourceManager;
    container_skia *discovery_container = nullptr;

    SatoruInstance() : resourceManager(context) { context.init(); }
    ~SatoruInstance() {
        if (discovery_container) delete discovery_container;
    }
};

SatoruInstance *api_create_instance();
void api_destroy_instance(SatoruInstance *inst);

std::string api_html_to_svg(SatoruInstance *inst, const char *html, int width, int height);
const uint8_t *api_html_to_png(SatoruInstance *inst, const char *html, int width, int height,
                               int &out_size);
const uint8_t *api_html_to_webp(SatoruInstance *inst, const char *html, int width, int height,
                                int &out_size);
const uint8_t *api_html_to_pdf(SatoruInstance *inst, const char *html, int width, int height,
                               int &out_size);
const uint8_t *api_htmls_to_pdf(SatoruInstance *inst, const std::vector<std::string> &htmls,
                                int width, int height, int &out_size);
const uint8_t *api_render(SatoruInstance *inst, const std::vector<std::string> &htmls, int width,
                          int height, RenderFormat format, int &out_size);
int api_get_last_png_size(SatoruInstance *inst);
int api_get_last_webp_size(SatoruInstance *inst);
int api_get_last_pdf_size(SatoruInstance *inst);
int api_get_last_svg_size(SatoruInstance *inst);
void api_collect_resources(SatoruInstance *inst, const char *html, int width);
void api_add_resource(SatoruInstance *inst, const std::string &url, int type,
                      const std::vector<uint8_t> &data);
void api_scan_css(SatoruInstance *inst, const std::string &css);
void api_clear_css(SatoruInstance *inst);
void api_load_font(SatoruInstance *inst, const std::string &name, const std::vector<uint8_t> &data);
void api_clear_fonts(SatoruInstance *inst);
void api_load_image(SatoruInstance *inst, const std::string &name, const std::string &data_url,
                    int width, int height);
void api_clear_images(SatoruInstance *inst);

std::string api_get_pending_resources(SatoruInstance *inst);

#endif  // SATORU_API_H
