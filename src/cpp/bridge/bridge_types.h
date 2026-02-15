#ifndef BRIDGE_TYPES_H
#define BRIDGE_TYPES_H

#include <string>
#include <tuple>
#include <vector>

#include "include/core/SkFontStyle.h"
#include "libs/litehtml/include/litehtml.h"

enum class LogLevel { None = 0, Error = 1, Warning = 2, Info = 3, Debug = 4 };
enum class RenderFormat { SVG = 0, PNG = 1, WebP = 2, PDF = 3 };

struct RenderOptions {
    bool svgTextToPaths = true;
};

void satoru_log(LogLevel level, const char *message);

struct font_info {
    litehtml::font_description desc;
    std::vector<class SkFont *> fonts;
    int fm_ascent;
    int fm_height;
    bool fake_bold;
};

struct font_request {
    std::string family;
    int weight;
    SkFontStyle::Slant slant;

    bool operator<(const font_request &other) const {
        if (family != other.family) return family < other.family;
        if (weight != other.weight) return weight < other.weight;
        return slant < other.slant;
    }
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
    float opacity;

    auto as_tuple() const {
        return std::make_tuple(color.red, color.green, color.blue, color.alpha, blur, x, y, spread,
                               inset, box_pos.x, box_pos.y, box_pos.width, box_pos.height, opacity);
    }
    bool operator<(const shadow_info &other) const { return as_tuple() < other.as_tuple(); }
};

struct image_draw_info {
    std::string url;
    litehtml::background_layer layer;
    float opacity;
};

struct conic_gradient_info {
    litehtml::background_layer layer;
    litehtml::background_layer::conic_gradient gradient;
    float opacity;
};

struct radial_gradient_info {
    litehtml::background_layer layer;
    litehtml::background_layer::radial_gradient gradient;
    float opacity;
};

struct image_resource_info {
    std::string url;
    std::vector<uint8_t> data;
};

struct linear_gradient_info {
    litehtml::background_layer layer;
    litehtml::background_layer::linear_gradient gradient;
    float opacity;
};

struct text_shadow_info {
    litehtml::shadow_vector shadows;
    litehtml::web_color text_color;
    float opacity;
};

struct text_draw_info {
    int weight;
    bool italic;
    litehtml::web_color color;
    float opacity;
};

struct filter_info {
    litehtml::css_token_vector tokens;
    float opacity;
};

#endif  // BRIDGE_TYPES_H
