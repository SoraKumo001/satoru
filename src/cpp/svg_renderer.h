#ifndef SVG_RENDERER_H
#define SVG_RENDERER_H

#include "satoru_context.h"
#include <string>

std::string renderHtmlToSvg(const char* html, int width, int height, SatoruContext& context);

#endif // SVG_RENDERER_H
