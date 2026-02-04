#ifndef IMAGE_TYPES_H
#define IMAGE_TYPES_H

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

struct image_info {
    sk_sp<SkImage> image;
    int width;
    int height;
};

#endif // IMAGE_TYPES_H
