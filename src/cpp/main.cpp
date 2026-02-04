#include <emscripten/emscripten.h>
#include <litehtml/litehtml.h>
#include <string>
#include <vector>
#include <iostream>
#include <map>

struct font_data {
    std::vector<unsigned char> data;
};
static std::map<std::string, font_data> g_font_cache;

class container_minimal : public litehtml::document_container {
public:
    litehtml::uint_ptr create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override {
        if (fm) {
            fm->ascent = size;
            fm->descent = size / 4;
            fm->height = fm->ascent + fm->descent;
            fm->x_height = size / 2;
        }
        return (litehtml::uint_ptr)1;
    }
    void delete_font(litehtml::uint_ptr hFont) override {}
    int text_width(const char* text, litehtml::uint_ptr hFont) override { return strlen(text) * 10; }
    void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override {}
    int pt_to_px(int pt) const override { return pt; }
    int get_default_font_size() const override { return 16; }
    const char* get_default_font_name() const override { return "serif"; }
    void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override {}
    void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override {}
    void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) override {}
    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override {}
    void set_caption(const char* caption) override {}
    void set_base_url(const char* base_url) override {}
    void on_anchor_click(const char* url, const litehtml::element::ptr& el) override {}
    void set_cursor(const char* cursor) override {}
    void transform_text(litehtml::string& text, litehtml::text_transform tt) override {}
    void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override {}
    void get_client_rect(litehtml::position& client) const override {
        client.width = 800;
        client.height = 600;
    }
    litehtml::element::ptr create_element(const char* tag_name, const litehtml::string_map& attributes, const litehtml::document::ptr& doc) override { return nullptr; }
    void get_media_features(litehtml::media_features& features) const override {}
    void get_language(litehtml::string& language, litehtml::string& culture) const override {}
    void link(const litehtml::document::ptr& doc, const litehtml::element::ptr& el) override {}
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override {}
    void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override {}
    void del_clip() override {}
};

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void load_font(const char* name, const uint8_t* data, int size) {
        g_font_cache[name].data.assign(data, data + size);
        std::cout << "[Wasm] Font loaded: " << name << " (" << size << " bytes). Total fonts: " << g_font_cache.size() << std::endl;
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_fonts() {
        g_font_cache.clear();
        std::cout << "[Wasm] All fonts cleared." << std::endl;
    }

    EMSCRIPTEN_KEEPALIVE
    const char* html_to_svg(const char* html) {
        container_minimal container;
        litehtml::document::ptr doc = litehtml::document::createFromString(html, &container, "");
        doc->render(800); 

        static std::string svg_output;
        svg_output = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\">";
        svg_output += "<foreignObject width=\"100%\" height=\"100%\">";
        svg_output += "<div xmlns=\"http://www.w3.org/1999/xhtml\">";
        svg_output += html;
        svg_output += "</div></foreignObject></svg>";

        return svg_output.c_str();
    }
}