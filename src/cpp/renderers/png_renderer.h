#ifndef PNG_RENDERER_H
#define PNG_RENDERER_H

#include <string>

#include "core/satoru_context.h"
#include "include/core/SkData.h"

std::string renderHtmlToPng(const char *html, int width, int height, SatoruContext &context,
                            const char *master_css);

sk_sp<SkData> renderHtmlToPngBinary(const char *html, int width, int height, SatoruContext &context,
                                    const char *master_css);

#endif  // PNG_RENDERER_H