#ifndef PDF_RENDERER_H
#define PDF_RENDERER_H

#include <cstdint>
#include <vector>

#include "satoru_context.h"

std::vector<uint8_t> renderHtmlToPdf(const char *html, int width, int height,
                                     SatoruContext &context, const char *master_css);

#endif  // PDF_RENDERER_H
