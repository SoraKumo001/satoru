#ifndef CONTAINER_SKIA_H
#define CONTAINER_SKIA_H

#include "litehtml.h"
#include "background.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "image_types.h"
#include <map>
#include <string>
#include <vector>

class container_skia : public litehtml::document_container {
    SkCanvas* m_canvas;
    int m_width;
    int m_height;
    sk_sp<SkFontMgr>& m_fontMgr;
    std::map<std::string, sk_sp<SkTypeface>>& m_typefaceCache;
    sk_sp<SkTypeface>& m_defaultTypeface;
    std::vector<sk_sp<SkTypeface>>& m_fallbackTypefaces;
    std::map<std::string, image_info>& m_imageCache;

public:
    container_skia(int w, int h, SkCanvas* canvas,
                  sk_sp<SkFontMgr>& fontMgr,
                  std::map<std::string, sk_sp<SkTypeface>>& typefaceCache,
                  sk_sp<SkTypeface>& defaultTypeface,
                  std::vector<sk_sp<SkTypeface>>& fallbackTypefaces,
                  std::map<std::string, image_info>& imageCache);

    void set_canvas(SkCanvas* canvas) { m_canvas = canvas; }
    void set_height(int h) { m_height = h; }

    // New litehtml interface
    litehtml::uint_ptr create_font(const litehtml::font_description& desc, const litehtml::document* doc, litehtml::font_metrics* fm) override;
    void delete_font(litehtml::uint_ptr hFont) override;
    litehtml::pixel_t text_width(const char* text, litehtml::uint_ptr hFont) override;
    void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;
    
    // Background and shadows (Modern litehtml use layers)
    void draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector& shadows, const litehtml::position& pos, const litehtml::border_radiuses& radius, bool inset) override;
    void draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const std::string& url, const std::string& base_url) override;
    void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::web_color& color) override;
    void draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::linear_gradient& gradient) override;
    void draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::radial_gradient& gradient) override;
    void draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::conic_gradient& gradient) override {}

    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;

    litehtml::pixel_t pt_to_px(float pt) const override;
    litehtml::pixel_t get_default_font_size() const override;
    const char* get_default_font_name() const override;

    void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override;
    void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;

    void get_viewport(litehtml::position& viewport) const override;
    
    void set_caption(const char* caption) override {}
    void set_base_url(const char* base_url) override {}
    void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override {}
    void on_anchor_click(const char* url, const litehtml::element::ptr& el) override {}
    void on_mouse_event(const litehtml::element::ptr& el, litehtml::mouse_event event) override {}
    void set_cursor(const char* cursor) override {}
    void transform_text(litehtml::string& text, litehtml::text_transform tt) override {}
    void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override {}
    void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;
    void del_clip() override;
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override {}
    std::shared_ptr<litehtml::element> create_element(const char* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) override { return nullptr; }
    void get_media_features(litehtml::media_features& features) const override;
    void get_language(litehtml::string& language, litehtml::string& culture) const override;
};

#endif // CONTAINER_SKIA_H