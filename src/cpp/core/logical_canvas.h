#ifndef SATORU_LOGICAL_CANVAS_H
#define SATORU_LOGICAL_CANVAS_H

#include "core/logical_geometry.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

namespace satoru {

/**
 * 書き込みモード（WritingMode）に基づき、論理座標で描画命令を受け付けるラッパークラス
 */
class LogicalCanvas {
   public:
    LogicalCanvas(SkCanvas* canvas, const WritingModeContext& wm_ctx, float offset_x = 0,
                  float offset_y = 0)
        : m_canvas(canvas), m_wm_ctx(wm_ctx), m_offset_x(offset_x), m_offset_y(offset_y) {}

    /**
     * 論理矩形を描画する
     */
    void drawRect(const logical_pos& pos, const logical_size& size, const SkPaint& paint) {
        auto phys = m_wm_ctx.to_physical(pos, size);
        m_canvas->drawRect(SkRect::MakeXYWH(m_offset_x + phys.x, m_offset_y + phys.y,
                                            (float)phys.width, (float)phys.height),
                           paint);
    }

    /**
     * 論理座標で線を引く
     */
    void drawLine(const logical_pos& start, const logical_pos& end, const SkPaint& paint) {
        auto p_start = m_wm_ctx.to_physical(start, logical_size(0, 0));
        auto p_end = m_wm_ctx.to_physical(end, logical_size(0, 0));
        m_canvas->drawLine(m_offset_x + p_start.x, m_offset_y + p_start.y, m_offset_x + p_end.x,
                           m_offset_y + p_end.y, paint);
    }

    /**
     * 論理矩形でクリップする
     */
    void clipRect(const logical_pos& pos, const logical_size& size, bool antialias = false) {
        auto phys = m_wm_ctx.to_physical(pos, size);
        m_canvas->clipRect(SkRect::MakeXYWH(m_offset_x + phys.x, m_offset_y + phys.y,
                                            (float)phys.width, (float)phys.height),
                           SkClipOp::kIntersect, antialias);
    }

    /**
     * SkCanvas の生インスタンスを取得（特殊な描画用）
     */
    SkCanvas* canvas() { return m_canvas; }

    /**
     * 状態の保存と復元
     */
    void save() { m_canvas->save(); }
    void restore() { m_canvas->restore(); }

    /**
     * 物理オフセットを更新（ネストした要素用）
     */
    void translate(float dx, float dy) {
        m_offset_x += dx;
        m_offset_y += dy;
    }

   private:
    SkCanvas* m_canvas;
    const WritingModeContext& m_wm_ctx;
    float m_offset_x;
    float m_offset_y;
};

}  // namespace satoru

#endif  // SATORU_LOGICAL_CANVAS_H
