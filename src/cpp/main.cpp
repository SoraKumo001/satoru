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

        // Post-process SVG to replace marker rectangles with actual images
        std::regex rect_regex("<rect ([^>]*?)/?>");
        auto words_begin = std::sregex_iterator(svg_str.begin(), svg_str.end(), rect_regex);
        auto words_end = std::sregex_iterator();

        std::string processed_svg;
        size_t last_pos = 0;

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string attributes = match[1].str();
            
            bool found = false;
            std::string data_url;
            for (int j = 1; j <= (int)container.get_image_count(); ++j) {
                char hex_upper[16], hex_lower[16], rgb[32];
                sprintf(hex_upper, "#0000%02X", j);
                sprintf(hex_lower, "#0000%02x", j);
                sprintf(rgb, "rgb(0,0,%d)", j);
                
                if (attributes.find(hex_upper) != std::string::npos || 
                    attributes.find(hex_lower) != std::string::npos || 
                    attributes.find(rgb) != std::string::npos) {
                    found = true;
                    data_url = g_imageCache[container.get_image_url(j)].data_url;
                    break;
                }
            }

            processed_svg += svg_str.substr(last_pos, match.position() - last_pos);
            if (found) {
                std::regex fill_regex("fill=\"(.*?)\"");
                std::string new_attributes = std::regex_replace(attributes, fill_regex, "");
                processed_svg += "<image " + new_attributes + " xlink:href=\"" + data_url + "\"/>";
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