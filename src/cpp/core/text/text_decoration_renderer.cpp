#include "text_decoration_renderer.h"

#include <algorithm>
#include <cmath>

#include "core/logical_canvas.h"
#include "core/logical_geometry.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/effects/SkDashPathEffect.h"

namespace satoru {

void TextDecorationRenderer::drawDecoration(SkCanvas* canvas, font_info* fi,
                                            const litehtml::position& pos,
                                            const litehtml::web_color& color, double finalWidth,
                                            litehtml::writing_mode mode) {
    float inline_size = (float)finalWidth;
    float thickness = (float)fi->desc.decoration_thickness.val();
    if (thickness == 0) thickness = 1.0f;

    litehtml::web_color dec_color = fi->desc.decoration_color;
    if (dec_color == litehtml::web_color::current_color) dec_color = color;

    SkPaint dec_paint;
    dec_paint.setColor(
        SkColorSetARGB(dec_color.alpha, dec_color.red, dec_color.green, dec_color.blue));
    dec_paint.setAntiAlias(true);
    dec_paint.setStrokeWidth(thickness);
    dec_paint.setStyle(SkPaint::kStroke_Style);

    WritingModeContext wm_ctx(mode, pos.width, pos.height);
    LogicalCanvas l_canvas(canvas, wm_ctx, (float)pos.x, (float)pos.y);

    auto draw_logical_line = [&](float block_offset) {
        logical_pos logical_start(0, block_offset);
        logical_pos logical_end(inline_size, block_offset);

        if (fi->desc.decoration_style == litehtml::text_decoration_style_dotted) {
            float intervals[] = {thickness, thickness};
            dec_paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
        } else if (fi->desc.decoration_style == litehtml::text_decoration_style_dashed) {
            float intervals[] = {thickness * 3, thickness * 3};
            dec_paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const float>(intervals, 2), 0));
        }

        if (fi->desc.decoration_style == litehtml::text_decoration_style_double) {
            float gap = thickness + 1.0f;
            l_canvas.drawLine(logical_pos(0, block_offset - gap / 2),
                              logical_pos(inline_size, block_offset - gap / 2), dec_paint);
            l_canvas.drawLine(logical_pos(0, block_offset + gap / 2),
                              logical_pos(inline_size, block_offset + gap / 2), dec_paint);
        } else if (fi->desc.decoration_style == litehtml::text_decoration_style_wavy) {
            dec_paint.setStrokeWidth(thickness * 1.5f);

            float wave_wavelength = std::max(8.0f, thickness * 8.0f);
            float wave_amplitude = std::max(2.0f, thickness * 2.0f);
            float block_offset_wavy = block_offset + wave_amplitude;

            bool is_vertical = wm_ctx.is_vertical();
            float phase_ref = is_vertical ? (float)pos.y : (float)pos.x;
            float start_i = -fmodf(phase_ref, wave_wavelength);

            SkPathBuilder wavy_builder;
            bool first = true;

            for (float i = start_i; i < inline_size; i += wave_wavelength) {
                {
                    logical_pos lp1(i, block_offset_wavy);
                    logical_pos lp2(i + wave_wavelength / 4.0f, block_offset_wavy + wave_amplitude);
                    logical_pos lp3(i + wave_wavelength / 2.0f, block_offset_wavy);

                    litehtml::position p1 = wm_ctx.to_physical(lp1, logical_size(0, 0));
                    litehtml::position p2 = wm_ctx.to_physical(lp2, logical_size(0, 0));
                    litehtml::position p3 = wm_ctx.to_physical(lp3, logical_size(0, 0));

                    if (first) {
                        wavy_builder.moveTo((float)pos.x + p1.x, (float)pos.y + p1.y);
                        first = false;
                    }
                    wavy_builder.quadTo((float)pos.x + p2.x, (float)pos.y + p2.y,
                                        (float)pos.x + p3.x, (float)pos.y + p3.y);
                }
                {
                    logical_pos lp2(i + 3.0f * wave_wavelength / 4.0f,
                                    block_offset_wavy - wave_amplitude);
                    logical_pos lp3(i + wave_wavelength, block_offset_wavy);

                    litehtml::position p2 = wm_ctx.to_physical(lp2, logical_size(0, 0));
                    litehtml::position p3 = wm_ctx.to_physical(lp3, logical_size(0, 0));

                    wavy_builder.quadTo((float)pos.x + p2.x, (float)pos.y + p2.y,
                                        (float)pos.x + p3.x, (float)pos.y + p3.y);
                }
            }

            l_canvas.save();
            l_canvas.clipRect(logical_pos(0, block_offset_wavy - wave_amplitude * 2),
                              logical_size(inline_size, wave_amplitude * 4));
            canvas->drawPath(wavy_builder.detach(), dec_paint);
            l_canvas.restore();
        } else {
            l_canvas.drawLine(logical_start, logical_end, dec_paint);
        }
    };

    if (fi->desc.decoration_line & litehtml::text_decoration_line_underline) {
        float underline_offset =
            (float)fi->fm_ascent + (float)fi->desc.underline_offset.val() + thickness;
        draw_logical_line(underline_offset);
    }
    if (fi->desc.decoration_line & litehtml::text_decoration_line_overline) {
        draw_logical_line(0);
    }
    if (fi->desc.decoration_line & litehtml::text_decoration_line_line_through) {
        draw_logical_line((float)fi->fm_ascent * 0.65f);
    }
}

}  // namespace satoru
