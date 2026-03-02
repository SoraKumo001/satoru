#include "tagging_context.h"

#include <cmath>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"

namespace satoru {

void TaggingContext::drawGlyph(const SkFont& font, SkGlyphID glyphId, float phys_x, float phys_y,
                               float rotation, const SkPaint& basePaint) {
    auto pathOpt = font.getPath(glyphId);

    if (pathOpt.has_value() && !pathOpt.value().isEmpty()) {
        const SkPath& path = pathOpt.value();
        int glyphIdx = getOrAddGlyphPath(path);

        glyph_draw_info drawInfo;
        drawInfo.glyph_index = glyphIdx;
        drawInfo.style_tag = m_styleTag;
        drawInfo.style_index = m_styleIndex;

        m_usedGlyphDraws.push_back(drawInfo);
        int drawIdx = (int)m_usedGlyphDraws.size();

        SkPaint glyphPaint = basePaint;
        glyphPaint.setColor(make_magic_color(MagicTag::GlyphPath, drawIdx));

        m_canvas->save();
        m_canvas->translate(phys_x, phys_y);
        if (rotation != 0) m_canvas->rotate(rotation);
        m_canvas->drawPath(path, glyphPaint);
        m_canvas->restore();
    } else {
        // Raster glyph fallback (e.g. for color emoji or bitmaps)
        SkRect bounds = font.getBounds(glyphId, &basePaint);

        int w = (int)ceilf(bounds.width());
        int h = (int)ceilf(bounds.height());
        if (w > 0 && h > 0) {
            SkImageInfo info = SkImageInfo::MakeN32Premul(w, h, SkColorSpace::MakeSRGB());
            auto surface = SkSurfaces::Raster(info);
            if (surface) {
                auto tmpCanvas = surface->getCanvas();
                tmpCanvas->clear(SK_ColorTRANSPARENT);
                tmpCanvas->drawSimpleText(&glyphId, sizeof(uint16_t), SkTextEncoding::kGlyphID,
                                          -bounds.fLeft, -bounds.fTop, font, basePaint);
                auto img = surface->makeImageSnapshot();

                m_canvas->save();
                m_canvas->translate(phys_x, phys_y);
                if (rotation != 0) m_canvas->rotate(rotation);
                SkRect dst = SkRect::MakeXYWH(bounds.fLeft, bounds.fTop, (float)w, (float)h);
                m_canvas->drawImageRect(img, dst, SkSamplingOptions(SkFilterMode::kLinear));
                m_canvas->restore();
            }
        }
    }
}

int TaggingContext::getOrAddGlyphPath(const SkPath& path) {
    for (size_t gi = 0; gi < m_usedGlyphs.size(); ++gi) {
        if (m_usedGlyphs[gi] == path) {
            return (int)gi + 1;
        }
    }
    m_usedGlyphs.push_back(path);
    return (int)m_usedGlyphs.size();
}

}  // namespace satoru
