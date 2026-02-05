#ifndef PNG_RENDERER_H
#define PNG_RENDERER_H

#include "satoru_context.h"
#include <string>

std::string renderHtmlToPng(const char* html, int width, int height, SatoruContext& context);

#endif // PNG_RENDERER_H