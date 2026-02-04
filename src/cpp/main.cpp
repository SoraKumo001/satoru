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
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkCodec.h"

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
        SkPngDecoder::Decoder();
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
        std::regex marker_regex("(fill|stroke)=\"(#0[01]00[0-9A-Fa-f]{2})\"");
        auto markers_begin = std::sregex_iterator(svg_str.begin(), svg_str.end(), marker_regex);
        auto markers_end = std::sregex_iterator();
        for (std::sregex_iterator i = markers_begin; i != markers_end; ++i) {
            std::string marker = (*i)[2].str();
            if (marker.substr(0, 3) == "#01") {
                int index = std::stoi(marker.substr(5), nullptr, 16);
                used_shadow_indices.insert(index);
            }
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
                    sprintf(path_data, "M%d %d h%d q%d 0 %d %d v%d q0 %d %d %d h%d q%d 0 %d %d v%d q0 %d %d %d Z",
                        (int)(si.box_pos.x + si.box_radius.top_left_x), (int)si.box_pos.y,
                        (int)(si.box_pos.width - si.box_radius.top_left_x - si.box_radius.top_right_x),
                        (int)si.box_radius.top_right_x, (int)si.box_radius.top_right_x, (int)si.box_radius.top_right_y,
                        (int)(si.box_pos.height - si.box_radius.top_right_y - si.box_radius.bottom_right_y),
                        0, (int)-si.box_radius.bottom_right_x, (int)si.box_radius.bottom_right_y,
                        (int)-(si.box_pos.width - si.box_radius.bottom_left_x - si.box_radius.bottom_right_x),
                        (int)-si.box_radius.bottom_left_x, (int)-si.box_radius.bottom_left_x, (int)-si.box_radius.bottom_left_y,
                        (int)-(si.box_pos.height - si.box_radius.top_left_y - si.box_radius.bottom_left_y),
                        0, (int)si.box_radius.top_left_x, (int)-si.box_radius.top_left_y);
                    defs += "<path d=\"";
                    defs += path_data;
                    defs += "\"/>";
                } else {
                    defs += "<rect x=\"";
                    defs += std::to_string((int)si.box_pos.x);
                    defs += "\" y=\"";
                    defs += std::to_string((int)si.box_pos.y);
                    defs += "\" width=\"";
                    defs += std::to_string((int)si.box_pos.width);
                    defs += "\" height=\"";
                    defs += std::to_string((int)si.box_pos.height);
                    defs += "\"/>";
                }
                defs += "</clipPath>";
            }
        }
        defs += "</defs>";

        std::regex tag_regex("<(rect|path|image|circle|ellipse) ([^>]*?)/?>");
        auto tags_begin = std::sregex_iterator(svg_str.begin(), svg_str.end(), tag_regex);
        auto tags_end = std::sregex_iterator();

        std::string processed_svg;
        size_t last_pos = 0;

        size_t svg_tag_end = svg_str.find('>', svg_str.find("<svg"));
        if (svg_tag_end != std::string::npos) {
            processed_svg += svg_str.substr(0, svg_tag_end + 1);
            processed_svg += defs;
            last_pos = svg_tag_end + 1;
        }

        for (std::sregex_iterator i = tags_begin; i != tags_end; ++i) {
            std::smatch match = *i;
            std::string tag_name = match[1].str();
            std::string attributes = match[2].str();
            
            processed_svg += svg_str.substr(last_pos, match.position() - last_pos);

            bool is_image = false;
            int img_idx = -1;
            std::regex img_marker_regex("#0000([0-9A-Fa-f]{2})");
            std::smatch img_match;
            if (std::regex_search(attributes, img_match, img_marker_regex)) {
                is_image = true;
                img_idx = std::stoi(img_match[1].str(), nullptr, 16);
            }

            bool is_shadow = false;
            int shd_idx = -1;
            std::regex shd_marker_regex("#0100([0-9A-Fa-f]{2})");
            std::smatch shd_match;
            if (std::regex_search(attributes, shd_match, shd_marker_regex)) {
                is_shadow = true;
                shd_idx = std::stoi(shd_match[1].str(), nullptr, 16);
            }

            if (is_image) {
                std::string data_url = g_imageCache[container.get_image_url(img_idx)].data_url;
                std::regex fill_regex("fill=\"#0000[0-9A-Fa-f]{2}\"");
                std::string new_attributes = std::regex_replace(attributes, fill_regex, "");
                processed_svg += "<image " + new_attributes + " xlink:href=\"" + data_url + "\"/>";
            } else if (is_shadow) {
                const auto& si = container.get_shadow_info(shd_idx);
                char color_str[64];
                sprintf(color_str, "rgba(%d,%d,%d,%.2f)", si.color.red, si.color.green, si.color.blue, si.color.alpha / 255.0f);
                
                std::string new_attributes = std::regex_replace(attributes, shd_marker_regex, color_str);
                
                std::string shadow_element;
                if (si.inset) {
                    char path_data[512];
                    if (si.box_radius.top_left_x > 0 || si.box_radius.top_right_x > 0 || si.box_radius.bottom_left_x > 0 || si.box_radius.bottom_right_x > 0) {
                        sprintf(path_data, "M%d %d h%d q%d 0 %d %d v%d q0 %d %d %d h%d q%d 0 %d %d v%d q0 %d %d %d Z",
                            (int)(si.box_pos.x + (int)si.x + si.box_radius.top_left_x), (int)(si.box_pos.y + (int)si.y),
                            (int)(si.box_pos.width - si.box_radius.top_left_x - si.box_radius.top_right_x),
                            (int)si.box_radius.top_right_x, (int)si.box_radius.top_right_x, (int)si.box_radius.top_right_y,
                            (int)(si.box_pos.height - si.box_radius.top_right_y - si.box_radius.bottom_right_y),
                            0, (int)-si.box_radius.bottom_right_x, (int)si.box_radius.bottom_right_y,
                            (int)-(si.box_pos.width - si.box_radius.bottom_left_x - si.box_radius.bottom_right_x),
                            (int)-si.box_radius.bottom_left_x, (int)-si.box_radius.bottom_left_x, (int)-si.box_radius.bottom_left_y,
                            (int)-(si.box_pos.height - si.box_radius.top_left_y - si.box_radius.bottom_left_y),
                            0, (int)si.box_radius.top_left_x, (int)-si.box_radius.top_left_y);
                        shadow_element = "<path d=\"" + std::string(path_data) + "\" fill=\"none\" stroke=\"" + color_str + "\" stroke-width=\"" + std::to_string(si.blur * 2) + "\"/>";
                    } else {
                        shadow_element = "<rect x=\"" + std::to_string((int)si.box_pos.x + (int)si.x - (int)si.blur) + 
                                         "\" y=\"" + std::to_string((int)si.box_pos.y + (int)si.y - (int)si.blur) + 
                                         "\" width=\"" + std::to_string((int)si.box_pos.width + (int)si.blur * 2) + 
                                         "\" height=\"" + std::to_string((int)si.box_pos.height + (int)si.blur * 2) + 
                                         "\" fill=\"none\" stroke=\"" + color_str + "\" stroke-width=\"" + std::to_string(si.blur * 2) + "\"/>";
                    }
                } else {
                    shadow_element = "<" + tag_name + " " + new_attributes + "/>";
                }

                std::string wrapper = shadow_element;
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
                processed_svg += match.str();
            }
            last_pos = match.position() + match.length();
        }
        processed_svg += svg_str.substr(last_pos);

        static std::string final_out;
        final_out = processed_svg;
        return final_out.c_str();
    }
}