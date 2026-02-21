#ifndef SATORU_TEXT_RENDERER_H
#define SATORU_TEXT_RENDERER_H

#include <set>
#include <vector>

#include "bridge/bridge_types.h"
#include "include/core/SkCanvas.h"
#include "libs/litehtml/include/litehtml.h"

class SatoruContext;

namespace satoru {

class TextRenderer {
   public:
    static void drawText(SatoruContext* ctx, SkCanvas* canvas, const char* text, font_info* fi,
                         const litehtml::web_color& color, const litehtml::position& pos,
                         litehtml::text_overflow overflow, litehtml::direction dir, bool tagging,
                         float currentOpacity, std::vector<text_shadow_info>& usedTextShadows,
                         std::vector<text_draw_info>& usedTextDraws,
                         std::set<char32_t>* usedCodepoints);

   private:
    // Internal helper for shaping and drawing a single run of text
    static double drawTextInternal(SatoruContext* ctx, SkCanvas* canvas, const char* str,
                                   size_t strLen, font_info* fi, double tx, double ty,
                                   const SkPaint& paint, bool tagging,
                                   std::vector<text_draw_info>& usedTextDraws,
                                   std::set<char32_t>* usedCodepoints);

    static void drawDecoration(SkCanvas* canvas, font_info* fi, const litehtml::position& pos,
                               const litehtml::web_color& color, double finalWidth);
};

}  // namespace satoru

#endif  // SATORU_TEXT_RENDERER_H
