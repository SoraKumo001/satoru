#ifndef SATORU_LOGICAL_GEOMETRY_H
#define SATORU_LOGICAL_GEOMETRY_H

#include <litehtml.h>

namespace satoru {

using pixel_t = litehtml::pixel_t;

struct logical_size {
    pixel_t inline_size;
    pixel_t block_size;

    logical_size() : inline_size(0), block_size(0) {}
    logical_size(pixel_t is, pixel_t bs) : inline_size(is), block_size(bs) {}
};

struct logical_pos {
    pixel_t inline_offset;
    pixel_t block_offset;

    logical_pos() : inline_offset(0), block_offset(0) {}
    logical_pos(pixel_t io, pixel_t bo) : inline_offset(io), block_offset(bo) {}
};

struct logical_edges {
    pixel_t inline_start;
    pixel_t inline_end;
    pixel_t block_start;
    pixel_t block_end;

    logical_edges() : inline_start(0), inline_end(0), block_start(0), block_end(0) {}
};

class WritingModeContext {
   public:
    WritingModeContext(litehtml::writing_mode mode, pixel_t container_width,
                       pixel_t container_height)
        : m_mode(mode), m_container_width(container_width), m_container_height(container_height) {}

    // 論理座標から物理座標（litehtml::position）への変換
    litehtml::position to_physical(const logical_pos& pos, const logical_size& size) const {
        litehtml::position phys;
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl:
                phys.x = m_container_width - pos.block_offset - size.block_size;
                phys.y = pos.inline_offset;
                phys.width = size.block_size;
                phys.height = size.inline_size;
                break;
            case litehtml::writing_mode_vertical_lr:
                phys.x = pos.block_offset;
                phys.y = pos.inline_offset;
                phys.width = size.block_size;
                phys.height = size.inline_size;
                break;
            case litehtml::writing_mode_horizontal_tb:
            default:
                phys.x = pos.inline_offset;
                phys.y = pos.block_offset;
                phys.width = size.inline_size;
                phys.height = size.block_size;
                break;
        }
        return phys;
    }

    // 物理サイズから論理サイズへの変換
    logical_size to_logical(pixel_t width, pixel_t height) const {
        if (is_vertical()) {
            return {height, width};
        }
        return {width, height};
    }

    logical_pos to_logical_pos(pixel_t x, pixel_t y, pixel_t width, pixel_t height) const {
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl:
                return {y, m_container_width - x - width};
            case litehtml::writing_mode_vertical_lr:
                return {y, x};
            default:
                return {x, y};
        }
    }

    bool is_vertical() const {
        return m_mode == litehtml::writing_mode_vertical_rl ||
               m_mode == litehtml::writing_mode_vertical_lr;
    }

    litehtml::writing_mode mode() const { return m_mode; }

    litehtml::containing_block_context::typed_pixel inline_size(const litehtml::containing_block_context& cb) const {
        return is_vertical() ? cb.height : cb.width;
    }
    litehtml::containing_block_context::typed_pixel block_size(const litehtml::containing_block_context& cb) const {
        return is_vertical() ? cb.width : cb.height;
    }

    const litehtml::css_length& get_inline_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_height() : css.get_width();
    }
    const litehtml::css_length& get_block_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_width() : css.get_height();
    }
    const litehtml::css_length& get_min_inline_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_min_height() : css.get_min_width();
    }
    const litehtml::css_length& get_min_block_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_min_width() : css.get_min_height();
    }
    const litehtml::css_length& get_max_inline_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_max_height() : css.get_max_width();
    }
    const litehtml::css_length& get_max_block_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_max_width() : css.get_max_height();
    }

    litehtml::containing_block_context update_logical(const litehtml::containing_block_context& cb,
                                                      std::optional<litehtml::containing_block_context::typed_pixel> inline_size,
                                                      std::optional<litehtml::containing_block_context::typed_pixel> block_size) const {
        litehtml::containing_block_context ret = cb;
        if (inline_size) {
            if (is_vertical()) {
                ret.height = *inline_size;
                ret.render_height = *inline_size;
            } else {
                ret.width = *inline_size;
                ret.render_width = *inline_size;
            }
        }
        if (block_size) {
            if (is_vertical()) {
                ret.width = *block_size;
                ret.render_width = *block_size;
            } else {
                ret.height = *block_size;
                ret.render_height = *block_size;
            }
        }
        return ret;
    }

    pixel_t inline_start(const litehtml::margins& m) const {
        return is_vertical() ? m.top : m.left;
    }
    pixel_t inline_end(const litehtml::margins& m) const {
        return is_vertical() ? m.bottom : m.right;
    }
    pixel_t block_start(const litehtml::margins& m) const {
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl: return m.right;
            case litehtml::writing_mode_vertical_lr: return m.left;
            default: return m.top;
        }
    }
    pixel_t block_end(const litehtml::margins& m) const {
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl: return m.left;
            case litehtml::writing_mode_vertical_lr: return m.right;
            default: return m.bottom;
        }
    }

    const litehtml::css_length& inline_start(const litehtml::css_margins& m) const {
        return is_vertical() ? m.top : m.left;
    }
    const litehtml::css_length& inline_end(const litehtml::css_margins& m) const {
        return is_vertical() ? m.bottom : m.right;
    }
    const litehtml::css_length& block_start(const litehtml::css_margins& m) const {
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl: return m.right;
            case litehtml::writing_mode_vertical_lr: return m.left;
            default: return m.top;
        }
    }
    const litehtml::css_length& block_end(const litehtml::css_margins& m) const {
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl: return m.left;
            case litehtml::writing_mode_vertical_lr: return m.right;
            default: return m.bottom;
        }
    }

    void set_inline_start(litehtml::margins& m, pixel_t val) const {
        if (is_vertical()) m.top = val; else m.left = val;
    }
    void set_inline_end(litehtml::margins& m, pixel_t val) const {
        if (is_vertical()) m.bottom = val; else m.right = val;
    }
    void set_block_start(litehtml::margins& m, pixel_t val) const {
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl: m.right = val; break;
            case litehtml::writing_mode_vertical_lr: m.left = val; break;
            default: m.top = val; break;
        }
    }
    void set_block_end(litehtml::margins& m, pixel_t val) const {
        switch (m_mode) {
            case litehtml::writing_mode_vertical_rl: m.left = val; break;
            case litehtml::writing_mode_vertical_lr: m.right = val; break;
            default: m.bottom = val; break;
        }
    }

   private:
    litehtml::writing_mode m_mode;
    pixel_t m_container_width;
    pixel_t m_container_height;
};

}  // namespace satoru

#endif  // SATORU_LOGICAL_GEOMETRY_H
