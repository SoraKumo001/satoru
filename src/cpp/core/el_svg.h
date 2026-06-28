#ifndef EL_SVG_H
#define EL_SVG_H

#include <cstdio>
#include <sstream>

#include "libs/litehtml/include/litehtml/html_tag.h"

namespace litehtml {

// ─────────────────────────────────────────────────
// Pure utility functions (extracted for testability)
// ─────────────────────────────────────────────────
// These are inline so they don't require el_svg.cpp
// (which depends on Skia infrastructure) to be compiled.

/// Convert a web_color to SVG-compatible string format:
///   opaque → "#RRGGBB"
///   alpha < 255 → "rgba(r,g,b,a)"
inline std::string color_to_string(const web_color& color) {
    if (color.alpha < 255) {
        std::stringstream ss;
        ss << "rgba(" << (int)color.red << "," << (int)color.green << "," << (int)color.blue << ","
           << (float)color.alpha / 255.0f << ")";
        return ss.str();
    } else {
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.red, color.green, color.blue);
        return std::string(hex);
    }
}

/// Replace every occurrence of "currentColor" in @p xml with the
/// string representation of @p color.
/// Returns the modified copy.
inline std::string replace_current_color(const std::string& xml, const web_color& color) {
    std::string result = xml;
    std::string color_str = color_to_string(color);
    size_t start_pos = 0;
    while ((start_pos = result.find("currentColor", start_pos)) != std::string::npos) {
        result.replace(start_pos, 12, color_str);
        start_pos += color_str.length();
    }
    return result;
}

// ─────────────────────────────────────────────────
// el_svg class
// ─────────────────────────────────────────────────

class el_svg : public html_tag {
   public:
    el_svg(const std::shared_ptr<document>& doc);
    virtual ~el_svg();

    void draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
              const std::shared_ptr<render_item>& ri) override;
    void parse_attributes() override;
    bool is_replaced() const override { return true; }
    void get_content_size(size& sz, pixel_t max_width) override;
    std::shared_ptr<render_item> create_render_item(
        const std::shared_ptr<render_item>& parent_ri) override;

   private:
    std::string reconstruct_xml(int x, int y, int width, int height) const;
    void write_element(std::ostream& os, const element::ptr& el) const;
};
}  // namespace litehtml

#endif
