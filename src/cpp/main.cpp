#include <emscripten/emscripten.h>
#include "litehtml.h"
#include <litehtml/master_css.h>
#include "include/core/SkCanvas.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkImage.h"
#include "include/ports/SkFontMgr_empty.h"
#include "include/svg/SkSVGCanvas.h"

#include "image_types.h"
#include "container_skia.h"
#include "utils.h"

#include <string>
#include <vector>
#include <map>
#include <set>

static sk_sp<SkFontMgr> g_fontMgr;
static std::map<std::string, std::vector<sk_sp<SkTypeface>>> g_typefaceCache;
static sk_sp<SkTypeface> g_defaultTypeface;
static std::vector<sk_sp<SkTypeface>> g_fallbackTypefaces;
static std::map<std::string, image_info> g_imageCache;

inline int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

inline int parse_hex4(const char* p) {
    return (hex_to_int(p[0]) << 12) | (hex_to_int(p[1]) << 8) | (hex_to_int(p[2]) << 4) | hex_to_int(p[3]);
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void init_engine() {
        SkGraphics::Init();
        g_fontMgr = SkFontMgr_New_Custom_Empty();
    }

    EMSCRIPTEN_KEEPALIVE
    void load_font(const char* name, const uint8_t* data, int size) {
        std::string cleanedName = clean_font_name(name);
        sk_sp<SkData> skData = SkData::MakeWithCopy(data, size);
        sk_sp<SkTypeface> typeface = g_fontMgr->makeFromData(skData);
        if (typeface) {
            g_typefaceCache[cleanedName].push_back(typeface);
            if (!g_defaultTypeface) g_defaultTypeface = typeface;
            g_fallbackTypefaces.push_back(typeface);
        }
    }

    EMSCRIPTEN_KEEPALIVE
    void load_image(const char* name, const char* data_url, int width, int height) {
        if (!name || !data_url) return;
        
        image_info info;
        info.width = width;
        info.height = height;
        info.data_url = std::string(data_url);

        g_imageCache[std::string(name)] = info;
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_images() {
        g_imageCache.clear();
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_fonts() {
        g_typefaceCache.clear();
        g_fallbackTypefaces.clear();
        g_defaultTypeface = nullptr;
    }

    EMSCRIPTEN_KEEPALIVE
    const char* html_to_svg(const char* html, int width, int height) {
        int initial_height = (height > 0) ? height : 1000;
        container_skia container(width, initial_height, nullptr, 
                                 g_fontMgr, g_typefaceCache, g_defaultTypeface, g_fallbackTypefaces, g_imageCache);
        
        std::string css = litehtml::master_css;
        css += "\nbr { display: -litehtml-br !important; }\n";
        css += "* { box-sizing: border-box; }\n";
        css += "button { text-align: center; }\n";

        litehtml::document::ptr doc = litehtml::document::createFromString(html, &container, css.c_str());
        if (!doc) return "";
        
        doc->render(width);
        
        int content_height = (height > 0) ? height : (int)doc->height();
        if (content_height < 1) content_height = 1;
        
        container.set_height(content_height);

        SkDynamicMemoryWStream stream;
        SkRect bounds = SkRect::MakeWH((float)width, (float)content_height);
        
        std::unique_ptr<SkCanvas> canvas = SkSVGCanvas::Make(bounds, &stream, SkSVGCanvas::kConvertTextToPaths_Flag);

        container.set_canvas(canvas.get());
        litehtml::position clip(0, 0, width, content_height);
        doc->draw(0, 0, 0, &clip);

        canvas.reset();

        sk_sp<SkData> data = stream.detachAsData();
        std::string svg_str((const char*)data->data(), data->size());
        const char* svg_ptr = svg_str.c_str();
        size_t svg_len = svg_str.length();

        // 1. Fast pre-scan for markers to build indices
        std::set<int> used_shadow_indices;
        std::set<int> used_image_indices;
        
        for (size_t i = 0; i + 6 < svg_len; ++i) {
            if (svg_ptr[i] == '#' && svg_ptr[i+1] == '0') {
                if (svg_ptr[i+2] == '0') { // Image #00xxxx
                    used_image_indices.insert(parse_hex4(svg_ptr + i + 3));
                    i += 6;
                } else if (svg_ptr[i+2] == '1') { // Shadow #01xxxx
                    used_shadow_indices.insert(parse_hex4(svg_ptr + i + 3));
                    i += 6;
                }
            }
        }

        // 2. Generate <defs>
        std::string defs = "<defs>";
        for (int idx : used_shadow_indices) {
            const auto& si = container.get_shadow_info(idx);
            if (si.blur > 0) {
                char buf[256];
                sprintf(buf, "<filter id=\"shadow_filter_%d\" x=\"-50%%\" y=\"-50%%\" width=\"200%%\" height=\"200%%\">", idx);
                defs += buf;
                sprintf(buf, "<feGaussianBlur stdDeviation=\"%.2f\"/>", si.blur * 0.5f);
                defs += buf;
                defs += "</filter>";
            }
            if (si.inset) {
                char buf[128];
                sprintf(buf, "<clipPath id=\"shadow_clip_%d\">", idx);
                defs += buf;
                if (si.box_radius.top_left_x > 0 || si.box_radius.top_right_x > 0 || si.box_radius.bottom_left_x > 0 || si.box_radius.bottom_right_x > 0) {
                    char path_data[512];
                    sprintf(path_data, "M%.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 1 %.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 1 %.2f %.2f Z",
                        (float)si.box_pos.x + si.box_radius.top_left_x, (float)si.box_pos.y,
                        (float)si.box_pos.width - si.box_radius.top_left_x - si.box_radius.top_right_x,
                        (float)si.box_radius.top_right_x, (float)si.box_radius.top_right_y, (float)si.box_radius.top_right_x, (float)si.box_radius.top_right_y,
                        (float)si.box_pos.height - si.box_radius.top_right_y - si.box_radius.bottom_right_y,
                        (float)si.box_radius.bottom_right_x, (float)si.box_radius.bottom_right_y, (float)-si.box_radius.bottom_right_x, (float)si.box_radius.bottom_right_y,
                        (float)-(si.box_pos.width - si.box_radius.bottom_left_x - si.box_radius.bottom_right_x),
                        (float)si.box_radius.bottom_left_x, (float)si.box_radius.bottom_left_y, (float)-si.box_radius.bottom_left_x, (float)-si.box_radius.bottom_left_y,
                        (float)-(si.box_pos.height - si.box_radius.top_left_y - si.box_radius.bottom_left_y),
                        (float)si.box_radius.top_left_x, (float)si.box_radius.top_left_y, (float)si.box_radius.top_left_x, (float)-si.box_radius.top_left_y);
                    defs += "<path d=\""; defs += path_data; defs += "\"/>";
                } else {
                    char rect_data[128];
                    sprintf(rect_data, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\"/>",
                        (float)si.box_pos.x, (float)si.box_pos.y, (float)si.box_pos.width, (float)si.box_pos.height);
                    defs += rect_data;
                }
                defs += "</clipPath>";
            }
        }
        for (int idx : used_image_indices) {
            const auto& idi = container.get_image_draw_info(idx);
            const auto& br = idi.layer.border_radius;
            if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 || br.bottom_right_x > 0) {
                char buf[128];
                sprintf(buf, "<clipPath id=\"image_clip_%d\">", idx);
                defs += buf;
                char path_data[512];
                sprintf(path_data, "M%.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 1 %.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 1 %.2f %.2f Z",
                    (float)idi.layer.border_box.x + br.top_left_x, (float)idi.layer.border_box.y,
                    (float)idi.layer.border_box.width - br.top_left_x - br.top_right_x,
                    (float)br.top_right_x, (float)br.top_right_y, (float)br.top_right_x, (float)br.top_right_y,
                    (float)idi.layer.border_box.height - br.top_right_y - br.bottom_right_y,
                    (float)br.bottom_right_x, (float)br.bottom_right_y, (float)-br.bottom_right_x, (float)br.bottom_right_y,
                    (float)-(idi.layer.border_box.width - br.bottom_left_x - br.bottom_right_x),
                    (float)br.bottom_left_x, (float)br.bottom_left_y, (float)-br.bottom_left_x, (float)-br.bottom_left_y,
                    (float)-(idi.layer.border_box.height - br.top_left_y - br.bottom_left_y),
                    (float)br.top_left_x, (float)br.top_left_y, (float)br.top_left_x, (float)-br.top_left_y);
                defs += "<path d=\""; defs += path_data; defs += "\"/>";
                defs += "</clipPath>";
            }
        }
        defs += "</defs>";

        // 3. Process tags and replace markers
        std::string final_out;
        final_out.reserve(svg_len + defs.size() + 1024);

        size_t svg_tag_start = svg_str.find("<svg");
        if (svg_tag_start == std::string::npos) return "";
        size_t svg_tag_end = svg_str.find('>', svg_tag_start);
        if (svg_tag_end == std::string::npos) return "";
        
        final_out.append(svg_str.substr(0, svg_tag_end + 1));
        final_out.append(defs);

        size_t cur = svg_tag_end + 1;
        while (cur < svg_len) {
            size_t tag_start = svg_str.find('<', cur);
            if (tag_start == std::string::npos) {
                final_out.append(svg_str.substr(cur));
                break;
            }
            final_out.append(svg_str.substr(cur, tag_start - cur));
            
            size_t tag_end = svg_str.find('>', tag_start);
            if (tag_end == std::string::npos) {
                final_out.append(svg_str.substr(tag_start));
                break;
            }
            
            std::string tag = svg_str.substr(tag_start, tag_end - tag_start + 1);
            size_t m_pos = tag.find("#0");
            
            if (m_pos == std::string::npos) {
                final_out.append(tag);
            } else {
                if (tag[m_pos + 2] == '0') { // Image #00xxxx
                    int idx = parse_hex4(tag.c_str() + m_pos + 3);
                    const auto& idi = container.get_image_draw_info(idx);
                    std::string data_url = g_imageCache[idi.url].data_url;
                    
                    char img_coords[256];
                    sprintf(img_coords, "<image x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" xlink:href=\"",
                        (float)idi.layer.origin_box.x, (float)idi.layer.origin_box.y,
                        (float)idi.layer.origin_box.width, (float)idi.layer.origin_box.height);
                    
                    std::string img_tag = img_coords;
                    img_tag += data_url;
                    img_tag += "\"/>";
                    
                    const auto& br = idi.layer.border_radius;
                    if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 || br.bottom_right_x > 0) {
                        char buf[128];
                        sprintf(buf, "<g clip-path=\"url(#image_clip_%d)\">", idx);
                        final_out += buf; final_out += img_tag; final_out += "</g>";
                    } else {
                        final_out += img_tag;
                    }
                } else if (tag[m_pos + 2] == '1') { // Shadow #01xxxx
                    int idx = parse_hex4(tag.c_str() + m_pos + 3);
                    const auto& si = container.get_shadow_info(idx);
                    char color_str[64];
                    sprintf(color_str, "rgba(%d,%d,%d,%.2f)", si.color.red, si.color.green, si.color.blue, si.color.alpha / 255.0f);
                    
                    tag.replace(m_pos, 7, color_str);
                    
                    if (si.inset) {
                        std::string inset_tag;
                        if (si.box_radius.top_left_x > 0 || si.box_radius.top_right_x > 0 || si.box_radius.bottom_left_x > 0 || si.box_radius.bottom_right_x > 0) {
                            char path_data[512];
                            sprintf(path_data, "M%.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 1 %.2f %.2f h%.2f a%.2f %.2f 0 0 1 %.2f %.2f v%.2f a%.2f %.2f 0 0 1 %.2f %.2f Z",
                                (float)si.box_pos.x + si.x + si.box_radius.top_left_x, (float)si.box_pos.y + si.y,
                                (float)si.box_pos.width - si.box_radius.top_left_x - si.box_radius.top_right_x,
                                (float)si.box_radius.top_right_x, (float)si.box_radius.top_right_y, (float)si.box_radius.top_right_x, (float)si.box_radius.top_right_y,
                                (float)si.box_pos.height - si.box_radius.top_right_y - si.box_radius.bottom_right_y,
                                (float)si.box_radius.bottom_right_x, (float)si.box_radius.bottom_right_y, (float)-si.box_radius.bottom_right_x, (float)si.box_radius.bottom_right_y,
                                (float)-(si.box_pos.width - si.box_radius.bottom_left_x - si.box_radius.bottom_right_x),
                                (float)si.box_radius.bottom_left_x, (float)si.box_radius.bottom_left_y, (float)-si.box_radius.bottom_left_x, (float)-si.box_radius.bottom_left_y,
                                (float)-(si.box_pos.height - si.box_radius.top_left_y - si.box_radius.bottom_left_y),
                                (float)si.box_radius.top_left_x, (float)si.box_radius.top_left_y, (float)si.box_radius.top_left_x, (float)-si.box_radius.top_left_y);
                            char path_tag[1024];
                            sprintf(path_tag, "<path d=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\"/>", path_data, color_str, si.blur * 2.0f);
                            inset_tag = path_tag;
                        } else {
                            char rect_tag[1024];
                            sprintf(rect_tag, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\"/>",
                                (float)si.box_pos.x + si.x - si.blur, (float)si.box_pos.y + si.y - si.blur,
                                (float)si.box_pos.width + si.blur * 2, (float)si.box_pos.height + si.blur * 2,
                                color_str, si.blur * 2.0f);
                            inset_tag = rect_tag;
                        }
                        tag = inset_tag;
                    }

                    if (si.blur > 0) {
                        char buf[64]; sprintf(buf, "<g filter=\"url(#shadow_filter_%d)\">", idx);
                        final_out += buf;
                    }
                    if (si.inset) {
                        char buf[64]; sprintf(buf, "<g clip-path=\"url(#shadow_clip_%d)\">", idx);
                        final_out += buf;
                    }
                    final_out += tag;
                    if (si.inset) final_out += "</g>";
                    if (si.blur > 0) final_out += "</g>";
                } else {
                    final_out.append(tag);
                }
            }
            cur = tag_end + 1;
        }

        static std::string static_out;
        static_out = final_out;
        return static_out.c_str();
    }
}
