#include "text_geometry.h"

#include <cstring>

namespace satoru {

const SkFontMetrics& TextGeometry::metricsFor(const SkFont& font) const {
    auto typeface = font.getTypeface();
    uint32_t typeface_id = typeface ? typeface->uniqueID() : 0;
    float font_size = font.getSize();
    if (!m_has_cached_metrics || m_cached_typeface_id != typeface_id ||
        m_cached_font_size != font_size) {
        font.getMetrics(&m_cached_metrics);
        m_cached_typeface_id = typeface_id;
        m_cached_font_size = font_size;
        m_has_cached_metrics = true;
    }
    return m_cached_metrics;
}

float TextGeometry::glyphWidthFor(const SkFont& font, SkGlyphID glyph_id) const {
    auto typeface = font.getTypeface();
    uint32_t typeface_id = typeface ? typeface->uniqueID() : 0;
    uint32_t font_size_bits = 0;
    static_assert(sizeof(font_size_bits) == sizeof(float));
    float font_size = font.getSize();
    memcpy(&font_size_bits, &font_size, sizeof(float));
    uint64_t key = ((uint64_t)typeface_id << 32) ^ ((uint64_t)font_size_bits << 16) ^
                   (uint64_t)glyph_id;
    auto cached = m_fi->glyph_width_cache.find(key);
    if (cached != m_fi->glyph_width_cache.end()) return cached->second;
    float width = (float)font.getWidth(glyph_id);
    m_fi->glyph_width_cache.emplace(key, width);
    return width;
}

GlyphPlacement TextGeometry::getGlyphPlacement(float inline_offset, float block_offset,
                                               bool is_upright, bool is_punctuation,
                                               const SkFont& font, SkGlyphID glyph_id) const {
    GlyphPlacement placement;
    placement.rotation = 0;

    if (m_is_vertical) {
        const SkFontMetrics& metrics = metricsFor(font);

        if (is_upright) {
            float font_size = (float)m_fi->desc.size;
            float width = glyphWidthFor(font, glyph_id);
            placement.x = (float)m_line_pos.x + (float)m_line_pos.width / 2.0f - width / 2.0f;

            // Center the character vertically in its font_size-tall slot.
            // (metrics.fAscent + metrics.fDescent) / 2.0f is the center of the glyph relative to
            // the baseline. We want this to be at (inline_offset + font_size / 2.0f).
            float v_shift = font_size / 2.0f - (metrics.fAscent + metrics.fDescent) / 2.0f;

            if (is_punctuation) {
                float h_shift = (font_size - width) / 2.0f + width * 0.58f;
                placement.y =
                    (float)m_line_pos.y + inline_offset + block_offset + v_shift - width * 0.5f;
                placement.x =
                    (float)m_line_pos.x + (float)m_line_pos.width / 2.0f - width / 2.0f + h_shift;
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
