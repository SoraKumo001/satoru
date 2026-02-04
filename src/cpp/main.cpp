#include <emscripten/emscripten.h>
#include <litehtml/litehtml.h>
#include "include/core/SkCanvas.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkBitmap.h"
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

// Global caches (kept here for now, could be further encapsulated)
static sk_sp<SkFontMgr> g_fontMgr;
static std::map<std::string, sk_sp<SkTypeface>> g_typefaceCache;
static sk_sp<SkTypeface> g_defaultTypeface;
static std::map<std::string, image_info> g_imageCache;

const char* master_css = R"##(
html { display: block; }
body { display: block; margin: 8px; line-height: 1.6; }
p { display: block; margin-top: 1em; margin-bottom: 1em; }
h1 { display: block; font-size: 2em; margin-top: 0.67em; margin-bottom: 0.67em; font-weight: bold; }
h2 { display: block; font-size: 1.5em; margin-top: 0.83em; margin-bottom: 0.83em; font-weight: bold; }
h3 { display: block; font-size: 1.17em; margin-top: 1em; margin-bottom: 1em; font-weight: bold; }
div { display: block; }
b, strong { font-weight: bold; }
i, em { font-style: italic; }
* { box-sizing: border-box; }
)##";

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
            g_typefaceCache[cleanedName] = typeface;
            if (!g_defaultTypeface) g_defaultTypeface = typeface;
        }
    }

    EMSCRIPTEN_KEEPALIVE
    void load_image(const char* name, const uint8_t* data, int size, int width, int height) {
        if (!name || !data || size <= 0) return;
        
        sk_sp<SkData> skData = SkData::MakeWithCopy(data, size);
        std::unique_ptr<SkCodec> codec = SkPngDecoder::Decode(skData, nullptr);
        if (codec) {
            SkBitmap bitmap;
            bitmap.allocPixels(codec->getInfo());
            if (codec->getPixels(codec->getInfo(), bitmap.getPixels(), bitmap.rowBytes()) == SkCodec::kSuccess) {
                image_info info;
                info.image = bitmap.asImage();
                info.width = (width > 0) ? width : info.image->width();
                info.height = (height > 0) ? height : info.image->height();
                g_imageCache[std::string(name)] = info;
                return;
            }
        }

        image_info info;
        info.width = width;
        info.height = height;
        info.image = nullptr;
        g_imageCache[std::string(name)] = info;
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_images() {
        g_imageCache.clear();
    }

    EMSCRIPTEN_KEEPALIVE
    void clear_fonts() {
        g_typefaceCache.clear();
        g_defaultTypeface = nullptr;
    }

    EMSCRIPTEN_KEEPALIVE
    const char* html_to_svg(const char* html, int width, int height) {
        int initial_height = (height > 0) ? height : 1000;
        container_skia container(width, initial_height, nullptr, 
                                 g_fontMgr, g_typefaceCache, g_defaultTypeface, g_imageCache);
        
        litehtml::document::ptr doc = litehtml::document::createFromString(html, &container, master_css);
        doc->render(width);
        
        int content_height = (height > 0) ? height : doc->height();
        if (content_height < 1) content_height = 1;
        
        container.set_height(content_height);

        SkDynamicMemoryWStream stream;
        SkRect bounds = SkRect::MakeWH((float)width, (float)content_height);
        
        std::unique_ptr<SkCanvas> canvas = SkSVGCanvas::Make(bounds, &stream, SkSVGCanvas::kConvertTextToPaths_Flag);

        container.set_canvas(canvas.get());
        doc->draw(0, 0, 0, nullptr);

        canvas.reset();

        static std::string svg_out;
        sk_sp<SkData> data = stream.detachAsData();
        svg_out.assign((const char*)data->data(), data->size());

        return svg_out.c_str();
    }
}
