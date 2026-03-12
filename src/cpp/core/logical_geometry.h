#ifndef SATORU_LOGICAL_GEOMETRY_H
#define SATORU_LOGICAL_GEOMETRY_H

#include <litehtml.h>

#include <optional>

namespace satoru {

using pixel_t = litehtml::pixel_t;

struct logical_size {
    pixel_t inline_size;
    pixel_t block_size;

    logical_size() : inline_size(0), block_size(0) {}
    logical_size(pixel_t is, pixel_t bs) : inline_size(is), block_size(bs) {}

    bool operator==(const logical_size& other) const {
        return inline_size == other.inline_size && block_size == other.block_size;
    }
};

struct logical_pos {
    pixel_t inline_offset;
    pixel_t block_offset;

    logical_pos() : inline_offset(0), block_offset(0) {}
    logical_pos(pixel_t io, pixel_t bo) : inline_offset(io), block_offset(bo) {}

    bool operator==(const logical_pos& other) const {
        return inline_offset == other.inline_offset && block_offset == other.block_offset;
    }

    logical_pos operator+(const logical_pos& other) const {
        return {inline_offset + other.inline_offset, block_offset + other.block_offset};
    }
};

class WritingModeContext {
   public:
    WritingModeContext(litehtml::writing_mode mode, pixel_t container_width,
                       pixel_t container_height)
        : m_mode(mode), m_container_width(container_width), m_container_height(container_height) {}

    bool is_vertical() const {
        return m_mode == litehtml::writing_mode_vertical_rl ||
               m_mode == litehtml::writing_mode_vertical_lr;
    }

    litehtml::writing_mode mode() const { return m_mode; }
    pixel_t container_width() const { return m_container_width; }
    pixel_t container_height() const { return m_container_height; }

    void update_container_size(pixel_t width, pixel_t height) {
        m_container_width = width;
        m_container_height = height;
    }

    // Coordinate conversions
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
            default:
                phys.x = pos.inline_offset;
                phys.y = pos.block_offset;
                phys.width = size.inline_size;
                phys.height = size.block_size;
                break;
        }
        return phys;
    }

    logical_size to_logical(pixel_t width, pixel_t height) const {
        return is_vertical() ? logical_size(height, width) : logical_size(width, height);
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

    // Logical property helpers
    pixel_t inline_start(const litehtml::margins& m) const {
        return is_vertical() ? m.top : m.left;
    }
    pixel_t inline_end(const litehtml::margins& m) const {
        return is_vertical() ? m.bottom : m.right;
    }
    pixel_t block_start(const litehtml::margins& m) const {
        if (m_mode == litehtml::writing_mode_vertical_rl) return m.right;
        return is_vertical() ? m.left : m.top;
    }
    pixel_t block_end(const litehtml::margins& m) const {
        if (m_mode == litehtml::writing_mode_vertical_rl) return m.left;
        return is_vertical() ? m.right : m.bottom;
    }

    const litehtml::css_length& inline_start(const litehtml::css_margins& m) const {
        return is_vertical() ? m.top : m.left;
    }
    const litehtml::css_length& inline_end(const litehtml::css_margins& m) const {
        return is_vertical() ? m.bottom : m.right;
    }
    const litehtml::css_length& block_start(const litehtml::css_margins& m) const {
        if (m_mode == litehtml::writing_mode_vertical_rl) return m.right;
        return is_vertical() ? m.left : m.top;
    }
    const litehtml::css_length& block_end(const litehtml::css_margins& m) const {
        if (m_mode == litehtml::writing_mode_vertical_rl) return m.left;
        return is_vertical() ? m.right : m.bottom;
    }

    void set_inline_start(litehtml::margins& m, pixel_t val) const {
        if (is_vertical())
            m.top = val;
        else
            m.left = val;
    }
    void set_inline_end(litehtml::margins& m, pixel_t val) const {
        if (is_vertical())
            m.bottom = val;
        else
            m.right = val;
    }
    void set_block_start(litehtml::margins& m, pixel_t val) const {
        if (m_mode == litehtml::writing_mode_vertical_rl)
            m.right = val;
        else if (is_vertical())
            m.left = val;
        else
            m.top = val;
    }
    void set_block_end(litehtml::margins& m, pixel_t val) const {
        if (m_mode == litehtml::writing_mode_vertical_rl)
            m.left = val;
        else if (is_vertical())
            m.right = val;
        else
            m.bottom = val;
    }

    // CSS length helpers
    const litehtml::css_length& get_inline_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_height() : css.get_width();
    }
    const litehtml::css_length& get_block_size(const litehtml::css_properties& css) const {
        return is_vertical() ? css.get_width() : css.get_height();
    }

    litehtml::containing_block_context update_logical(
        const litehtml::containing_block_context& cb,
        std::optional<litehtml::containing_block_context::typed_pixel> isize,
        std::optional<litehtml::containing_block_context::typed_pixel> bsize) const {
        litehtml::containing_block_context ret = cb;
        ret.mode = m_mode;
        if (isize) {
            if (is_vertical()) {
                ret.height = *isize;
                ret.render_height = *isize;
            } else {
                ret.width = *isize;
                ret.render_width = *isize;
            }
        }
        if (bsize) {
            if (is_vertical()) {
                ret.width = *bsize;
                ret.render_width = *bsize;
            } else {
                ret.height = *bsize;
                ret.render_height = *bsize;
            }
        }
        return ret;
    }

   private:
    litehtml::writing_mode m_mode;
    pixel_t m_container_width;
    pixel_t m_container_height;
};

struct logical_rect {
    pixel_t inline_offset;
    pixel_t block_offset;
    pixel_t inline_size;
    pixel_t block_size;

    logical_rect() : inline_offset(0), block_offset(0), inline_size(0), block_size(0) {}
    logical_rect(pixel_t io, pixel_t bo, pixel_t is, pixel_t bs)
        : inline_offset(io), block_offset(bo), inline_size(is), block_size(bs) {}

    logical_pos pos() const { return {inline_offset, block_offset}; }
    logical_size size() const { return {inline_size, block_size}; }

    pixel_t inline_start() const { return inline_offset; }
    pixel_t inline_end() const { return inline_offset + inline_size; }
    pixel_t block_start() const { return block_offset; }
    pixel_t block_end() const { return block_offset + block_size; }

    litehtml::position to_physical(const WritingModeContext& wm) const {
        return wm.to_physical(pos(), size());
    }
};

}  // namespace satoru

#endif  // SATORU_LOGICAL_GEOMETRY_H
