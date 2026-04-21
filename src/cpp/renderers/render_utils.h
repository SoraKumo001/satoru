#pragma once

#include <algorithm>

#include "include/core/SkCanvas.h"

inline void apply_resize_transform(SkCanvas* canvas, int src_w, int src_h, int out_width,
                                   int out_height, int fitType) {
    if (out_width > 0 || out_height > 0) {
        float sx = (float)out_width / src_w;
        float sy = (float)out_height / src_h;
        float scaleX = sx, scaleY = sy;
        float dx = 0, dy = 0;

        if (fitType == 0) {  // contain
            scaleX = scaleY = std::min(sx, sy);
            dx = (out_width - src_w * scaleX) / 2.0f;
            dy = (out_height - src_h * scaleY) / 2.0f;
        } else if (fitType == 1) {  // cover
            scaleX = scaleY = std::max(sx, sy);
            dx = (out_width - src_w * scaleX) / 2.0f;
            dy = (out_height - src_h * scaleY) / 2.0f;
        }

        canvas->translate(dx, dy);
        canvas->scale(scaleX, scaleY);
    }
}
