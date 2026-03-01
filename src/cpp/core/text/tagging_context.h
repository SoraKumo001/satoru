#ifndef SATORU_TAGGING_CONTEXT_H
#define SATORU_TAGGING_CONTEXT_H

#include <set>
#include <vector>

#include "bridge/bridge_types.h"
#include "bridge/magic_tags.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkFont.h"
#include "libs/litehtml/include/litehtml.h"

namespace satoru {

/**
 * テキスト描画時のタグ付け（マジックカラーによるID埋め込み）を管理するクラス
 */
class TaggingContext {
   public:
    TaggingContext(SkCanvas* canvas,
                   std::vector<SkPath>& usedGlyphs,
                   std::vector<glyph_draw_info>& usedGlyphDraws,
                   int styleTag,
                   int styleIndex)
        : m_canvas(canvas),
          m_usedGlyphs(usedGlyphs),
          m_usedGlyphDraws(usedGlyphDraws),
          m_styleTag(styleTag),
          m_styleIndex(styleIndex) {}

    /**
     * 指定された位置にグリフをタグ付きで描画する
     */
    void drawGlyph(const SkFont& font,
                   SkGlyphID glyphId,
                   float phys_x,
                   float phys_y,
                   float rotation,
                   const SkPaint& basePaint);

   private:
    SkCanvas* m_canvas;
    std::vector<SkPath>& m_usedGlyphs;
    std::vector<glyph_draw_info>& m_usedGlyphDraws;
    int m_styleTag;
    int m_styleIndex;

    int getOrAddGlyphPath(const SkPath& path);
};

}  // namespace satoru

#endif  // SATORU_TAGGING_CONTEXT_H
