#pragma once

#include <algorithm>

#include "../bridge/bridge_types.h"
#include "include/core/SkCanvas.h"

inline void apply_resize_transform(SkCanvas* canvas, int src_w, int src_h,
                                   const RenderOptions& options) {
    if (options.outputWidth > 0 || options.outputHeight > 0) {
        float sx = (float)options.outputWidth / src_w;
        float sy = (float)options.outputHeight / src_h;
        float scaleX = sx, scaleY = sy;
        float dx = 0, dy = 0;

        if (options.fitType == 0) {  // contain
            scaleX = scaleY = std::min(sx, sy);
            dx = (options.outputWidth - src_w * scaleX) * options.fitPositionX;
            dy = (options.outputHeight - src_h * scaleY) * options.fitPositionY;
        } else if (options.fitType == 1) {  // cover
            scaleX = scaleY = std::max(sx, sy);
            dx = (options.outputWidth - src_w * scaleX) * options.fitPositionX;
            dy = (options.outputHeight - src_h * scaleY) * options.fitPositionY;
        }

        canvas->translate(dx, dy);
        canvas->scale(scaleX, scaleY);
    }
}
