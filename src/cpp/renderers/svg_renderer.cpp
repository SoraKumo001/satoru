#include "svg_renderer.h"

#include <litehtml/master_css.h>

#include <cstdio>
#include <iomanip>
#include <map>
#include <memory>
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

namespace {
inline int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

inline int parse_hex2(const char *p) { return (hex_to_int(p[0]) << 4) | hex_to_int(p[1]); }

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const uint8_t *data, size_t len) {
    std::string ret;
    int i = 0, j = 0;
    uint8_t char_array_3[3], char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; j < i + 1; j++) ret += base64_chars[char_array_4[j]];
        while (i++ < 3) ret += '=';
    }
    return ret;
}

static std::string bitmapToDataUrl(const SkBitmap &bitmap) {
    SkDynamicMemoryWStream stream;
    if (!SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) return "";
    sk_sp<SkData> data = stream.detachAsData();
    return "data:image/png;base64," + base64_encode((const uint8_t *)data->data(), data->size());
}

std::string f2s(float v) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%.2f", v);
    if (len <= 0) return "0";
    std::string s(buf, len);
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') s.pop_back();
    }
    return s;
}
}  // namespace

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context) {
    SkDynamicMemoryWStream stream;
    SkSVGCanvas::Options options;
    options.flags = SkSVGCanvas::kConvertTextToPaths_Flag;
    auto canvas = SkSVGCanvas::Make(SkRect::MakeWH((float)width, height > 0 ? (float)height : 1000.0f), &stream, options);

    container_skia container(width, height > 0 ? height : 1000, canvas.get(), context, nullptr, true);
    
    std::string css = litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";
    
    auto doc = litehtml::document::createFromString(html, &container, css.c_str());
    if (!doc) return "";

    doc->render(width);
    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    litehtml::position clip(0, 0, width, content_height);
    doc->draw(0, 0, 0, &clip);

    canvas.reset();

    sk_sp<SkData> data = stream.detachAsData();
    if (!data) return "";
    std::string svg_str((const char *)data->data(), data->size());

    // Step 1: Pre-calculate conic
    std::map<int, std::string> conic_data_urls;
    for (size_t i = 0; i < container.get_conic_gradient_count(); ++i) {
        const auto &cgi = container.get_conic_gradient_info(i + 1);
        SkBitmap bitmap;
        bitmap.allocN32Pixels((int)cgi.layer.border_box.width, (int)cgi.layer.border_box.height);
        SkCanvas conic_canvas(bitmap);
        conic_canvas.clear(SK_ColorTRANSPARENT);

        SkPoint center =
            SkPoint::Make((float)cgi.gradient.position.x - (float)cgi.layer.border_box.x,
                          (float)cgi.gradient.position.y - (float)cgi.layer.border_box.y);

        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        for (const auto &stop : cgi.gradient.color_points) {
            colors.push_back({stop.color.red / 255.0f, stop.color.green / 255.0f,
                              stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
            pos.push_back(stop.offset);
        }

        SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(pos), SkTileMode::kClamp),
                          SkGradient::Interpolation());
        auto shader = SkShaders::SweepGradient(center, cgi.gradient.angle, cgi.gradient.angle + 360.0f, skGrad);

        SkPaint paint;
        paint.setShader(shader);
        paint.setAntiAlias(true);
        conic_canvas.drawRect(SkRect::MakeWH((float)bitmap.width(), (float)bitmap.height()), paint);
        conic_data_urls[i + 1] = bitmapToDataUrl(bitmap);
    }

    // Step 2: Discovery of used tags
    std::set<int> used_shadow_indices;
    std::set<int> used_image_indices;
    std::set<int> used_conic_indices;

    size_t s_pos = 0;
    while ((s_pos = svg_str.find("fill=\"#0000", s_pos)) != std::string::npos) {
        if (s_pos + 13 >= svg_str.length()) break;
        int idx = parse_hex2(svg_str.c_str() + s_pos + 11);
        
        size_t op_pos = svg_str.find("fill-opacity=\"", s_pos);
        if (op_pos != std::string::npos && op_pos < s_pos + 50) {
            if (svg_str.compare(op_pos + 14, 5, "0.996") == 0) used_shadow_indices.insert(idx);
            else if (svg_str.compare(op_pos + 14, 5, "0.992") == 0) used_conic_indices.insert(idx);
            else used_image_indices.insert(idx);
        } else {
            used_image_indices.insert(idx);
        }
        s_pos += 13;
    }

    std::string final_out;
    final_out.reserve(svg_str.size());

    // Step 3: Definitions Injection
    std::string injected_defs;
    for (int idx : used_shadow_indices) {
        const auto &si = container.get_shadow_info(idx);
        std::string color_str = "rgba(" + std::to_string(si.color.red) + "," + std::to_string(si.color.green) + "," + 
                                std::to_string(si.color.blue) + "," + f2s(si.color.alpha / 255.0f) + ")";
        
        injected_defs += "<filter id=\"shadow_filter_" + std::to_string(idx) + "\" x=\"-100%\" y=\"-100%\" width=\"300%\" height=\"300%\">";
        if (si.inset) {
            // Inset Shadow Filter
            injected_defs += "<feFlood flood-color=\"" + color_str + "\"/>";
            injected_defs += "<feComposite in2=\"SourceAlpha\" operator=\"out\" result=\"outside\"/>";
            injected_defs += "<feOffset dx=\"" + f2s(si.x) + "\" dy=\"" + f2s(si.y) + "\" in=\"outside\" result=\"offsetOutside\"/>";
            injected_defs += "<feGaussianBlur stdDeviation=\"" + f2s(si.blur * 0.5f) + "\" in=\"offsetOutside\" result=\"blurredOffset\"/>";
            injected_defs += "<feComposite in2=\"SourceAlpha\" operator=\"in\"/>";
        } else {
            // Outer Shadow Filter
            injected_defs += "<feGaussianBlur in=\"SourceAlpha\" stdDeviation=\"" + f2s(si.blur * 0.5f) + "\"/>";
            injected_defs += "<feOffset dx=\"" + f2s(si.x) + "\" dy=\"" + f2s(si.y) + "\" result=\"offsetblur\"/>";
            injected_defs += "<feFlood flood-color=\"" + color_str + "\"/>";
            injected_defs += "<feComposite in2=\"offsetblur\" operator=\"in\" result=\"coloredShadow\"/>";
            injected_defs += "<feComposite in2=\"SourceAlpha\" operator=\"out\"/>";
        }
        injected_defs += "</filter>";
    }
    for (int idx : used_conic_indices) {
        const auto &cgi = container.get_conic_gradient_info(idx);
        injected_defs += "<clipPath id=\"conic_clip_" + std::to_string(idx) + "\">";
        const auto &br = cgi.layer.border_radius;
        if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 || br.bottom_right_x > 0) {
            injected_defs += "<path d=\"M" + f2s((float)cgi.layer.border_box.x + (float)br.top_left_x) + " " + f2s((float)cgi.layer.border_box.y) + 
                         " h" + f2s((float)cgi.layer.border_box.width - (float)br.top_left_x - (float)br.top_right_x) + 
                         " a" + f2s((float)br.top_right_x) + " " + f2s((float)br.top_right_y) + " 0 0 1 " + f2s((float)br.top_right_x) + " " + f2s((float)br.top_right_y) + 
                         " v" + f2s((float)cgi.layer.border_box.height - (float)br.top_right_y - (float)br.bottom_right_y) + 
                         " a" + f2s((float)br.bottom_right_x) + " " + f2s((float)br.bottom_right_y) + " 0 0 1 " + f2s((float)-br.bottom_right_x) + " " + f2s((float)br.bottom_right_y) + 
                         " h" + f2s((float)-(cgi.layer.border_box.width - (float)br.bottom_left_x - (float)br.bottom_right_x)) + 
                         " a" + f2s((float)br.bottom_left_x) + " " + f2s((float)br.bottom_left_y) + " 0 0 1 " + f2s((float)-br.bottom_left_x) + " " + f2s((float)-br.bottom_left_y) + 
                         " v" + f2s((float)-(cgi.layer.border_box.height - (float)br.top_left_y - (float)br.bottom_left_y)) + 
                         " a" + f2s((float)br.top_left_x) + " " + f2s((float)br.top_left_y) + " 0 0 1 " + f2s((float)br.top_left_x) + " " + f2s((float)-br.top_left_y) + " Z\"/>";
        } else {
            injected_defs += "<rect x=\"" + f2s((float)cgi.layer.border_box.x) + "\" y=\"" + f2s((float)cgi.layer.border_box.y) + 
                         "\" width=\"" + f2s((float)cgi.layer.border_box.width) + "\" height=\"" + f2s((float)cgi.layer.border_box.height) + "\"/>";
        }
        injected_defs += "</clipPath>";
    }
    for (int idx : used_image_indices) {
        const auto &idi = container.get_image_draw_info(idx);
        injected_defs += "<clipPath id=\"image_clip_" + std::to_string(idx) + "\">";
        const auto &br = idi.layer.border_radius;
        if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 || br.bottom_right_x > 0) {
            injected_defs += "<path d=\"M" + f2s((float)idi.layer.border_box.x + (float)br.top_left_x) + " " + f2s((float)idi.layer.border_box.y) + 
                         " h" + f2s((float)idi.layer.border_box.width - (float)br.top_left_x - (float)br.top_right_x) + 
                         " a" + f2s((float)br.top_right_x) + " " + f2s((float)br.top_right_y) + " 0 0 1 " + f2s((float)br.top_right_x) + " " + f2s((float)br.top_right_y) + 
                         " v" + f2s((float)idi.layer.border_box.height - (float)br.top_right_y - (float)br.bottom_right_y) + 
                         " a" + f2s((float)br.bottom_right_x) + " " + f2s((float)br.bottom_right_y) + " 0 0 1 " + f2s((float)-br.bottom_right_x) + " " + f2s((float)br.bottom_right_y) + 
                         " h" + f2s((float)-(idi.layer.border_box.width - (float)br.bottom_left_x - (float)br.bottom_right_x)) + 
                         " a" + f2s((float)br.bottom_left_x) + " " + f2s((float)br.bottom_left_y) + " 0 0 1 " + f2s((float)-br.bottom_left_x) + " " + f2s((float)-br.bottom_left_y) + 
                         " v" + f2s((float)-(idi.layer.border_box.height - (float)br.top_left_y - (float)br.bottom_left_y)) + 
                         " a" + f2s((float)br.top_left_x) + " " + f2s((float)br.top_left_y) + " 0 0 1 " + f2s((float)br.top_left_x) + " " + f2s((float)-br.top_left_y) + " Z\"/>";
        } else {
            injected_defs += "<rect x=\"" + f2s((float)idi.layer.border_box.x) + "\" y=\"" + f2s((float)idi.layer.border_box.y) + 
                         "\" width=\"" + f2s((float)idi.layer.border_box.width) + "\" height=\"" + f2s((float)idi.layer.border_box.height) + "\"/>";
        }
        injected_defs += "</clipPath>";
    }

    size_t defs_pos = svg_str.find("<defs>");
    size_t start_render_pos = 0;
    if (defs_pos != std::string::npos) {
        final_out.append(svg_str.substr(0, defs_pos + 6));
        final_out.append(injected_defs);
        start_render_pos = defs_pos + 6;
    } else {
        size_t svg_tag_end = svg_str.find(">", svg_str.find("<svg"));
        if (svg_tag_end != std::string::npos) {
            final_out.append(svg_str.substr(0, svg_tag_end + 1));
            final_out.append("<defs>");
            final_out.append(injected_defs);
            final_out.append("</defs>");
            start_render_pos = svg_tag_end + 1;
        } else {
            return svg_str;
        }
    }

    // Step 4: Final Tag Replacement
    size_t last_pos = start_render_pos;
    while (true) {
        const char *tags[] = {"<path", "<rect", "<ellipse", "<circle"};
        size_t s_pos = std::string::npos;
        for (const char *tag : tags) {
            size_t pos = svg_str.find(tag, last_pos);
            if (pos != std::string::npos) {
                if (s_pos == std::string::npos || pos < s_pos) s_pos = pos;
            }
        }

        if (s_pos == std::string::npos) break;
        
        size_t end_pos = svg_str.find("/>", s_pos);
        if (end_pos == std::string::npos) break;
        
        size_t fill_pos = svg_str.find("fill=\"#0000", s_pos);
        if (fill_pos != std::string::npos && fill_pos < end_pos) {
            int idx = parse_hex2(svg_str.c_str() + fill_pos + 11);
            size_t op_pos = svg_str.find("fill-opacity=\"", s_pos);
            bool is_shadow = (op_pos != std::string::npos && op_pos < end_pos && svg_str.compare(op_pos + 14, 5, "0.996") == 0);
            bool is_conic = (op_pos != std::string::npos && op_pos < end_pos && svg_str.compare(op_pos + 14, 5, "0.992") == 0);
            
            final_out.append(svg_str.substr(last_pos, s_pos - last_pos));

            if (is_shadow) {
                final_out += "<g filter=\"url(#shadow_filter_" + std::to_string(idx) + ")\">";
                std::string p_tag = svg_str.substr(s_pos, end_pos - s_pos + 2);
                final_out.append(p_tag);
                final_out += "</g>";
            } else if (is_conic) {
                const auto &cgi = container.get_conic_gradient_info(idx);
                final_out += "<g clip-path=\"url(#conic_clip_" + std::to_string(idx) + ")\">";
                final_out += "<image x=\"" + f2s((float)cgi.layer.border_box.x) + "\" y=\"" + f2s((float)cgi.layer.border_box.y) + 
                             "\" width=\"" + f2s((float)cgi.layer.border_box.width) + "\" height=\"" + f2s((float)cgi.layer.border_box.height) + 
                             "\" xlink:href=\"" + conic_data_urls[idx] + "\"/></g>";
            } else { // Image
                const auto &idi = container.get_image_draw_info(idx);
                final_out += "<g clip-path=\"url(#image_clip_" + std::to_string(idx) + ")\">";
                final_out += "<image x=\"" + f2s((float)idi.layer.origin_box.x) + "\" y=\"" + f2s((float)idi.layer.origin_box.y) + 
                             "\" width=\"" + f2s((float)idi.layer.origin_box.width) + "\" height=\"" + f2s((float)idi.layer.origin_box.height) + 
                             "\" xlink:href=\"" + context.imageCache[idi.url].data_url + "\"/></g>";
            }
            last_pos = end_pos + 2;
        } else {
            final_out.append(svg_str.substr(last_pos, end_pos + 2 - last_pos));
            last_pos = end_pos + 2;
        }
    }
    final_out.append(svg_str.substr(last_pos));
    return final_out;
}
