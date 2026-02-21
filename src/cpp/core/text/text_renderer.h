#ifndef SATORU_TEXT_RENDERER_H
#define SATORU_TEXT_RENDERER_H

#include <set>
#include <vector>

#include "bridge/bridge_types.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkTextBlob.h"
#include "libs/litehtml/include/litehtml.h"

class SatoruContext;

namespace satoru {

class TextBatcher {
   public:
    struct Style {
        font_info* fi;
        litehtml::web_color color;
        float opacity;
        bool tagging;

        bool operator==(const Style& other) const {
            return fi == other.fi && color == other.color && opacity == other.opacity &&
                   tagging == other.tagging;
        }
        bool operator!=(const Style& other) const { return !(*this == other); }
    };

    TextBatcher(SatoruContext* ctx, SkCanvas* canvas)
        : m_ctx(ctx), m_canvas(canvas), m_active(false) {
        m_currentStyle = {nullptr, {0, 0, 0, 0}, 0.0f, false};
    }

    void addText(const sk_sp<SkTextBlob>& blob, double tx, double ty, const Style& style);
    void flush();
    bool isActive() const { return m_active; }

   private:
    void addBlobToBuilder(const sk_sp<SkTextBlob>& blob, double tx, double ty);

    SatoruContext* m_ctx;
    SkCanvas* m_canvas;
    Style m_currentStyle;
    SkTextBlobBuilder m_builder;

    sk_sp<SkTextBlob> m_firstBlob;
    double m_firstTx, m_firstTy;

    bool m_active;
};

class TextRenderer {
   public:
    static void drawText(SatoruContext* ctx, SkCanvas* canvas, const char* text, font_info* fi,
                         const litehtml::web_color& color, const litehtml::position& pos,
                         litehtml::text_overflow overflow, litehtml::direction dir, bool tagging,
                         float currentOpacity, std::vector<text_shadow_info>& usedTextShadows,
                         std::vector<text_draw_info>& usedTextDraws,
                         std::set<char32_t>* usedCodepoints, TextBatcher* batcher = nullptr);

   private:
    // Internal helper for shaping and drawing a single run of text
    static double drawTextInternal(SatoruContext* ctx, SkCanvas* canvas, const char* str,
                                   size_t strLen, font_info* fi, double tx, double ty,
                                   const SkPaint& paint, bool tagging,
                                   std::vector<text_draw_info>& usedTextDraws,
                                   std::set<char32_t>* usedCodepoints,
                                   TextBatcher* batcher = nullptr);

    static void drawDecoration(SkCanvas* canvas, font_info* fi, const litehtml::position& pos,
                               const litehtml::web_color& color, double finalWidth);
};

}  // namespace satoru

#endif  // SATORU_TEXT_RENDERER_H
