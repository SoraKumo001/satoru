#ifndef SATORU_CONTEXT_H
#define SATORU_CONTEXT_H

#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "image_types.h"
#include <map>
#include <vector>
#include <string>

class SatoruContext {
public:
    sk_sp<SkFontMgr> fontMgr;
    std::map<std::string, std::vector<sk_sp<SkTypeface>>> typefaceCache;
    sk_sp<SkTypeface> defaultTypeface;
    std::vector<sk_sp<SkTypeface>> fallbackTypefaces;
    std::map<std::string, image_info> imageCache;

    void init();
    void loadFont(const char* name, const uint8_t* data, int size);
    void loadImage(const char* name, const char* data_url, int width, int height);
    void clearImages();
    void clearFonts();
};

#endif // SATORU_CONTEXT_H
