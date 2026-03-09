#include "text_geometry.h"

#include "include/core/SkFontMetrics.h"

namespace satoru {

GlyphPlacement TextGeometry::getGlyphPlacement(float inline_offset, float block_offset,
                                               bool is_upright, bool is_punctuation,
                                               const SkFont& font, SkGlyphID glyph_id) const {
    GlyphPlacement placement;
    placement.rotation = 0;

    if (m_is_vertical) {
        SkFontMetrics metrics;
        font.getMetrics(&metrics);

        if (is_upright) {
            float font_size = (float)m_fi->desc.size;
            float width = (float)font.getWidth(glyph_id);
            placement.x = (float)m_line_pos.x + (float)m_line_pos.width / 2.0f - width / 2.0f;

            // Center the character vertically in its font_size-tall slot.
            // (metrics.fAscent + metrics.fDescent) / 2.0f is the center of the glyph relative to the baseline.
            // We want this to be at (inline_offset + font_size / 2.0f).
            float v_shift = font_size / 2.0f - (metrics.fAscent + metrics.fDescent) / 2.0f;

            if (is_punctuation) {
                float h_shift = (font_size - width) / 2.0f + width * 0.58f;
                placement.y = (float)m_line_pos.y + inline_offset + block_offset + v_shift -
                              width * 0.5f;
                placement.x = (float)m_line_pos.x + (float)m_line_pos.width / 2.0f - width / 2.0f + h_shift;
            } else {
                placement.y = (float)m_line_pos.y + inline_offset + block_offset + v_shift;
            }
        } else {
            placement.rotation = 90.0f;
            float em_center = (metrics.fAscent + metrics.fDescent) / 2.0f;

            placement.x = (float)m_line_pos.x + (float)m_line_pos.width / 2.0f + em_center;
            placement.y = (float)m_line_pos.y + inline_offset;
        }
    } else {
        placement.x = (float)m_line_pos.x + inline_offset;
        placement.y = (float)m_line_pos.y + block_offset;
    }

    return placement;
}

}  // namespace satoru
