#ifndef CONTAINER_SKIA_H
#ifndef LH_TYPES_H
#include "libs/litehtml/include/litehtml/types.h"
#endif
#define CONTAINER_SKIA_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkRRect.h"
#include "include/core/SkTypeface.h"
#include "libs/litehtml/include/litehtml.h"
#include "resource_manager.h"
#include "satoru_context.h"

class container_skia : public litehtml::document_container {
    SkCanvas *m_canvas;
    int m_width;
    int m_height;
    SatoruContext &m_context;
    ResourceManager *m_resourceManager;

    std::vector<shadow_info> m_usedShadows;
    std::vector<text_shadow_info> m_usedTextShadows;
    std::vector<image_draw_info> m_usedImageDraws;
    std::vector<conic_gradient_info> m_usedConicGradients;
    std::vector<radial_gradient_info> m_usedRadialGradients;
    std::vector<linear_gradient_info> m_usedLinearGradients;

    std::set<char32_t> m_usedCodepoints;
    std::set<font_request> m_requestedFontAttributes;

    std::set<font_request> m_missingFonts;

    std::vector<std::pair<litehtml::position, litehtml::border_radiuses>> m_clips;
    std::vector<float> m_opacity_stack;

    bool m_tagging;

    float get_current_opacity() const {
        float opacity = 1.0f;
        for (float o : m_opacity_stack) {
            opacity *= o;
        }
        return opacity;
    }

   public:
    container_skia(int w, int h, SkCanvas *canvas, SatoruContext &context, ResourceManager *rm,
                   bool tagging = false);
    virtual ~container_skia() {}

    void set_canvas(SkCanvas *canvas) { m_canvas = canvas; }
    void set_height(int h) { m_height = h; }

    const std::vector<image_draw_info> &get_used_image_draws() const { return m_usedImageDraws; }
    const std::vector<conic_gradient_info> &get_used_conic_gradients() const {
        return m_usedConicGradients;
    }
    const std::vector<radial_gradient_info> &get_used_radial_gradients() const {
        return m_usedRadialGradients;
    }
    const std::vector<linear_gradient_info> &get_used_linear_gradients() const {
        return m_usedLinearGradients;
    }
    const std::vector<shadow_info> &get_used_shadows() const { return m_usedShadows; }
    const std::vector<text_shadow_info> &get_used_text_shadows() const { return m_usedTextShadows; }

    const std::set<char32_t> &get_used_codepoints() const { return m_usedCodepoints; }
    const std::set<font_request> &get_requested_font_attributes() const {
        return m_requestedFontAttributes;
    }

    const std::set<font_request> &get_missing_fonts() const { return m_missingFonts; }

    // litehtml::document_container implementations
    virtual litehtml::uint_ptr create_font(const litehtml::font_description &desc,
                                           const litehtml::document *doc,
                                           litehtml::font_metrics *fm) override;
    virtual void delete_font(litehtml::uint_ptr hFont) override;
    virtual litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr hFont) override;
    virtual void draw_text(litehtml::uint_ptr hdc, const char *text, litehtml::uint_ptr hFont,
                           litehtml::web_color color, const litehtml::position &pos,
                           litehtml::text_overflow overflow) override;
    virtual litehtml::pixel_t pt_to_px(float pt) const override;
    virtual litehtml::pixel_t get_default_font_size() const override;
    virtual const char *get_default_font_name() const override;
    virtual void draw_list_marker(litehtml::uint_ptr hdc,
                                  const litehtml::list_marker &marker) override {}
    virtual void load_image(const char *src, const char *baseurl, bool redraw_on_ready) override;
    virtual void get_image_size(const char *src, const char *baseurl, litehtml::size &sz) override;
    virtual void draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                            const std::string &url, const std::string &base_url) override;
    virtual void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                                 const litehtml::web_color &color) override;
    virtual void draw_linear_gradient(
        litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
        const litehtml::background_layer::linear_gradient &gradient) override;
    virtual void draw_radial_gradient(
        litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
        const litehtml::background_layer::radial_gradient &gradient) override;
    virtual void draw_conic_gradient(
        litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
        const litehtml::background_layer::conic_gradient &gradient) override;
    virtual void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders &borders,
                              const litehtml::position &draw_pos, bool root) override;
    virtual void draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector &shadows,
                                 const litehtml::position &pos,
                                 const litehtml::border_radiuses &radius, bool inset) override;

    virtual void set_caption(const char *caption) override {}
    virtual void set_base_url(const char *base_url) override {}
    virtual void link(const std::shared_ptr<litehtml::document> &doc,
                      const litehtml::element::ptr &el) override {}
    virtual void on_anchor_click(const char *url, const litehtml::element::ptr &el) override {}
    virtual void on_mouse_event(const litehtml::element::ptr &el,
                                litehtml::mouse_event event) override {}
    virtual void set_cursor(const char *cursor) override {}
    virtual void transform_text(litehtml::string &text, litehtml::text_transform tt) override;
    virtual void import_css(litehtml::string &text, const litehtml::string &url,
                            litehtml::string &baseurl) override;
    virtual void set_clip(const litehtml::position &pos,
                          const litehtml::border_radiuses &bdr_radius) override;
    virtual void del_clip() override;
    virtual void get_viewport(litehtml::position &viewport) const override;
    virtual void get_media_features(litehtml::media_features &features) const override;
    virtual void get_language(litehtml::string &language, litehtml::string &culture) const override;

    virtual void push_layer(litehtml::uint_ptr hdc, float opacity) override;
    virtual void pop_layer(litehtml::uint_ptr hdc) override;

    virtual litehtml::element::ptr create_element(
        const char *tag_name, const litehtml::string_map &attributes,
        const std::shared_ptr<litehtml::document> &doc) override;
};

#endif
