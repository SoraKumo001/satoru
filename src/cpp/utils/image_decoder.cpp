#include "image_decoder.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <ctre.hpp>

#include "include/codec/SkCodec.h"
#include "include/codec/SkEncodedOrigin.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/codec/SkAvifDecoder.h"
#include "include/codec/SkBmpDecoder.h"
#include "include/codec/SkGifDecoder.h"
#include "include/codec/SkIcoDecoder.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkFontMgr.h"

#include "modules/skresources/include/SkResources.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGNode.h"
#include "modules/svg/include/SkSVGSVG.h"

namespace satoru {

sk_sp<SkImage> ImageDecoder::decode(const uint8_t* data, size_t size, int& out_width, int& out_height, sk_sp<SkFontMgr> font_mgr) {
    if (!data || size == 0) return nullptr;

    auto sk_data = SkData::MakeWithCopy(data, size);

    // 1. Try raster decoding via SkCodec
    std::unique_ptr<SkCodec> codec = SkCodec::MakeFromData(sk_data);
    if (codec) {
        SkImageInfo info = codec->getInfo().makeColorType(kN32_SkColorType).makeAlphaType(kPremul_SkAlphaType);
        SkBitmap bitmap;
        if (bitmap.tryAllocPixels(info)) {
            if (codec->getPixels(info, bitmap.getPixels(), bitmap.rowBytes()) == SkCodec::kSuccess) {
                out_width = bitmap.width();
                out_height = bitmap.height();
                return bitmap.asImage();
            }
        }
    }

    // 2. Try SVG decoding
    // Heuristic: if it starts with '<', it might be SVG
    const uint8_t* p = data;
    size_t s = size;
    while (s > 0 && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
        s--;
    }

    if (s >= 4 && p[0] == '<') {
        auto patched_data = patch_svg_data(sk_data);
        return decode_svg(patched_data, out_width, out_height, font_mgr);
    }

    return nullptr;
}

sk_sp<SkData> ImageDecoder::patch_svg_data(const sk_sp<SkData>& data) {
    // Note: Complex patching with ctre caused RuntimeError in Wasm due to stack usage.
    // For now, return original data.
    return data;
}

sk_sp<SkImage> ImageDecoder::decode_svg(const sk_sp<SkData>& data, int& out_width, int& out_height, sk_sp<SkFontMgr> font_mgr) {
    SkMemoryStream stream(data);
    auto resource_provider = skresources::DataURIResourceProviderProxy::Make(
        nullptr, skresources::ImageDecodeStrategy::kLazyDecode);
    
    auto svg_dom = SkSVGDOM::Builder()
                       .setResourceProvider(std::move(resource_provider))
                       .setFontManager(font_mgr ? font_mgr : SkFontMgr::RefEmpty())
                       .make(stream);

    if (!svg_dom) return nullptr;

    auto container_size = svg_dom->containerSize();
    if (container_size.isEmpty()) {
        container_size = SkSize::Make(512, 512); // Default fallback
    }
    
    svg_dom->setContainerSize(container_size);
    out_width = (int)container_size.width();
    out_height = (int)container_size.height();

    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::MakeN32Premul(out_width, out_height))) {
        return nullptr;
    }

    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    svg_dom->render(&canvas);

    return bitmap.asImage();
}

} // namespace satoru
