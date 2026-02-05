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
#include <regex>
#include <set>

static sk_sp<SkFontMgr> g_fontMgr;
static std::map<std::string, std::vector<sk_sp<SkTypeface>>> g_typefaceCache;
static sk_sp<SkTypeface> g_defaultTypeface;
static std::vector<sk_sp<SkTypeface>> g_fallbackTypefaces;
static std::map<std::string, image_info> g_imageCache;

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

        // Post-process SVG
        std::set<int> used_shadow_indices;
        std::set<int> used_image_indices;
        static const std::regex shd_marker_scan_regex(R"raw((?:fill|stroke)="#01([0-9A-Fa-f]{4})")raw");
        static const std::regex img_marker_scan_regex(R"raw((?:fill|stroke)="#00([0-9A-Fa-f]{4})")raw");
        
        auto shd_scan_begin = std::sregex_iterator(svg_str.begin(), svg_str.end(), shd_marker_scan_regex);
        auto scan_end = std::sregex_iterator();
        for (std::sregex_iterator i = shd_scan_begin; i != scan_end; ++i) {
            used_shadow_indices.insert(std::stoi((*i)[1].str(), nullptr, 16));
        }

        auto img_scan_begin = std::sregex_iterator(svg_str.begin(), svg_str.end(), img_marker_scan_regex);
        for (std::sregex_iterator i = img_scan_begin; i != scan_end; ++i) {
            used_image_indices.insert(std::stoi((*i)[1].str(), nullptr, 16));
        }

        std::string defs = "<defs>";
        for (int idx : used_shadow_indices) {
            const auto& si = container.get_shadow_info(idx);
            
            if (si.blur > 0) {
                char filter_id[64];
                sprintf(filter_id, "shadow_filter_%d", idx);
                defs += "<filter id=\"";
                defs += filter_id;
                defs += "\" x=\"-50%\" y=\"-50%\" width=\"200%\" height=\"200%\">";
                defs += "<feGaussianBlur stdDeviation=\"";
                char dev[32];
                sprintf(dev, "%.2f", si.blur * 0.5f);
                defs += dev;
                defs += "\"/>";
                defs += "</filter>";
            }

            if (si.inset) {
                char clip_id[64];
                sprintf(clip_id, "shadow_clip_%d", idx);
                defs += "<clipPath id=\"";
                defs += clip_id;
                defs += "\">";
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
                    defs += "<path d=\"";
                    defs += path_data;
                    defs += "\"/>";
                } else {
                    char rect_data[256];
                    sprintf(rect_data, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\"/>",
                        (float)si.box_pos.x, (float)si.box_pos.y, (float)si.box_pos.width, (float)si.box_pos.height);
                    defs += rect_data;
                }
                defs += "</clipPath>";
            }
        }

        for (int idx : used_image_indices) {
            const auto& idi = container.get_image_draw_info(idx);
            
            // Handle rounded corners for image
            const auto& br = idi.layer.border_radius;
            if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 || br.bottom_right_x > 0) {
                char clip_id[64];
                sprintf(clip_id, "image_clip_%d", idx);
                defs += "<clipPath id=\"";
                defs += clip_id;
                defs += "\">";
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
                defs += "<path d=\"";
                defs += path_data;
                defs += "\"/>";
                defs += "</clipPath>";
            }
        }
        defs += "</defs>";

        static const std::regex tag_regex(R"raw(<(rect|path|image|circle|ellipse) ([^>]*?)/?>)raw");
        static const std::regex img_marker_regex(R"raw(#00([0-9A-Fa-f]{4}))raw");
        static const std::regex shd_marker_regex(R"raw(#01([0-9A-Fa-f]{4}))raw");
        static const std::regex fill_attr_regex(R"raw(fill="#00[0-9A-Fa-f]{4}")raw");

        auto tags_begin = std::sregex_iterator(svg_str.begin(), svg_str.end(), tag_regex);
        auto tags_end = std::sregex_iterator();

        std::string processed_svg;
        processed_svg.reserve(svg_str.size() + defs.size());

        size_t last_pos = 0;

        size_t svg_tag_end = svg_str.find('>', svg_str.find("<svg"));
        if (svg_tag_end != std::string::npos) {
            processed_svg.append(svg_str.data(), svg_tag_end + 1);
            processed_svg.append(defs);
            last_pos = svg_tag_end + 1;
        }

        for (std::sregex_iterator i = tags_begin; i != tags_end; ++i) {
            const std::smatch& match = *i;
            const std::string& attributes = match[2].str();
            
            processed_svg.append(svg_str.data() + last_pos, match.position() - last_pos);

            // Fast-path: check if any marker exists in attributes
            if (attributes.find("#0") == std::string::npos) {
                processed_svg.append(match[0].str());
            } else {
                std::smatch img_match;
                if (std::regex_search(attributes, img_match, img_marker_regex)) {
                    int img_idx = std::stoi(img_match[1].str(), nullptr, 16);
                    const auto& idi = container.get_image_draw_info(img_idx);
                    std::string data_url = g_imageCache[idi.url].data_url;
                    
                    // Replace marker with image tag, keeping other attributes
                    std::string new_tag = match[0].str();
                    new_tag = std::regex_replace(new_tag, std::regex(R"raw(<(rect|path|image|circle|ellipse))raw"), "<image");
                    new_tag = std::regex_replace(new_tag, fill_attr_regex, "");
                    
                    // Set image coordinates and size based on origin_box
                    char pos_attrs[256];
                    sprintf(pos_attrs, "x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\"",
                        (float)idi.layer.origin_box.x, (float)idi.layer.origin_box.y, 
                        (float)idi.layer.origin_box.width, (float)idi.layer.origin_box.height);
                    
                    new_tag = std::regex_replace(new_tag, std::regex(R"raw(x="[^"]*")raw"), "");
                    new_tag = std::regex_replace(new_tag, std::regex(R"raw(y="[^"]*")raw"), "");
                    new_tag = std::regex_replace(new_tag, std::regex(R"raw(width="[^"]*")raw"), "");
                    new_tag = std::regex_replace(new_tag, std::regex(R"raw(height="[^"]*")raw"), "");
                    
                    size_t first_space = new_tag.find(' ');
                    if (first_space != std::string::npos) {
                        new_tag.insert(first_space + 1, std::string(pos_attrs) + " ");
                    }

                    static const std::regex self_close_regex(R"raw(/>)raw");
                    if (std::regex_search(new_tag, self_close_regex)) {
                        new_tag = std::regex_replace(new_tag, self_close_regex, " xlink:href=\"" + data_url + "\"/>");
                    } else {
                        size_t last_gt = new_tag.find_last_of('>');
                        if (last_gt != std::string::npos) {
                            new_tag.insert(last_gt, " xlink:href=\"" + data_url + "\" /");
                        }
                    }

                    // Wrap with clipPath if needed
                    const auto& br = idi.layer.border_radius;
                    if (br.top_left_x > 0 || br.top_right_x > 0 || br.bottom_left_x > 0 || br.bottom_right_x > 0) {
                        char clip_url[64];
                        sprintf(clip_url, "url(#image_clip_%d)", img_idx);
                        new_tag = "<g clip-path=\"" + std::string(clip_url) + "\">" + new_tag + "</g>";
                    }
                    
                    processed_svg += new_tag;
                } else {
                    std::smatch shd_match;
                    if (std::regex_search(attributes, shd_match, shd_marker_regex)) {
                        int shd_idx = std::stoi(shd_match[1].str(), nullptr, 16);
                        const auto& si = container.get_shadow_info(shd_idx);
                        char color_str[64];
                        sprintf(color_str, "rgba(%d,%d,%d,%.2f)", si.color.red, si.color.green, si.color.blue, si.color.alpha / 255.0f);
                        
                        std::string new_tag = match[0].str();
                        new_tag = std::regex_replace(new_tag, shd_marker_regex, color_str);
                        
                        if (si.inset) {
                            char path_data[512];
                            if (si.box_radius.top_left_x > 0 || si.box_radius.top_right_x > 0 || si.box_radius.bottom_left_x > 0 || si.box_radius.bottom_right_x > 0) {
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
                                new_tag = "<path d=\"" + std::string(path_data) + "\" fill=\"none\" stroke=\"" + color_str + "\" stroke-width=\"" + std::to_string(si.blur * 2) + "\"/>";
                            } else {
                                char rect_data[256];
                                sprintf(rect_data, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\"/>",
                                    (float)si.box_pos.x + si.x - si.blur, (float)si.box_pos.y + si.y - si.blur,
                                    (float)si.box_pos.width + si.blur * 2, (float)si.box_pos.height + si.blur * 2,
                                    color_str, si.blur * 2);
                                new_tag = rect_data;
                            }
                        }

                        std::string wrapper = new_tag;
                        if (si.blur > 0) {
                            char filter_url[64];
                            sprintf(filter_url, "url(#shadow_filter_%d)", shd_idx);
                            wrapper = "<g filter=\"" + std::string(filter_url) + "\">" + wrapper + "</g>";
                        }
                        if (si.inset) {
                            char clip_url[64];
                            sprintf(clip_url, "url(#shadow_clip_%d)", shd_idx);
                            wrapper = "<g clip-path=\"" + std::string(clip_url) + "\">" + wrapper + "</g>";
                        }
                        processed_svg += wrapper;
                    } else {
                        processed_svg.append(match[0].str());
                    }
                }
            }
            last_pos = match.position() + match.length();
        }
        processed_svg.append(svg_str.data() + last_pos, svg_str.size() - last_pos);

        static std::string final_out;
        final_out = processed_svg;
        return final_out.c_str();
    }
}