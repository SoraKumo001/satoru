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
            float font_size = (float)m_fi->desc.size;
            placement.x = (float)m_line_pos.x + (float)m_line_pos.width / 2.0f - font_size / 2.0f;

            if (is_punctuation) {
                float shift = font_size * 0.58f;
                placement.y = (float)m_line_pos.y + inline_offset + block_offset - font_size * 0.5f;
                placement.x += shift;
            } else {
                placement.y = (float)m_line_pos.y + inline_offset + block_offset;
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
