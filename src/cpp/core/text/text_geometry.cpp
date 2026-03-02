#include "text_geometry.h"

namespace satoru {

GlyphPlacement TextGeometry::getGlyphPlacement(float inline_offset, float block_offset,
                                               bool is_upright, bool is_punctuation) const {
    GlyphPlacement placement;
    placement.rotation = 0;

    if (m_is_vertical) {
        if (is_upright) {
            // 正立 (Upright): グリフの水平中心を行のブロック中心に合わせる
            // base_block_offset
            // は論理的なブロック開始位置（RLなら右端、LRなら左端）からのオフセット
            float base_block_offset = (float)m_line_pos.width / 2.0f - m_fi->desc.size / 2.0f;
            float baseline_adj = is_punctuation ? m_fi->desc.size * 0.30f : m_fi->desc.size * 0.92f;

            // block_offset はベースラインからの微調整（水平筆記時の Y に相当するもの）
            logical_pos l_p(inline_offset + baseline_adj, base_block_offset + block_offset);

            // 座標計算は block_offset 内で完結しているため、size は 0 を渡す
            // これにより to_physical 内での size.block_size の重複減算を防ぐ
            litehtml::position p_p = m_wm_ctx.to_physical(l_p, logical_size(0, 0));

            placement.x = (float)m_line_pos.x + p_p.x;
            placement.y = (float)m_line_pos.y + p_p.y;

            if (is_punctuation) {
                // 句読点を右上にオフセット (RL/LR共通)
                placement.x += (float)m_fi->desc.size * 0.55f;
            }
        } else {
            // 回転 (Sideways): グリフを時計回りに90度回転
            float base_block_offset = (float)m_line_pos.width / 2.0f - m_fi->desc.size * 0.40f;
            logical_pos l_p(inline_offset, base_block_offset + block_offset);

            litehtml::position p_p = m_wm_ctx.to_physical(l_p, logical_size(0, 0));

            placement.x = (float)m_line_pos.x + p_p.x;
            placement.y = (float)m_line_pos.y + p_p.y;
            placement.rotation = 90.0f;
        }
    } else {
        // 水平筆記: inline_offset は x, block_offset は y (ベースライン)
        placement.x = (float)m_line_pos.x + inline_offset;
        placement.y = (float)m_line_pos.y + block_offset;
    }

    return placement;
}

}  // namespace satoru
