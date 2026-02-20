#ifndef SATORU_API_H
#define SATORU_API_H

#include <litehtml/document.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../bridge/bridge_types.h"
#include "core/container_skia.h"
#include "core/resource_manager.h"
#include "core/satoru_context.h"

class SatoruInstance {
   public:
    SatoruContext context;
    ResourceManager resourceManager;
    std::unique_ptr<container_skia> discovery_container;

    // Persisted state
    std::unique_ptr<container_skia> render_container;
    std::shared_ptr<litehtml::document> doc;

    SatoruInstance();
    ~SatoruInstance();

    // Core Logic
    void init_document(const char *html, int width);
    void layout_document(int width);
    void collect_resources(const std::string &html, int width);
    std::string get_full_master_css() const;

    // Resource Management
    void add_resource(const std::string &url, ResourceType type, const std::vector<uint8_t> &data);
    void scan_css(const std::string &css);
    void load_font(const std::string &name, const std::vector<uint8_t> &data);
    void load_image(const std::string &name, const std::string &data_url, int width, int height);
    std::string get_pending_resources_json();
};

SatoruInstance *api_create_instance();
void api_destroy_instance(SatoruInstance *inst);

// State Management API
void api_init_document(SatoruInstance *inst, const char *html, int width);
void api_layout_document(SatoruInstance *inst, int width);
const uint8_t *api_render_from_state(SatoruInstance *inst, int width, int height,
                                     RenderFormat format, const RenderOptions &options,
                                     int &out_size);

std::string api_html_to_svg(SatoruInstance *inst, const char *html, int width, int height,
                            const RenderOptions &options);
const uint8_t *api_html_to_png(SatoruInstance *inst, const char *html, int width, int height,
                               const RenderOptions &options, int &out_size);
const uint8_t *api_html_to_webp(SatoruInstance *inst, const char *html, int width, int height,
                                const RenderOptions &options, int &out_size);
const uint8_t *api_html_to_pdf(SatoruInstance *inst, const char *html, int width, int height,
                               const RenderOptions &options, int &out_size);
const uint8_t *api_htmls_to_pdf(SatoruInstance *inst, const std::vector<std::string> &htmls,
                                int width, int height, const RenderOptions &options, int &out_size);
const uint8_t *api_render(SatoruInstance *inst, const std::vector<std::string> &htmls, int width,
                          int height, RenderFormat format, const RenderOptions &options,
                          int &out_size);
int api_get_last_png_size(SatoruInstance *inst);
int api_get_last_webp_size(SatoruInstance *inst);
int api_get_last_pdf_size(SatoruInstance *inst);
int api_get_last_svg_size(SatoruInstance *inst);
void api_collect_resources(SatoruInstance *inst, const std::string &html, int width);
void api_add_resource(SatoruInstance *inst, const std::string &url, int type,
                      const std::vector<uint8_t> &data);
void api_scan_css(SatoruInstance *inst, const std::string &css);
void api_load_font(SatoruInstance *inst, const std::string &name, const std::vector<uint8_t> &data);
void api_load_image(SatoruInstance *inst, const std::string &name, const std::string &data_url,
                    int width, int height);

void api_set_log_level(int level);

std::string api_get_pending_resources(SatoruInstance *inst);

#endif  // SATORU_API_H
