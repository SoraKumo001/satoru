#ifndef SATORU_TEXT_GEOMETRY_H
#define SATORU_TEXT_GEOMETRY_H

#include "bridge/bridge_types.h"
#include "core/logical_geometry.h"
#include "include/core/SkFont.h"
#include "libs/litehtml/include/litehtml.h"

namespace satoru {

/**
 * グリフの描画位置と回転を保持する構造体
 */
struct GlyphPlacement {
    float x;
    float y;
    float rotation;
};

/**
 * 論理的なテキストレイアウト座標を物理的な Skia 座標に変換するユーティリティ
 */
class TextGeometry {
   public:
    TextGeometry(litehtml::writing_mode mode, const litehtml::position& line_pos, font_info* fi)
        : m_mode(mode),
          m_line_pos(line_pos),
          m_fi(fi),
          m_wm_ctx(mode, line_pos.width, line_pos.height) {
        m_is_vertical = (mode == litehtml::writing_mode_vertical_rl ||
                         mode == litehtml::writing_mode_vertical_lr);
    }

    /**
     * 論理インラインオフセットからグリフの配置情報を取得する
     * @param inline_offset 行頭からの論理インラインオフセット
     * @param block_offset 行のベースラインからの論理ブロックオフセット (水平ならY,
     * 垂直ならXの微調整)
     * @param is_upright 垂直筆記時に正立（回転なし）させるかどうか
     * @param is_punctuation 句読点（ベースライン調整が必要）かどうか
     * @param font 描画に使用する SkFont (メトリクス取得用)
     */
    GlyphPlacement getGlyphPlacement(float inline_offset, float block_offset, bool is_upright,
                                     bool is_punctuation, const SkFont& font,
                                     SkGlyphID glyph_id) const;

    bool isVertical() const { return m_is_vertical; }

   private:
    litehtml::writing_mode m_mode;
    litehtml::position m_line_pos;
    font_info* m_fi;
    bool m_is_vertical;
    WritingModeContext m_wm_ctx;
};

}  // namespace satoru

#endif  // SATORU_TEXT_GEOMETRY_H
