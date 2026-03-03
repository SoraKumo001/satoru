#include "text_geometry.h"

#include "include/core/SkFontMetrics.h"

namespace satoru {

GlyphPlacement TextGeometry::getGlyphPlacement(float inline_offset, float block_offset,
                                               bool is_upright, bool is_punctuation,
                                               const SkFont& font) const {
    GlyphPlacement placement;
    placement.rotation = 0;

    if (m_is_vertical) {
        SkFontMetrics metrics;
        font.getMetrics(&metrics);

        if (is_upright) {
            // 正立 (Upright): グリフの視覚的な中心を行のブロック中心に合わせる
            float em_height = metrics.fDescent - metrics.fAscent;
            float em_center = metrics.fAscent + em_height / 2.0f;

            // 行ボックスのブロック方向中心から、グリフの視覚的中心までのオフセット
            float base_block_offset = (float)m_line_pos.width / 2.0f - em_center;

            // 縦書き時のベースライン (正立文字の場合)
            // 通常文字は視覚的な均衡（英字との上端合わせ）のため CapHeight
            // 分だけベースラインをシフトする。
            // 句読点の場合、縦書き用グリフは元々上寄りに設計されているため、シフトを行うと下がりすぎてしまう。
            float inline_adj = 0;
            if (!is_punctuation) {
                inline_adj =
                    metrics.fCapHeight > 0 ? metrics.fCapHeight : (float)m_fi->desc.size * 0.75f;
            }

            logical_pos l_p(inline_offset + inline_adj, base_block_offset + block_offset);
            litehtml::position p_p = m_wm_ctx.to_physical(l_p, logical_size(0, 0));

            placement.x = (float)m_line_pos.x + p_p.x;
            placement.y = (float)m_line_pos.y + p_p.y;

            if (is_punctuation) {
                // 句読点を右上（RLでは右、LRでは左）にオフセット
                float shift = (float)m_fi->desc.size * 0.58f;
                if (m_mode == litehtml::writing_mode_vertical_rl) {
                    placement.x += shift;
                    placement.y += (float)m_fi->desc.size * 0.2f;
                } else {
                    placement.x -= shift;
                }
            }
        } else {
            // 回転 (Sideways): グリフを時計回りに90度回転
            float base_block_offset = (float)m_line_pos.width / 2.0f - metrics.fAscent * 0.5f;
            logical_pos l_p(inline_offset, base_block_offset + block_offset);

            litehtml::position p_p = m_wm_ctx.to_physical(l_p, logical_size(0, 0));

            placement.x = (float)m_line_pos.x + p_p.x;
            placement.y = (float)m_line_pos.y + p_p.y;
            placement.rotation = 90.0f;
        }
    } else {
        // 水平筆記
        placement.x = (float)m_line_pos.x + inline_offset;
        placement.y = (float)m_line_pos.y + block_offset;
    }

    return placement;
}

}  // namespace satoru
