#ifndef CONTAINER_SKIA_H
#define CONTAINER_SKIA_H

#include <litehtml/litehtml.h>
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
    std::map<std::string, image_info>& m_imageCache;

public:
    container_skia(int w, int h, SkCanvas* canvas,
                  sk_sp<SkFontMgr>& fontMgr,
                  std::map<std::string, sk_sp<SkTypeface>>& typefaceCache,
                  sk_sp<SkTypeface>& defaultTypeface,
                  std::map<std::string, image_info>& imageCache);

    void set_canvas(SkCanvas* canvas) { m_canvas = canvas; }
    void set_height(int h) { m_height = h; }

    litehtml::uint_ptr create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override;
    void delete_font(litehtml::uint_ptr hFont) override;
    int text_width(const char* text, litehtml::uint_ptr hFont) override;
    void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;
    void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) override;
    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;

    int pt_to_px(int pt) const override;
    int get_default_font_size() const override;
    const char* get_default_font_name() const override;

    void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override {}
    void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;

    void set_caption(const char* caption) override {}
    void set_base_url(const char* base_url) override {}
    void on_anchor_click(const char* url, const litehtml::element::ptr& el) override {}
    void set_cursor(const char* cursor) override {}
    void transform_text(litehtml::string& text, litehtml::text_transform tt) override {}
    void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override {}
    void get_client_rect(litehtml::position& client) const override;
    litehtml::element::ptr create_element(const char* tag_name, const litehtml::string_map& attributes, const litehtml::document::ptr& doc) override { return nullptr; }
    void get_media_features(litehtml::media_features& features) const override;
    void get_language(litehtml::string& language, litehtml::string& culture) const override;
    void link(const litehtml::document::ptr& doc, const litehtml::element::ptr& el) override {}
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override {}
    void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;
    void del_clip() override;
};

#endif // CONTAINER_SKIA_H
