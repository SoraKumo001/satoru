#ifndef IMAGE_DECODER_H
#define IMAGE_DECODER_H

#include <cstdint>
#include <string>
#include <vector>

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkData.h"

#include "include/core/SkFontMgr.h"

namespace satoru {

class ImageDecoder {
public:
    /**
     * @brief Decodes an image from data. Supports SVG and raster formats (PNG, JPEG, WebP, etc.).
     * 
     * @param data Pointer to the image data.
     * @param size Size of the image data.
     * @param out_width Output width of the decoded image.
     * @param out_height Output height of the decoded image.
     * @param font_mgr Optional font manager for SVG text rendering.
     * @return sk_sp<SkImage> The decoded image, or nullptr if decoding failed.
     */
    static sk_sp<SkImage> decode(const uint8_t* data, size_t size, int& out_width, int& out_height, sk_sp<SkFontMgr> font_mgr = nullptr);

private:
    /**
     * @brief Patches SVG data to workaround issues in Skia's SVG DOM or to add features.
     * 
     * @param data Original SVG data.
     * @return sk_sp<SkData> Patched SVG data.
     */
    static sk_sp<SkData> patch_svg_data(const sk_sp<SkData>& data);

    /**
     * @brief Decodes SVG data to SkImage.
     * 
     * @param data SVG data.
     * @param out_width Output width.
     * @param out_height Output height.
     * @return sk_sp<SkImage> Decoded image.
     */
    static sk_sp<SkImage> decode_svg(const sk_sp<SkData>& data, int& out_width, int& out_height, sk_sp<SkFontMgr> font_mgr);
};

} // namespace satoru

#endif // IMAGE_DECODER_H
