#ifndef PNG_RENDERER_H
#define PNG_RENDERER_H

#include <string>

#include "core/satoru_context.h"
#include "include/core/SkData.h"

struct SatoruInstance;
sk_sp<SkData> renderDocumentToPng(SatoruInstance *inst, int width, int height,
                                  const RenderOptions &options);

sk_sp<SkData> renderHtmlToPng(const char *html, int width, int height, SatoruContext &context,
                              const char *master_css);

#endif  // PNG_RENDERER_H
