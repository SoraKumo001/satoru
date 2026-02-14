#ifndef SVG_RENDERER_H
#define SVG_RENDERER_H

#include <string>

#include "core/satoru_context.h"

struct SatoruInstance;
std::string renderDocumentToSvg(SatoruInstance* inst, int width, int height, const RenderOptions& options);

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context,
                            const char *master_css, const RenderOptions& options);

#endif  // SVG_RENDERER_H