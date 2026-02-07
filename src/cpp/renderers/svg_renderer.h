#ifndef SVG_RENDERER_H
#define SVG_RENDERER_H

#include <string>

#include "satoru_context.h"

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context,
                            const char *master_css);

#endif  // SVG_RENDERER_H