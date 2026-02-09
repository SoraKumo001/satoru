#include "satoru_context.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "utils/skia_utils.h"
#include "include/codec/SkAvifDecoder.h"
#include "include/codec/SkBmpDecoder.h"
#include "include/codec/SkCodec.h"
#include "include/codec/SkIcoDecoder.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkSpan.h"  // Include SkSpan header
#include "include/core/SkTypeface.h"

// External declaration for the custom empty font manager provided by Skia ports
extern sk_sp<SkFontMgr> SkFontMgr_New_Custom_Empty();

void SatoruContext::init() {
    SkCodecs::Register(SkPngDecoder::Decoder());
    SkCodecs::Register(SkJpegDecoder::Decoder());
    SkCodecs::Register(SkWebpDecoder::Decoder());
    SkCodecs::Register(SkAvifDecoder::Decoder());
    SkCodecs::Register(SkBmpDecoder::Decoder());
    SkCodecs::Register(SkIcoDecoder::Decoder());
    fontMgr = SkFontMgr_New_Custom_Empty();
    defaultTypeface = nullptr;
}

void SatoruContext::loadFont(const char *name, const uint8_t *data, int size) {
    auto data_ptr = SkData::MakeWithCopy(data, size);
    if (!fontMgr) fontMgr = SkFontMgr_New_Custom_Empty();

    auto typeface = fontMgr->makeFromData(std::move(data_ptr));
    if (typeface) {
        std::string cleaned = clean_font_name(name);
        typefaceCache[cleaned].push_back(typeface);
        if (!defaultTypeface) defaultTypeface = typeface;
    }
}

void SatoruContext::loadImage(const char *name, const char *data_url, int width, int height) {
    image_info info;
    info.data_url = data_url ? data_url : "";
    info.width = width;
    info.height = height;
    imageCache[name] = info;
}

void SatoruContext::loadImageFromData(const char *name, const uint8_t *data, size_t size) {
    auto data_ptr = SkData::MakeWithCopy(data, size);
    auto codec = SkCodec::MakeFromData(
        data_ptr, SkSpan<const SkCodecs::Decoder>({
                      SkPngDecoder::Decoder(),
                      SkJpegDecoder::Decoder(),
                      SkWebpDecoder::Decoder(),
                      SkAvifDecoder::Decoder(),
                      SkBmpDecoder::Decoder(),
                      SkIcoDecoder::Decoder(),
                  }));
    if (codec) {
        auto image = SkCodecs::DeferredImage(std::move(codec));
        if (image) {
            image_info info;
            info.data_url = "";
            info.width = image->width();
            info.height = image->height();
            info.skImage = image;
            imageCache[name] = info;
        }
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
                                                            SkFontStyle::Slant slant,
                                                            bool &out_fake_bold) {
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
                int diff = std::abs(style.weight() - weight) + (style.slant() == slant ? 0 : 500);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestMatch = tf;
                }
            }
            if (bestMatch) {
                if (result.empty()) {
                    if (weight >= 600 && bestMatch->fontStyle().weight() < 500) {
                        out_fake_bold = true;
                    }
                }
                result.push_back(bestMatch);
            }
        }
    }

    if (result.empty()) {
        out_fake_bold = false;
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