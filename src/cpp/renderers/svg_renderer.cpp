#include "svg_renderer.h"

#include <litehtml/master_css.h>

#include <cstdio>
#include <iomanip>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "container_skia.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkStream.h"
#include "include/effects/SkGradient.h"
#include "include/encode/SkPngEncoder.h"
#include "include/svg/SkSVGCanvas.h"
#include "../utils/utils.h"

namespace {
static std::string bitmapToDataUrl(const SkBitmap &bitmap) {
    SkDynamicMemoryWStream stream;
    if (!SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) return "";
    sk_sp<SkData> data = stream.detachAsData();
    return "data:image/png;base64," + base64_encode((const uint8_t *)data->data(), data->size());
}

static bool has_radius(const litehtml::border_radiuses &r) {
    return r.top_left_x > 0 || r.top_left_y > 0 || r.top_right_x > 0 || r.top_right_y > 0 ||
           r.bottom_right_x > 0 || r.bottom_right_y > 0 || r.bottom_left_x > 0 || r.bottom_left_y > 0;
}

static std::string path_from_rrect(const litehtml::position &pos, const litehtml::border_radiuses &r) {
    std::stringstream ss;
    float x = (float)pos.x, y = (float)pos.y, w = (float)pos.width, h = (float)pos.height;
    ss << "M" << x + r.top_left_x << "," << y
       << " L" << x + w - r.top_right_x << "," << y;
    if (r.top_right_x > 0 || r.top_right_y > 0)
        ss << " A " << r.top_right_x << " " << r.top_right_y << " 0 0 1 " << x + w << " " << y + r.top_right_y;
    ss << " L" << x + w << "," << y + h - r.bottom_right_y;
    if (r.bottom_right_x > 0 || r.bottom_right_y > 0)
        ss << " A " << r.bottom_right_x << " " << r.bottom_right_y << " 0 0 1 " << x + w - r.bottom_right_x << " " << y + h;
    ss << " L" << x + r.bottom_left_x << "," << y + h;
    if (r.bottom_left_x > 0 || r.bottom_left_y > 0)
        ss << " A " << r.bottom_left_x << " " << r.bottom_left_y << " 0 0 1 " << x << " " << y + h - r.bottom_left_y;
    ss << " L" << x << "," << y + r.top_left_y;
    if (r.top_left_x > 0 || r.top_left_y > 0)
        ss << " A " << r.top_left_x << " " << r.top_left_y << " 0 0 1 " << x + r.top_left_x << " " << y;
    ss << " Z";
    return ss.str();
}

void processTags(std::string& svg, SatoruContext& context, const container_skia& container) {
    const auto& shadows = container.get_used_shadows();
    for (size_t i = 0; i < shadows.size(); ++i) {
        int index = (int)(i + 1);
        std::string filterId = "shadow-" + std::to_string(index);
        char hex[32], rgb[64];
        snprintf(hex, sizeof(hex), "#0001%02X", index);
        snprintf(rgb, sizeof(rgb), "rgb(0,1,%d)", index);

        std::string pattern = "fill=\"(" + std::string(hex) + "|" + std::string(rgb) + ")\"";
        std::regex re(pattern, std::regex::icase);
        std::string replacement = "filter=\"url(#" + filterId + ")\" fill=\"black\"";
        svg = std::regex_replace(svg, re, replacement);
        
        std::string patternStyle = "fill:(" + std::string(hex) + "|" + std::string(rgb) + ");?";
        std::regex reStyle(patternStyle, std::regex::icase);
        std::string replacementStyle = "filter:url(#" + filterId + ");fill:black;";
        svg = std::regex_replace(svg, reStyle, replacementStyle);
    }

    const auto& images = container.get_used_image_draws();
    for (size_t i = 0; i < images.size(); ++i) {
        const auto& draw = images[i];
        int index = (int)(i + 1);
        auto it = context.imageCache.find(draw.url);
        if (it != context.imageCache.end() && it->second.skImage) {
            SkBitmap bitmap;
            bitmap.allocN32Pixels(it->second.skImage->width(), it->second.skImage->height());
            SkCanvas bitmapCanvas(bitmap);
            bitmapCanvas.drawImage(it->second.skImage, 0, 0);
            
            char hex[32], rgb[64];
            snprintf(hex, sizeof(hex), "#0100%02X", index);
            snprintf(rgb, sizeof(rgb), "rgb(1,0,%d)", index);

            std::stringstream ss;
            ss << "<image x=\"" << draw.layer.origin_box.x << "\" y=\"" << draw.layer.origin_box.y
               << "\" width=\"" << draw.layer.origin_box.width << "\" height=\""
               << draw.layer.origin_box.height << "\" href=\"" << bitmapToDataUrl(bitmap) << "\"";
            if (has_radius(draw.layer.border_radius)) {
                ss << " clip-path=\"url(#clip-img-" << index << ")\"";
            }
            ss << " />";
            size_t pos = svg.find(hex);
            if (pos == std::string::npos) {
                char hex_low[32]; snprintf(hex_low, sizeof(hex_low), "#0100%02x", index);
                pos = svg.find(hex_low);
            }
            if (pos == std::string::npos) pos = svg.find(rgb);

            if (pos != std::string::npos) {
                size_t start = svg.rfind("<", pos);
                size_t end = svg.find("/>", pos);
                if (start != std::string::npos && end != std::string::npos) {
                    svg.replace(start, end + 2 - start, ss.str());
                }
            }
        }
    }
}
}  // namespace

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context, const char* master_css) {
    container_skia measure_container(width, height > 0 ? height : 1000, nullptr, context, nullptr, false);
    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";
    
    auto doc = litehtml::document::createFromString(html, &measure_container, css.c_str());
    if (!doc) return "";
    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    SkDynamicMemoryWStream stream;
    SkSVGCanvas::Options svg_options;
    svg_options.flags = SkSVGCanvas::kConvertTextToPaths_Flag;
    auto canvas = SkSVGCanvas::Make(SkRect::MakeWH((float)width, (float)content_height), &stream, svg_options);

    container_skia render_container(width, content_height, canvas.get(), context, nullptr, true);
    auto render_doc = litehtml::document::createFromString(html, &render_container, css.c_str());
    render_doc->render(width);

    litehtml::position clip(0, 0, width, content_height);
    render_doc->draw(0, 0, 0, &clip);

    canvas.reset();
    sk_sp<SkData> data = stream.detachAsData();
    std::string svg((const char *)data->data(), data->size());

    processTags(svg, context, render_container);

    std::stringstream defs;
    const auto &shadows = render_container.get_used_shadows();
    for (size_t i = 0; i < shadows.size(); ++i) {
        const auto &s = shadows[i];
        int index = (int)(i + 1);
        
        defs << "<filter id=\"shadow-" << index << "\" x=\"-100%\" y=\"-100%\" width=\"300%\" height=\"300%\">";
        
        std::string floodColor = "rgb(" + std::to_string((int)s.color.red) + "," + 
                                 std::to_string((int)s.color.green) + "," + 
                                 std::to_string((int)s.color.blue) + ")";
        float floodOpacity = (float)s.color.alpha / 255.0f;

        if (s.inset) {
            // Inset Shadow Logic:
            // 1. Create a large flood area.
            // 2. Subtract SourceAlpha from it (getting the area outside the shape).
            // 3. Blur and offset this 'outside' area.
            // 4. Clip the result back to SourceAlpha.
            defs << "<feFlood flood-color=\"" << floodColor << "\" flood-opacity=\"" << floodOpacity << "\" result=\"color\"/>"
                 << "<feComposite in=\"color\" in2=\"SourceAlpha\" operator=\"out\" result=\"inverse\"/>"
                 << "<feGaussianBlur in=\"inverse\" stdDeviation=\"" << s.blur * 0.5f << "\" result=\"blur\"/>"
                 << "<feOffset dx=\"" << s.x << "\" dy=\"" << s.y << "\" result=\"offset\"/>"
                 << "<feComposite in=\"offset\" in2=\"SourceAlpha\" operator=\"in\" result=\"inset-shadow\"/>"
                 << "<feMerge><feMergeNode in=\"inset-shadow\"/></feMerge>";
        } else {
            // Normal (Outer) Shadow Logic:
            defs << "<feGaussianBlur in=\"SourceAlpha\" stdDeviation=\"" << s.blur * 0.5f << "\" result=\"blur\"/>"
                 << "<feOffset dx=\"" << s.x << "\" dy=\"" << s.y << "\" result=\"offsetblur\"/>"
                 << "<feFlood flood-color=\"" << floodColor << "\" flood-opacity=\"" << floodOpacity << "\"/>"
                 << "<feComposite in2=\"offsetblur\" operator=\"in\"/>"
                 << "<feMerge><feMergeNode/></feMerge>";
        }
        
        defs << "</filter>";
    }

    const auto &images = render_container.get_used_image_draws();
    for (size_t i = 0; i < images.size(); ++i) {
        const auto &draw = images[i];
        if (has_radius(draw.layer.border_radius)) {
            defs << "<clipPath id=\"clip-img-" << (i + 1) << "\">";
            defs << "<path d=\"" << path_from_rrect(draw.layer.border_box, draw.layer.border_radius) << "\" />";
            defs << "</clipPath>";
        }
    }

    std::string defsStr = defs.str();
    if (!defsStr.empty()) {
        size_t svgStart = svg.find("<svg");
        if (svgStart != std::string::npos) {
            size_t tagEnd = svg.find(">", svgStart);
            if (tagEnd != std::string::npos) svg.insert(tagEnd + 1, "<defs>" + defsStr + "</defs>");
        }
    }

    return svg;
}
