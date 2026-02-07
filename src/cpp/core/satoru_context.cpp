#include "satoru_context.h"

#include <iostream>
#include <sstream>

#include "include/codec/SkCodec.h"
#include "include/codec/SkPngDecoder.h"
#include "include/core/SkData.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkImage.h"
#include "include/ports/SkFontMgr_empty.h"
#include "utils.h"

void SatoruContext::init() {
    SkGraphics::Init();
    fontMgr = SkFontMgr_New_Custom_Empty();

    // Register codecs
    SkCodecs::Register(SkPngDecoder::Decoder());
}

void SatoruContext::loadFont(const char *name, const uint8_t *data, int size) {
    std::string cleanedName = clean_font_name(name);
    sk_sp<SkData> skData = SkData::MakeWithCopy(data, size);
    sk_sp<SkTypeface> typeface = fontMgr->makeFromData(skData);
    if (typeface) {
        typefaceCache[cleanedName].push_back(typeface);
        if (!defaultTypeface) defaultTypeface = typeface;
        fallbackTypefaces.push_back(typeface);
    }
}

void SatoruContext::loadImage(const char *name, const char *data_url, int width, int height) {
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

void SatoruContext::loadImageFromData(const char *name, const uint8_t *data_ptr, size_t size) {
    if (!name || !data_ptr || size == 0) return;

    image_info info;
    info.width = 0;
    info.height = 0;
    info.data_url = ""; // Not available

    sk_sp<SkData> skData = SkData::MakeWithCopy(data_ptr, size);
    info.skImage = SkImages::DeferredFromEncodedData(skData);

    if (info.skImage) {
        info.width = info.skImage->width();
        info.height = info.skImage->height();
        imageCache[std::string(name)] = info;
    }
}

void SatoruContext::clearImages() { imageCache.clear(); }

void SatoruContext::clearFonts() {
    typefaceCache.clear();
    fallbackTypefaces.clear();
    defaultTypeface = nullptr;
}

sk_sp<SkTypeface> SatoruContext::get_typeface(const std::string &family, int weight,
                                              SkFontStyle::Slant slant, bool &out_fake_bold) {
    auto tfs = get_typefaces(family, weight, slant, out_fake_bold);
    return tfs.empty() ? nullptr : tfs[0];
}

std::vector<sk_sp<SkTypeface>> SatoruContext::get_typefaces(const std::string &family, int weight,
                                                            SkFontStyle::Slant slant, bool &out_fake_bold) {
    std::vector<sk_sp<SkTypeface>> result;
    std::stringstream ss(family);
    std::string item;
    out_fake_bold = false;

    while (std::getline(ss, item, ',')) {
        size_t first = item.find_first_not_of(" \t\r\n'\"");
        if (first == std::string::npos) continue;
        size_t last = item.find_last_not_of(" \t\r\n'\"");
        std::string trimmed = item.substr(first, (last - first + 1));

        std::string cleaned = clean_font_name(trimmed.c_str());
        if (typefaceCache.count(cleaned)) {
            const auto &list = typefaceCache[cleaned];
            sk_sp<SkTypeface> bestMatch = nullptr;
            int minDiff = 1000;

            for (const auto &tf : list) {
                SkFontStyle style = tf->fontStyle();
                // CSS Matching heuristic:
                // 1. Slant match is prioritized
                // 2. Weight match follows
                int diff = std::abs(style.weight() - weight) + (style.slant() == slant ? 0 : 500);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestMatch = tf;
                }
            }
            if (bestMatch) {
                // If requested weight is >= 600 but best match is < 500, trigger fake bold
                if (weight >= 600 && bestMatch->fontStyle().weight() < 500) {
                    out_fake_bold = true;
                }
                result.push_back(bestMatch);
            }
        }
    }
    return result;
}

bool SatoruContext::get_image_size(const std::string &url, int &w, int &h) {
    auto it = imageCache.find(url);
    if (it != imageCache.end()) {
        w = it->second.width;
        h = it->second.height;
        return true;
    }
    return false;
}