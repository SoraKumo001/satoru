#ifndef SATORU_TEXT_LAYOUT_H
#define SATORU_TEXT_LAYOUT_H

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "core/text/text_types.h"

class SatoruContext;

namespace satoru {

class TextLayout {
   public:
    static TextAnalysis analyzeText(
        SatoruContext* ctx, const char* text, size_t len, font_info* fi,
        litehtml::writing_mode mode = litehtml::writing_mode_horizontal_tb,
        std::set<char32_t>* usedCodepoints = nullptr);

    static MeasureResult measureText(
        SatoruContext* ctx, const char* text, font_info* fi,
        litehtml::writing_mode mode = litehtml::writing_mode_horizontal_tb, double maxWidth = -1.0,
        std::set<char32_t>* usedCodepoints = nullptr);

    static std::string ellipsizeText(SatoruContext* ctx, const char* text, font_info* fi,
                                     litehtml::writing_mode mode, double maxWidth,
                                     std::set<char32_t>* usedCodepoints = nullptr);

    static ShapedResult shapeText(
        SatoruContext* ctx, const char* text, size_t len, font_info* fi,
        litehtml::writing_mode mode = litehtml::writing_mode_horizontal_tb,
        std::set<char32_t>* usedCodepoints = nullptr);

    static void splitText(SatoruContext* ctx, const char* text,
                          const std::function<void(const char*)>& onWord,
                          const std::function<void(const char*)>& onSpace);
};

}  // namespace satoru

#endif  // SATORU_TEXT_LAYOUT_H
