#ifndef SATORU_API_H
#define SATORU_API_H

#include <litehtml/document.h>

#include <cstdarg>
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

    // Persisted state
    std::unique_ptr<container_skia> render_container;
    std::shared_ptr<litehtml::document> doc;
    std::string last_parsed_html;
    size_t last_extra_css_size = 0;
    uint64_t last_css_version = 0;
    uint64_t last_font_version = 0;
    uint64_t last_image_version = 0;
    int last_width = -1;
    int last_height = -1;
    int last_media_type = -1;
    bool image_sizes_scanned = false;
    bool needs_relayout = false;
    std::string last_font_face_scan_html;
    std::string cached_full_master_css;
    std::vector<uint8_t> pending_resources_buffer;
    double profile_scan_font_faces_ms = 0.0;
    double profile_create_document_ms = 0.0;
    double profile_render_layout_ms = 0.0;
    double profile_scan_image_sizes_ms = 0.0;
    double profile_font_requests_ms = 0.0;
    int profile_requested_font_count = 0;
    int profile_font_url_count = 0;
    int profile_font_requests_with_urls_count = 0;
    int profile_provider_font_request_count = 0;
    int profile_mapped_font_url_request_count = 0;
    int profile_font_character_count = 0;
    int profile_rebuild_count = 0;
    int profile_rebuild_initial_count = 0;
    int profile_rebuild_html_count = 0;
    int profile_rebuild_css_count = 0;
    int profile_rebuild_media_count = 0;
    int profile_layout_count = 0;
    int profile_layout_size_count = 0;
    int profile_layout_relayout_count = 0;
    bool collect_profile_enabled = false;

    SatoruInstance();
    ~SatoruInstance();

    // Core Logic
    void init_document(const char *html, int width, int height);
    void layout_document(int width);
    void collect_resources(const std::string &html, int width, int height, int mediaType = 0);
    const std::string &get_full_master_css() const;
    std::string get_collect_profile_json() const;
    void set_collect_profile_enabled(bool enabled) { collect_profile_enabled = enabled; }

    // Resource Management
    void add_resource(const std::string &url, ResourceType type, const std::vector<uint8_t> &data);
    void scan_css(const std::string &css);
    void load_font(const std::string &name, const std::vector<uint8_t> &data);
    void load_image(const std::string &name, const std::string &data_url, int width, int height);
    void load_image_pixels(const std::string &name, int width, int height,
                           const std::vector<uint8_t> &pixels, const std::string &data_url);
    std::string get_pending_resources_json();
    const uint8_t *get_pending_resources_binary(int &out_size);
};

SatoruInstance *api_create_instance();
void api_destroy_instance(SatoruInstance *inst);

// State Management API
void api_init_document(SatoruInstance *inst, const char *html, int width, int height);
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
const uint8_t *api_merge_pdfs(SatoruInstance *inst, const std::vector<sk_sp<SkData>> &pdfs,
                              int &out_size);
int api_get_last_png_size(SatoruInstance *inst);
int api_get_last_webp_size(SatoruInstance *inst);
int api_get_last_pdf_size(SatoruInstance *inst);
int api_get_last_svg_size(SatoruInstance *inst);
void api_collect_resources(SatoruInstance *inst, const std::string &html, int width, int height,
                           int mediaType = 0);
std::string api_get_collect_profile(SatoruInstance *inst);
void api_set_collect_profile_enabled(SatoruInstance *inst, bool enabled);
void api_add_resource(SatoruInstance *inst, const std::string &url, int type,
                      const std::vector<uint8_t> &data);
void api_scan_css(SatoruInstance *inst, const std::string &css);
void api_load_font(SatoruInstance *inst, const std::string &name, const std::vector<uint8_t> &data);
void api_load_fallback_font(SatoruInstance *inst, const std::vector<uint8_t> &data);
void api_load_image(SatoruInstance *inst, const std::string &name, const std::string &data_url,
                    int width, int height);
void api_set_font_map(SatoruInstance *inst, const std::map<std::string, std::string> &fontMap);

void api_set_log_level(int level);

void satoru_log(LogLevel level, const char *message);
void satoru_log_printf(LogLevel level, const char *format, ...);

std::string api_get_pending_resources(SatoruInstance *inst);
const uint8_t *api_get_pending_resources_binary(SatoruInstance *inst, int &out_size);

#endif  // SATORU_API_H
