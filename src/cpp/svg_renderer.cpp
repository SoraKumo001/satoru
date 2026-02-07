#include "svg_renderer.h"

#include <litehtml/master_css.h>

#include <cstdio>
#include <map>
#include <memory>
#include <set>
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
#include "litehtml.h"
#include "utils.h"

inline int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

inline int parse_hex4(const char *p) {
    return (hex_to_int(p[0]) << 12) | (hex_to_int(p[1]) << 8) | (hex_to_int(p[2]) << 4) |
           hex_to_int(p[3]);
}

static std::string bitmapToDataUrl(const SkBitmap &bitmap) {
    SkDynamicMemoryWStream stream;
    if (!SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) return "";
    sk_sp<SkData> data = stream.detachAsData();
    return "data:image/png;base64," + base64_encode((const uint8_t *)data->data(), data->size());
}

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context) {
    int initial_height = (height > 0) ? height : 1000;
    container_skia container(width, initial_height, nullptr, context, true);

    std::string css = litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";
    css += "* { box-sizing: border-box; }\n";
    css += "button { text-align: center; }\n";

    litehtml::document::ptr doc =
        litehtml::document::createFromString(html, &container, css.c_str());
    if (!doc) return "";

    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    container.set_height(content_height);

    SkDynamicMemoryWStream stream;
    SkRect bounds = SkRect::MakeWH((float)width, (float)content_height);

    std::unique_ptr<SkCanvas> canvas =
        SkSVGCanvas::Make(bounds, &stream, SkSVGCanvas::kConvertTextToPaths_Flag);

    container.set_canvas(canvas.get());
    litehtml::position clip(0, 0, width, content_height);
    doc->draw(0, 0, 0, &clip);

    canvas.reset();

    sk_sp<SkData> data = stream.detachAsData();
    std::string svg_str((const char *)data->data(), data->size());
    size_t svg_len = svg_str.length();

    std::set<int> used_shadow_indices;
    std::set<int> used_image_indices;
    std::set<int> used_conic_indices;

    for (size_t i = 0; i + 6 < svg_len; ++i) {
        if (svg_str[i] == '#' && (svg_str[i + 1] == 'F' || svg_str[i + 1] == 'f')) {
            if (svg_str[i + 2] == 'B' || svg_str[i + 2] == 'b') {  // Image tag #FBxxxx
                int idx = parse_hex4(svg_str.c_str() + i + 3);
                used_image_indices.insert(idx);
                i += 6;
            } else if (svg_str[i + 2] == 'C' || svg_str[i + 2] == 'c') {  // Shadow tag #FCxxxx
                int idx = parse_hex4(svg_str.c_str() + i + 3);
                used_shadow_indices.insert(idx);
                i += 6;
            } else if (svg_str[i + 2] == 'D' ||
                       svg_str[i + 2] == 'd') {  // Conic Gradient tag #FDxxxx
                int idx = parse_hex4(svg_str.c_str() + i + 3);
                used_conic_indices.insert(idx);
                i += 6;
            }
        }
    }

    std::string extra_defs;
    for (int idx : used_shadow_indices) {
        if (idx <= 0 || idx > (int)container.get_shadow_count()) continue;
        const auto &si = container.get_shadow_info(idx);

        if (si.blur > 0) {
            char buf[256];
            sprintf(buf,
                    "<filter id=\"shadow_filter_%d\" x=\"-50%%\" y=\"-50%%\" "
                    "width=\"200%%\" height=\"200%%\">",
                    idx);
            extra_defs += buf;
            sprintf(buf, "<feGaussianBlur stdDeviation=\"%.2f\"/>", si.blur * 0.5f);
            extra_defs += buf;
            extra_defs += "</filter>";
        }

        char buf[128];
        sprintf(buf, "<clipPath id=\"shadow_clip_%d\">", idx);
        extra_defs += buf;

        char path_data[512];
        const auto &br = si.box_radius;
        sprintf(
            path_data,
            "M%.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 "
            "1 %.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 "
            "1 %.2f %.2f Z",
            (float)si.box_pos.x + br.top_left_x, (float)si.box_pos.y,
            (float)si.box_pos.width - br.top_left_x - br.top_right_x, (float)br.top_right_x,
            (float)br.top_right_y, (float)br.top_right_x, (float)br.top_right_y,
            (float)si.box_pos.height - br.top_right_y - br.bottom_right_y, (float)br.bottom_right_x,
            (float)br.bottom_right_y, (float)-br.bottom_right_x, (float)br.bottom_right_y,
            (float)-(si.box_pos.width - br.bottom_left_x - br.bottom_right_x),
            (float)br.bottom_left_x, (float)br.bottom_left_y, (float)-br.bottom_left_x,
            (float)-br.bottom_left_y,
            (float)-(si.box_pos.height - br.top_left_y - br.bottom_left_y), (float)br.top_left_x,
            (float)br.top_left_y, (float)br.top_left_x, (float)-br.top_left_y);

        if (si.inset) {
            extra_defs += "<path d=\"";
            extra_defs += path_data;
            extra_defs += "\"/>";
        } else {
            extra_defs +=
                "<path clip-rule=\"evenodd\" d=\"M-10000 -10000 h20000 "
                "v20000 h-20000 Z ";
            extra_defs += path_data;
            extra_defs += "\"/>";
        }
        extra_defs += "</clipPath>";
    }

    for (int idx : used_image_indices) {
        if (idx <= 0 || idx > (int)container.get_image_count()) continue;
        const auto &idi = container.get_image_draw_info(idx);
        const auto &br = idi.layer.border_radius;
        if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 ||
            br.bottom_right_x > 0) {
            char buf[128];
            sprintf(buf, "<clipPath id=\"image_clip_%d\">", idx);
            extra_defs += buf;
            char path_data[512];
            sprintf(path_data,
                    "M%.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 "
                    "0 1 %.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f "
                    "0 0 1 %.2f %.2f Z",
                    (float)idi.layer.border_box.x + br.top_left_x, (float)idi.layer.border_box.y,
                    (float)idi.layer.border_box.width - br.top_left_x - br.top_right_x,
                    (float)br.top_right_x, (float)br.top_right_y, (float)br.top_right_x,
                    (float)br.top_right_y,
                    (float)idi.layer.border_box.height - br.top_right_y - br.bottom_right_y,
                    (float)br.bottom_right_x, (float)br.bottom_right_y, (float)-br.bottom_right_x,
                    (float)br.bottom_right_y,
                    (float)-(idi.layer.border_box.width - br.bottom_left_x - br.bottom_right_x),
                    (float)br.bottom_left_x, (float)br.bottom_left_y, (float)-br.bottom_left_x,
                    (float)-br.bottom_left_y,
                    (float)-(idi.layer.border_box.height - br.top_left_y - br.bottom_left_y),
                    (float)br.top_left_x, (float)br.top_left_y, (float)br.top_left_x,
                    (float)-br.top_left_y);
            extra_defs += "<path d=\"";
            extra_defs += path_data;
            extra_defs += "\"/>";
            extra_defs += "</clipPath>";
        }
    }

    std::map<int, std::string> conic_data_urls;
    for (int idx : used_conic_indices) {
        if (idx <= 0 || idx > (int)container.get_conic_gradient_count()) continue;
        const auto &cgi = container.get_conic_gradient_info(idx);

        int w = (int)cgi.layer.border_box.width;
        int h = (int)cgi.layer.border_box.height;
        if (w < 1) w = 1;
        if (h < 1) h = 1;

        SkBitmap bitmap;
        bitmap.allocN32Pixels(w, h);
        SkCanvas offscreen(bitmap);
        offscreen.clear(SK_ColorTRANSPARENT);

        // Render conic gradient to bitmap
        std::vector<SkColor4f> colors;
        std::vector<float> positions;
        for (const auto &pt : cgi.gradient.color_points) {
            colors.push_back({pt.color.red / 255.0f, pt.color.green / 255.0f,
                              pt.color.blue / 255.0f, pt.color.alpha / 255.0f});
            positions.push_back(pt.offset);
        }
        SkPoint center = {(float)cgi.gradient.position.x - cgi.layer.border_box.x,
                          (float)cgi.gradient.position.y - cgi.layer.border_box.y};
        float startAngle = cgi.gradient.angle - 90.0f;

        SkGradient skGrad(SkGradient::Colors(SkSpan(colors), SkSpan(positions), SkTileMode::kClamp),
                          SkGradient::Interpolation());
        auto shader = SkShaders::SweepGradient(center, startAngle, startAngle + 360.0f, skGrad);

        SkPaint paint;
        paint.setShader(shader);
        paint.setAntiAlias(true);
        offscreen.drawRect(SkRect::MakeWH((float)w, (float)h), paint);

        conic_data_urls[idx] = bitmapToDataUrl(bitmap);

        // Also add clipPath for border radius if needed
        const auto &br = cgi.layer.border_radius;
        if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 ||
            br.bottom_right_x > 0) {
            char buf[128];
            sprintf(buf, "<clipPath id=\"conic_clip_%d\">", idx);
            extra_defs += buf;
            char path_data[512];
            sprintf(path_data,
                    "M%.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 "
                    "0 1 %.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f "
                    "0 0 1 %.2f %.2f Z",
                    (float)cgi.layer.border_box.x + br.top_left_x, (float)cgi.layer.border_box.y,
                    (float)cgi.layer.border_box.width - br.top_left_x - br.top_right_x,
                    (float)br.top_right_x, (float)br.top_right_y, (float)br.top_right_x,
                    (float)br.top_right_y,
                    (float)cgi.layer.border_box.height - br.top_right_y - br.bottom_right_y,
                    (float)br.bottom_right_x, (float)br.bottom_right_y, (float)-br.bottom_right_x,
                    (float)br.bottom_right_y,
                    (float)-(cgi.layer.border_box.width - br.bottom_left_x - br.bottom_right_x),
                    (float)br.bottom_left_x, (float)br.bottom_left_y, (float)-br.bottom_left_x,
                    (float)-br.bottom_left_y,
                    (float)-(cgi.layer.border_box.height - br.top_left_y - br.bottom_left_y),
                    (float)br.top_left_x, (float)br.top_left_y, (float)br.top_left_x,
                    (float)-br.top_left_y);
            extra_defs += "<path d=\"";
            extra_defs += path_data;
            extra_defs += "\"/>";
            extra_defs += "</clipPath>";
        }
    }

    std::string assembled;
    assembled.reserve(svg_len + extra_defs.size() + 1024);

    size_t svg_tag_start = svg_str.find("<svg");
    if (svg_tag_start == std::string::npos) return "";
    size_t svg_tag_end = svg_str.find('>', svg_tag_start);
    if (svg_tag_end == std::string::npos) return "";

    size_t existing_defs_start = svg_str.find("<defs>", svg_tag_end);

    if (existing_defs_start != std::string::npos) {
        assembled.append(svg_str.substr(0, existing_defs_start + 6));
        assembled.append(extra_defs);
        assembled.append(svg_str.substr(existing_defs_start + 6));
    } else {
        assembled.append(svg_str.substr(0, svg_tag_end + 1));
        assembled.append("<defs>");
        assembled.append(extra_defs);
        assembled.append("</defs>");
        assembled.append(svg_str.substr(svg_tag_end + 1));
    }

    std::string final_out;
    final_out.reserve(assembled.size());

    size_t cur = 0;
    size_t len = assembled.length();
    while (cur < len) {
        size_t tag_start = assembled.find('<', cur);
        if (tag_start == std::string::npos) {
            final_out.append(assembled.substr(cur));
            break;
        }
        final_out.append(assembled.substr(cur, tag_start - cur));

        size_t tag_end = assembled.find('>', tag_start);
        if (tag_end == std::string::npos) {
            final_out.append(assembled.substr(tag_start));
            break;
        }

        std::string tag = assembled.substr(tag_start, tag_end - tag_start + 1);
        cur = tag_end + 1;

        if (tag.find("d=\"\"") != std::string::npos ||
            tag.find("width=\"0\"") != std::string::npos ||
            tag.find("height=\"0\"") != std::string::npos ||
            tag.find("width=\"0.00\"") != std::string::npos ||
            tag.find("height=\"0.00\"") != std::string::npos) {
            continue;
        }

        int first_shadow_idx = -1;
        int first_image_idx = -1;
        int first_conic_idx = -1;
        size_t tag_cur = 0;
        while (true) {
            tag_cur = tag.find("#", tag_cur);
            if (tag_cur == std::string::npos || tag_cur + 6 >= tag.length()) break;

            if (tag[tag_cur + 1] == 'F' || tag[tag_cur + 1] == 'f') {
                if (tag[tag_cur + 2] == 'B' || tag[tag_cur + 2] == 'b') {  // Image
                    int idx = parse_hex4(tag.c_str() + tag_cur + 3);
                    if (idx > 0 && idx <= (int)container.get_image_count()) {
                        if (first_image_idx == -1) first_image_idx = idx;
                    }
                    tag.replace(tag_cur, 7, "none");
                    tag_cur += 4;
                } else if (tag[tag_cur + 2] == 'C' || tag[tag_cur + 2] == 'c') {  // Shadow
                    int idx = parse_hex4(tag.c_str() + tag_cur + 3);
                    std::string replacement = "none";
                    if (idx > 0 && idx <= (int)container.get_shadow_count()) {
                        if (first_shadow_idx == -1) first_shadow_idx = idx;
                        const auto &si = container.get_shadow_info(idx);
                        char color_str[64];
                        sprintf(color_str, "rgba(%d,%d,%d,%.2f)", si.color.red, si.color.green,
                                si.color.blue, si.color.alpha / 255.0f);
                        replacement = color_str;
                    }
                    tag.replace(tag_cur, 7, replacement);
                    tag_cur += replacement.length();
                } else if (tag[tag_cur + 2] == 'D' || tag[tag_cur + 2] == 'd') {  // Conic
                    int idx = parse_hex4(tag.c_str() + tag_cur + 3);
                    if (idx > 0 && idx <= (int)container.get_conic_gradient_count()) {
                        if (first_conic_idx == -1) first_conic_idx = idx;
                    }
                    tag.replace(tag_cur, 7, "none");
                    tag_cur += 4;
                } else {
                    tag_cur += 1;
                }
            } else {
                tag_cur += 1;
            }
        }

        if (first_image_idx != -1) {
            const auto &idi = container.get_image_draw_info(first_image_idx);
            std::string data_url = context.imageCache[idi.url].data_url;
            char img_coords[256];
            sprintf(img_coords,
                    "<image x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                    "xlink:href=\"",
                    (float)idi.layer.origin_box.x, (float)idi.layer.origin_box.y,
                    (float)idi.layer.origin_box.width, (float)idi.layer.origin_box.height);

            std::string img_tag = img_coords;
            img_tag += data_url;
            img_tag += "\"/>";

            const auto &br = idi.layer.border_radius;
            if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 ||
                br.bottom_right_x > 0) {
                char buf[128];
                sprintf(buf, "<g clip-path=\"url(#image_clip_%d)\">", first_image_idx);
                final_out += buf;
                final_out += img_tag;
                final_out += "</g>";
            } else {
                final_out += img_tag;
            }
        } else if (first_conic_idx != -1) {
            const auto &cgi = container.get_conic_gradient_info(first_conic_idx);
            std::string data_url = conic_data_urls[first_conic_idx];
            char img_coords[256];
            sprintf(img_coords,
                    "<image x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                    "xlink:href=\"",
                    (float)cgi.layer.border_box.x, (float)cgi.layer.border_box.y,
                    (float)cgi.layer.border_box.width, (float)cgi.layer.border_box.height);

            std::string img_tag = img_coords;
            img_tag += data_url;
            img_tag += "\"/>";

            const auto &br = cgi.layer.border_radius;
            if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 ||
                br.bottom_right_x > 0) {
                char buf[128];
                sprintf(buf, "<g clip-path=\"url(#conic_clip_%d)\">", first_conic_idx);
                final_out += buf;
                final_out += img_tag;
                final_out += "</g>";
            } else {
                final_out += img_tag;
            }
        } else if (first_shadow_idx != -1) {
            const auto &si = container.get_shadow_info(first_shadow_idx);
            char buf_clip[64];
            sprintf(buf_clip, "<g clip-path=\"url(#shadow_clip_%d)\">", first_shadow_idx);
            final_out += buf_clip;

            if (si.inset) {
                std::string inset_tag;
                char color_str[64];
                sprintf(color_str, "rgba(%d,%d,%d,%.2f)", si.color.red, si.color.green,
                        si.color.blue, si.color.alpha / 255.0f);
                if (si.box_radius.top_left_x > 0 || si.box_radius.top_right_x > 0 ||
                    si.box_radius.bottom_left_x > 0 || si.box_radius.bottom_right_x > 0) {
                    char path_data[512];
                    sprintf(
                        path_data,
                        "M%.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f "
                        "%.2f 0 0 1 %.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f "
                        "a%.2f %.2f 0 0 1 %.2f %.2f Z",
                        (float)si.box_pos.x + si.x + si.box_radius.top_left_x,
                        (float)si.box_pos.y + si.y,
                        (float)si.box_pos.width - si.box_radius.top_left_x -
                            si.box_radius.top_right_x,
                        (float)si.box_radius.top_right_x, (float)si.box_radius.top_right_y,
                        (float)si.box_radius.top_right_x, (float)si.box_radius.top_right_y,
                        (float)si.box_pos.height - si.box_radius.top_right_y -
                            si.box_radius.bottom_right_y,
                        (float)si.box_radius.bottom_right_x, (float)si.box_radius.bottom_right_y,
                        (float)-si.box_radius.bottom_right_x, (float)si.box_radius.bottom_right_y,
                        (float)-(si.box_pos.width - si.box_radius.bottom_left_x -
                                 si.box_radius.bottom_right_x),
                        (float)si.box_radius.bottom_left_x, (float)si.box_radius.bottom_left_y,
                        (float)-si.box_radius.bottom_left_x, (float)-si.box_radius.bottom_left_y,
                        (float)-(si.box_pos.height - si.box_radius.top_left_y -
                                 si.box_radius.bottom_left_y),
                        (float)si.box_radius.top_left_x, (float)si.box_radius.top_left_y,
                        (float)si.box_radius.top_left_x, (float)-si.box_radius.top_left_y);
                    char path_tag[1024];
                    sprintf(path_tag,
                            "<path d=\"%s\" fill=\"none\" stroke=\"%s\" "
                            "stroke-width=\"%.2f\"/>",
                            path_data, color_str, si.blur * 2.0f);
                    inset_tag = path_tag;
                } else {
                    char rect_tag[1024];
                    sprintf(rect_tag,
                            "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                            "fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\"/>",
                            (float)si.box_pos.x + si.x - si.blur,
                            (float)si.box_pos.y + si.y - si.blur,
                            (float)si.box_pos.width + si.blur * 2,
                            (float)si.box_pos.height + si.blur * 2, color_str, si.blur * 2.0f);
                    inset_tag = rect_tag;
                }
                if (si.blur > 0) {
                    char buf[64];
                    sprintf(buf, "<g filter=\"url(#shadow_filter_%d)\">", first_shadow_idx);
                    final_out += buf;
                }
                final_out += inset_tag;
                if (si.blur > 0) final_out += "</g>";
            } else if (si.blur > 0) {
                char buf[64];
                sprintf(buf, "<g filter=\"url(#shadow_filter_%d)\">", first_shadow_idx);
                final_out += buf;
                final_out += tag;
                final_out += "</g>";
            } else {
                final_out += tag;
            }
            final_out += "</g>";
        } else {
            final_out += tag;
        }
    }

    return final_out;
}