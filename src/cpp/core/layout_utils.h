#ifndef SATORU_LAYOUT_UTILS_H
#define SATORU_LAYOUT_UTILS_H

#include "core/logical_geometry.h"
#include "libs/litehtml/include/litehtml.h"

namespace satoru {

/**
 * litehtml::render_item に対する論理座標操作のヘルパー
 */
class LayoutUtils {
   public:
    static pixel_t get_inline_size(const litehtml::render_item* item,
                                   const WritingModeContext& wm) {
        return wm.to_logical(item->width(), item->height()).inline_size;
    }

    static pixel_t get_block_size(const litehtml::render_item* item, const WritingModeContext& wm) {
        return wm.to_logical(item->width(), item->height()).block_size;
    }

    static void place_logical(litehtml::render_item* item, pixel_t inline_pos, pixel_t block_pos,
                              const litehtml::containing_block_context& cb_context,
                              litehtml::formatting_context* fmt_ctx) {
        WritingModeContext wm(cb_context.mode, cb_context.width, cb_context.height);
        auto phys_pos = wm.to_physical(logical_pos(inline_pos, block_pos),
                                       wm.to_logical(item->width(), item->height()));
        item->place(phys_pos.x, phys_pos.y, cb_context, fmt_ctx);
    }

    // 他のボックスモデルプロパティ（margin, border, padding）の論理取得もここに追加可能
};

}  // namespace satoru

#endif  // SATORU_LAYOUT_UTILS_H
