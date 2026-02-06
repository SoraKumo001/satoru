#include "satoru_context.h"
#include "utils.h"
#include "include/core/SkGraphics.h"
#include "include/ports/SkFontMgr_empty.h"
#include "include/core/SkData.h"
#include "include/codec/SkCodec.h"
#include "include/codec/SkPngDecoder.h"

void SatoruContext::init() {
    SkGraphics::Init();
    fontMgr = SkFontMgr_New_Custom_Empty();
    
    // Register codecs
    SkCodecs::Register(SkPngDecoder::Decoder());
}

void SatoruContext::loadFont(const char* name, const uint8_t* data, int size) {
    std::string cleanedName = clean_font_name(name);
    sk_sp<SkData> skData = SkData::MakeWithCopy(data, size);
    sk_sp<SkTypeface> typeface = fontMgr->makeFromData(skData);
    if (typeface) {
        typefaceCache[cleanedName].push_back(typeface);
        if (!defaultTypeface) defaultTypeface = typeface;
        fallbackTypefaces.push_back(typeface);
    }
}

void SatoruContext::loadImage(const char* name, const char* data_url, int width, int height) {
    if (!name || !data_url) return;

    image_info info;
    info.width = width;
    info.height = height;
    info.data_url = std::string(data_url);

    std::string urlStr(data_url);
    size_t commaPos = urlStr.find(',');
    if (commaPos != std::string::npos) {
        std::string base64Data = urlStr.substr(commaPos + 1);
        std::vector<uint8_t> decoded = base64_decode(base64Data);
        sk_sp<SkData> data = SkData::MakeWithCopy(decoded.data(), decoded.size());
        info.skImage = SkImages::DeferredFromEncodedData(data);
        
        if (info.skImage) {
            if (info.width <= 0) info.width = info.skImage->width();
            if (info.height <= 0) info.height = info.skImage->height();
        }
    }

    imageCache[std::string(name)] = info;
}

void SatoruContext::clearImages() {
    imageCache.clear();
}

void SatoruContext::clearFonts() {
    typefaceCache.clear();
    fallbackTypefaces.clear();
    defaultTypeface = nullptr;
}