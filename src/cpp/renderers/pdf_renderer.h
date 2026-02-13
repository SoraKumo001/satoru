#ifndef PDF_RENDERER_H
#define PDF_RENDERER_H

#include <cstdint>
#include <string>
#include <vector>

#include "core/satoru_context.h"

std::vector<uint8_t> renderHtmlsToPdf(const std::vector<std::string>& htmls, int width, int height,
                                      SatoruContext &context, const char *master_css);

#endif  // PDF_RENDERER_H
