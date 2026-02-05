#include "satoru_context.h"
#include "utils.h"
#include "include/core/SkGraphics.h"
#include "include/ports/SkFontMgr_empty.h"
#include "include/core/SkData.h"

void SatoruContext::init() {
    SkGraphics::Init();
    fontMgr = SkFontMgr_New_Custom_Empty();
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
