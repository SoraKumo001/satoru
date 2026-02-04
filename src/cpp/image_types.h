#ifndef IMAGE_TYPES_H
#define IMAGE_TYPES_H

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"
#include <string>

struct image_info {
    int width;
    int height;
    std::string data_url;
};

#endif // IMAGE_TYPES_H
