#include "satoru_context.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "include/codec/SkAvifDecoder.h"
#include "include/codec/SkBmpDecoder.h"
#include "include/codec/SkCodec.h"
#include "include/codec/SkIcoDecoder.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkSpan.h"
#include "utils/skia_utils.h"

void SatoruContext::init() {
    SkCodecs::Register(SkPngDecoder::Decoder());
    SkCodecs::Register(SkJpegDecoder::Decoder());
    SkCodecs::Register(SkWebpDecoder::Decoder());
    SkCodecs::Register(SkAvifDecoder::Decoder());
    SkCodecs::Register(SkBmpDecoder::Decoder());
    SkCodecs::Register(SkIcoDecoder::Decoder());
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
    auto codec = SkCodec::MakeFromData(data_ptr, SkSpan<const SkCodecs::Decoder>({
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

sk_sp<SkTypeface> SatoruContext::get_typeface(const std::string &family, int weight,
                                              SkFontStyle::Slant slant, bool &out_fake_bold) {
    auto tfs = get_typefaces(family, weight, slant, out_fake_bold);
    return tfs.empty() ? nullptr : tfs[0];
}

std::vector<sk_sp<SkTypeface>> SatoruContext::get_typefaces(const std::string &family, int weight,
                                                            SkFontStyle::Slant slant,
                                                            bool &out_fake_bold) {
    out_fake_bold = false;
    auto result = fontManager.matchFonts(family, weight, slant);

    if (!result.empty()) {
        // Fake bold check (only for non-variable fonts, though this flag will be 
        // overridden in container_skia if Variable Font cloning succeeds)
        if (weight >= 600 && result[0]->fontStyle().weight() < 500) {
            out_fake_bold = true;
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
