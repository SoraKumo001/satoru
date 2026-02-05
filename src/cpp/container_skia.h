#ifndef CONTAINER_SKIA_H
#define CONTAINER_SKIA_H

#include "litehtml.h"
#include "background.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFont.h"
#include "image_types.h"
#include "satoru_context.h"
#include <map>
#include <string>
#include <vector>
#include <tuple>

struct font_info {
    SkFont* font;
    litehtml::font_description desc;
};

struct shadow_info {
    litehtml::web_color color;
    float blur;
    float x;
    float y;
    float spread;
    bool inset;
    litehtml::position box_pos;
    litehtml::border_radiuses box_radius;

    auto as_tuple() const {
        return std::make_tuple(color.red, color.green, color.blue, color.alpha, blur, x, y, spread, inset,
                               box_pos.x, box_pos.y, box_pos.width, box_pos.height);
    }
    bool operator<(const shadow_info& other) const {
        return as_tuple() < other.as_tuple();
    }
};

struct image_draw_info {
    std::string url;
    litehtml::background_layer layer;
};

class container_skia : public litehtml::document_container {
    SkCanvas* m_canvas;
    int m_width;
    int m_height;
    SatoruContext& m_context;

    std::vector<std::string> m_usedImages;
    std::map<std::string, int> m_imageUrlToIndex;

    std::vector<shadow_info> m_usedShadows;
    std::map<shadow_info, int> m_shadowToIndex;
    std::vector<image_draw_info> m_usedImageDraws;
    bool m_tagging;

public:
    container_skia(int w, int h, SkCanvas* canvas, SatoruContext& context, bool tagging = false);

    void set_canvas(SkCanvas* canvas) { m_canvas = canvas; }
    void set_height(int h) { m_height = h; }

    size_t get_image_count() const { return m_usedImageDraws.size(); }
    const image_draw_info& get_image_draw_info(int index) const {
        if (index <= 0 || index > (int)m_usedImageDraws.size()) {
            static image_draw_info dummy;
            return dummy;
        }
        return m_usedImageDraws[index - 1];
    }

    size_t get_shadow_count() const { return m_usedShadows.size(); }
    const shadow_info& get_shadow_info(int index) const {
        if (index <= 0 || index > (int)m_usedShadows.size()) {
            static shadow_info dummy;
            return dummy;
        }
        return m_usedShadows[index - 1];
    }

    // litehtml::document_container members
    virtual litehtml::uint_ptr create_font(const litehtml::font_description& desc, const litehtml::document* doc, litehtml::font_metrics* fm) override;
    virtual void delete_font(litehtml::uint_ptr hFont) override;
    virtual litehtml::pixel_t text_width(const char* text, litehtml::uint_ptr hFont) override;
    virtual void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;

    virtual void draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector& shadows, const litehtml::position& pos, const litehtml::border_radiuses& radius, bool inset) override;
    virtual void draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const std::string& url, const std::string& base_url) override;
    virtual void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::web_color& color) override;
    virtual void draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::linear_gradient& gradient) override;
    virtual void draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::radial_gradient& gradient) override;
    virtual void draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::conic_gradient& gradient) override;
    virtual void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;

    virtual litehtml::pixel_t pt_to_px(float pt) const override;
    virtual litehtml::pixel_t get_default_font_size() const override;
    virtual const char* get_default_font_name() const override;

    virtual void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override;
    virtual void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;

    virtual void get_viewport(litehtml::position& viewport) const override;

    virtual void set_caption(const char* caption) override {}
    virtual void set_base_url(const char* base_url) override {}
    virtual void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override {}
    virtual void on_anchor_click(const char* url, const litehtml::element::ptr& el) override {}
    virtual void on_mouse_event(const litehtml::element::ptr& el, litehtml::mouse_event event) override {}
    virtual void set_cursor(const char* cursor) override {}
    virtual void transform_text(litehtml::string& text, litehtml::text_transform tt) override {}
    virtual void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override {}
    virtual void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;
    virtual void del_clip() override;
    virtual void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override {}
    virtual std::shared_ptr<litehtml::element> create_element(const char* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) override { return nullptr; }
    virtual void get_media_features(litehtml::media_features& features) const override;
    virtual void get_language(litehtml::string& language, litehtml::string& culture) const override;
};

#endif // CONTAINER_SKIA_H
