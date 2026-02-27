#ifndef SKIA_UTILS_H
#define SKIA_UTILS_H

#include <cstdint>
#include <string>
#include <vector>

#include "include/core/SkImage.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRefCnt.h"
#include "litehtml.h"

struct image_info {
    int width;
    int height;
    std::string data_url;
    sk_sp<SkImage> skImage;
};

std::string clean_font_name(const char* name);
std::string base64_encode(const uint8_t* data, size_t len);
std::vector<uint8_t> base64_decode(const std::string& in);
std::string url_decode(const std::string& in);

SkRRect make_rrect(const litehtml::position& pos, const litehtml::border_radiuses& radius);

#endif  // SKIA_UTILS_H
