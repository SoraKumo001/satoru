#ifndef SATORU_TEXT_LAYOUT_H
#define SATORU_TEXT_LAYOUT_H

#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "core/text/text_types.h"

class SatoruContext;

namespace satoru {

class TextLayout {
   public:
    // Measures the text width. If max_width is provided (>= 0), stops when width exceeds max_width.
    static MeasureResult measureText(SatoruContext* ctx, const char* text, font_info* fi,
                                     double maxWidth = -1.0,
                                     std::set<char32_t>* usedCodepoints = nullptr);

    // Ellipsizes the text to fit within maxWidth.
    static std::string ellipsizeText(SatoruContext* ctx, const char* text, font_info* fi,
                                     double maxWidth, std::set<char32_t>* usedCodepoints = nullptr);

    // Core shaping logic that produces a ShapedResult.
    static ShapedResult shapeText(SatoruContext* ctx, const char* text, size_t len, font_info* fi,
                                  std::set<char32_t>* usedCodepoints = nullptr);

    // Splits text into words and spaces (for litehtml's split_text)
    static void splitText(SatoruContext* ctx, const char* text,
                          const std::function<void(const char*)>& onWord,
                          const std::function<void(const char*)>& onSpace);

   private:
    static TextAnalysis analyzeText(SatoruContext* ctx, const char* text, size_t len, font_info* fi,
                                    std::set<char32_t>* usedCodepoints);
};

}  // namespace satoru

#endif  // SATORU_TEXT_LAYOUT_H
