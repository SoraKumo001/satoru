#include "core/logical_geometry.h"
#include "flex_item.h"
#include "render_item.h"

namespace litehtml {

// --- render_item extensions ---

satoru::WritingModeContext render_item::get_wm_context() const {
    return satoru::WritingModeContext(css().get_writing_mode(), m_cached_cb_context.width,
                                      m_cached_cb_context.height);
}

void render_item::place_logical(pixel_t inline_pos, pixel_t block_pos,
                                const containing_block_context& cb_context,
                                formatting_context* fmt_ctx) {
    satoru::WritingModeContext wm(cb_context.mode, cb_context.width, cb_context.height);
    position phys_pos = wm.to_physical(satoru::logical_pos(inline_pos, block_pos),
                                       wm.to_logical(width(), height()));
    place(phys_pos.x, phys_pos.y, cb_context, fmt_ctx);
}

pixel_t render_item::inline_size() const {
    auto wm = get_wm_context();
    return wm
        .to_logical(m_pos.width + m_margins.width() + m_borders.width() + m_padding.width(),
                    m_pos.height + m_margins.height() + m_borders.height() + m_padding.height())
        .inline_size;
}

pixel_t render_item::block_size() const {
    auto wm = get_wm_context();
    return wm
        .to_logical(m_pos.width + m_margins.width() + m_borders.width() + m_padding.width(),
                    m_pos.height + m_margins.height() + m_borders.height() + m_padding.height())
        .block_size;
}

pixel_t render_item::inline_size(const satoru::WritingModeContext& wm) const {
    return wm
        .to_logical(m_pos.width + m_margins.width() + m_borders.width() + m_padding.width(),
                    m_pos.height + m_margins.height() + m_borders.height() + m_padding.height())
        .inline_size;
}

pixel_t render_item::block_size(const satoru::WritingModeContext& wm) const {
    return wm
        .to_logical(m_pos.width + m_margins.width() + m_borders.width() + m_padding.width(),
                    m_pos.height + m_margins.height() + m_borders.height() + m_padding.height())
        .block_size;
}

void render_item::block_shift(pixel_t delta) {
    if (get_wm_context().is_vertical()) {
        if (css().get_writing_mode() == writing_mode_vertical_rl)
            m_pos.x -= delta;
        else
            m_pos.x += delta;
    } else {
        m_pos.y += delta;
    }
}

void render_item::inline_shift(pixel_t delta) {
    if (get_wm_context().is_vertical()) {
        m_pos.y += delta;
    } else {
        m_pos.x += delta;
    }
}

pixel_t render_item::inline_start_pos(const satoru::WritingModeContext& wm) const {
    return wm.is_vertical() ? m_pos.y : m_pos.x;
}

pixel_t render_item::inline_end_pos(const satoru::WritingModeContext& wm) const {
    return inline_start_pos(wm) + (wm.is_vertical() ? m_pos.height : m_pos.width);
}

pixel_t render_item::block_start_pos(const satoru::WritingModeContext& wm) const {
    if (wm.mode() == writing_mode_vertical_rl) return wm.container_width() - m_pos.x - m_pos.width;
    return wm.is_vertical() ? m_pos.x : m_pos.y;
}

pixel_t render_item::block_end_pos(const satoru::WritingModeContext& wm) const {
    return block_start_pos(wm) + (wm.is_vertical() ? m_pos.width : m_pos.height);
}

pixel_t render_item::inline_start_pos() const { return inline_start_pos(get_wm_context()); }
pixel_t render_item::inline_end_pos() const { return inline_end_pos(get_wm_context()); }
pixel_t render_item::block_start_pos() const { return block_start_pos(get_wm_context()); }
pixel_t render_item::block_end_pos() const { return block_end_pos(get_wm_context()); }

pixel_t render_item::margin_inline_start() const {
    return get_wm_context().inline_start(m_margins);
}
pixel_t render_item::margin_inline_end() const { return get_wm_context().inline_end(m_margins); }
pixel_t render_item::margin_block_start() const { return get_wm_context().block_start(m_margins); }
pixel_t render_item::margin_block_end() const { return get_wm_context().block_end(m_margins); }

void render_item::margin_inline_start(pixel_t val) {
    get_wm_context().set_inline_start(m_margins, val);
}
void render_item::margin_inline_end(pixel_t val) {
    get_wm_context().set_inline_end(m_margins, val);
}
void render_item::margin_block_start(pixel_t val) {
    get_wm_context().set_block_start(m_margins, val);
}
void render_item::margin_block_end(pixel_t val) { get_wm_context().set_block_end(m_margins, val); }

pixel_t render_item::padding_inline_start() const {
    return get_wm_context().inline_start(m_padding);
}
pixel_t render_item::padding_inline_end() const { return get_wm_context().inline_end(m_padding); }
pixel_t render_item::padding_block_start() const { return get_wm_context().block_start(m_padding); }
pixel_t render_item::padding_block_end() const { return get_wm_context().block_end(m_padding); }

void render_item::padding_inline_start(pixel_t val) {
    get_wm_context().set_inline_start(m_padding, val);
}
void render_item::padding_inline_end(pixel_t val) {
    get_wm_context().set_inline_end(m_padding, val);
}
void render_item::padding_block_start(pixel_t val) {
    get_wm_context().set_block_start(m_padding, val);
}
void render_item::padding_block_end(pixel_t val) { get_wm_context().set_block_end(m_padding, val); }

pixel_t render_item::border_inline_start() const {
    return get_wm_context().inline_start(m_borders);
}
pixel_t render_item::border_inline_end() const { return get_wm_context().inline_end(m_borders); }
pixel_t render_item::border_block_start() const { return get_wm_context().block_start(m_borders); }
pixel_t render_item::border_block_end() const { return get_wm_context().block_end(m_borders); }

void render_item::border_inline_start(pixel_t val) {
    get_wm_context().set_inline_start(m_borders, val);
}
void render_item::border_inline_end(pixel_t val) {
    get_wm_context().set_inline_end(m_borders, val);
}
void render_item::border_block_start(pixel_t val) {
    get_wm_context().set_block_start(m_borders, val);
}
void render_item::border_block_end(pixel_t val) { get_wm_context().set_block_end(m_borders, val); }

// --- flex_item extensions ---

void flex_item_row_direction::finalize_position(pixel_t container_width, pixel_t container_height) {
    m_container_wm.update_container_size(container_width, container_height);
    satoru::logical_pos pos(main_pos, cross_pos);
    satoru::logical_size size(get_el_main_size(), get_el_cross_size());
    litehtml::position phys_pos = m_container_wm.to_physical(pos, size);
    el->pos().x = phys_pos.x + el->content_offset_left();
    el->pos().y = phys_pos.y + el->content_offset_top();
}

void flex_item_column_direction::finalize_position(pixel_t container_width,
                                                   pixel_t container_height) {
    m_container_wm.update_container_size(container_width, container_height);
    satoru::logical_pos pos(cross_pos, main_pos);
    satoru::logical_size size(get_el_cross_size(), get_el_main_size());
    litehtml::position phys_pos = m_container_wm.to_physical(pos, size);
    el->pos().x = phys_pos.x + el->content_offset_left();
    el->pos().y = phys_pos.y + el->content_offset_top();
}

}  // namespace litehtml
