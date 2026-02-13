#ifndef WEBP_RENDERER_H
#define WEBP_RENDERER_H

#include "core/satoru_context.h"
#include "include/core/SkData.h"

sk_sp<SkData> renderHtmlToWebp(const char *html, int width, int height, SatoruContext &context,
                               const char *master_css = nullptr);

#endif
