#ifndef WEBP_RENDERER_H
#define WEBP_RENDERER_H

#include "core/satoru_context.h"
#include "include/core/SkData.h"

struct SatoruInstance;
sk_sp<SkData> renderDocumentToWebp(SatoruInstance* inst, int width, int height, const RenderOptions& options);

sk_sp<SkData> renderHtmlToWebp(const char *html, int width, int height, SatoruContext &context,
                               const char *master_css = nullptr);

#endif
