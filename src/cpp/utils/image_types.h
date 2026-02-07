#ifndef IMAGE_TYPES_H
#define IMAGE_TYPES_H

#include <string>

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

struct image_info {
    int width;
    int height;
    std::string data_url;
    sk_sp<SkImage> skImage;
};

#endif  // IMAGE_TYPES_H