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

    bool is_vertical() const {
        return m_mode == litehtml::writing_mode_vertical_rl ||
               m_mode == litehtml::writing_mode_vertical_lr;
    }

    litehtml::writing_mode mode() const { return m_mode; }

   private:
    litehtml::writing_mode m_mode;
    pixel_t m_container_width;
    pixel_t m_container_height;
};

}  // namespace satoru

#endif  // SATORU_LOGICAL_GEOMETRY_H
