#include "bridge/magic_tags.h"
#include "container_skia.h"
#include "container_skia_helpers.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/effects/SkImageFilters.h"
#include "litehtml/render_item.h"
#include "utils/skia_utils.h"

// ────────────────────────────────────────────────────────────────────────────
// Backdrop filter
// ────────────────────────────────────────────────────────────────────────────

void container_skia::push_backdrop_filter(litehtml::uint_ptr hdc,
                                          const std::shared_ptr<litehtml::render_item>& el) {
    if (!m_canvas || el->src_el()->css().get_backdrop_filter().empty()) return;
    flush();

    if (m_tagging) {
        backdrop_filter_info info;
        info.tokens = el->src_el()->css().get_backdrop_filter();

        litehtml::position el_pos_abs = el->get_placement();
        info.box_pos.x = (int)(el_pos_abs.x - el->padding_left() - el->border_left());
        info.box_pos.y = (int)(el_pos_abs.y - el->padding_top() - el->border_top());
        info.box_pos.width = (int)(el->pos().width + el->padding_left() + el->padding_right() +
                                   el->border_left() + el->border_right());
        info.box_pos.height = (int)(el->pos().height + el->padding_top() + el->padding_bottom() +
                                    el->border_top() + el->border_bottom());

        info.box_radius = el->src_el()->css().get_borders().radius.calc_percents(
            info.box_pos.width, info.box_pos.height);
        info.opacity = get_current_opacity();

        m_usedBackdropFilters.push_back(info);
        int index = (int)m_usedBackdropFilters.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::BackdropFilterPush, index));

        // Draw a tiny rectangle with magic color to signal the push
        m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
        return;
    }

    auto last_filter = build_backdrop_filter_chain(el->src_el()->css().get_backdrop_filter());

    if (last_filter) {
        litehtml::position el_pos_abs = el->get_placement();
        litehtml::position border_box;
        border_box.x = (int)(el_pos_abs.x - el->padding_left() - el->border_left());
        border_box.y = (int)(el_pos_abs.y - el->padding_top() - el->border_top());
        border_box.width = (int)(el->pos().width + el->padding_left() + el->padding_right() +
                                 el->border_left() + el->border_right());
        border_box.height = (int)(el->pos().height + el->padding_top() + el->padding_bottom() +
                                  el->border_top() + el->border_bottom());

        litehtml::border_radiuses bdr_radius =
            el->src_el()->css().get_borders().radius.calc_percents(border_box.width,
                                                                   border_box.height);

        m_canvas->save();
        m_canvas->clipRRect(make_rrect(border_box, bdr_radius), true);

        SkCanvas::SaveLayerRec layer_rec(nullptr, nullptr, last_filter.get(), 0);
        m_canvas->saveLayer(layer_rec);
    } else {
        // Save twice to balance the two restores in pop
        m_canvas->save();
        m_canvas->save();
    }
}

void container_skia::pop_backdrop_filter(litehtml::uint_ptr hdc) {
    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::BackdropFilterPop));
            m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
            return;
        }
        m_canvas->restore();  // for saveLayer
        m_canvas->restore();  // for clip's save
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Filter (CSS filter property)
// ────────────────────────────────────────────────────────────────────────────

void container_skia::push_filter(litehtml::uint_ptr hdc, const litehtml::css_token_vector& filter) {
    if (!m_canvas || filter.empty()) return;
    flush();

    if (m_tagging) {
        filter_info info;
        info.tokens = filter;
        info.opacity = get_current_opacity();
        m_usedFilters.push_back(info);
        int index = (int)m_usedFilters.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::FilterPush, index));

        SkRect rect;
        if (!m_clips.empty()) {
            rect = SkRect::MakeXYWH((float)m_clips.back().first.x, (float)m_clips.back().first.y,
                                    (float)m_clips.back().first.width,
                                    (float)m_clips.back().first.height);
        } else {
            rect = SkRect::MakeWH((float)m_width, (float)m_height);
        }

        m_canvas->drawRect(rect, p);
        m_filter_stack_depth++;
        return;
    }

    auto last_filter = build_filter_chain(filter);

    if (last_filter) {
        SkPaint paint;
        paint.setImageFilter(last_filter);
        m_canvas->saveLayer(nullptr, &paint);
        m_filter_stack_depth++;
    } else {
        m_canvas->save();
        m_filter_stack_depth++;
    }
}

void container_skia::pop_filter(litehtml::uint_ptr hdc) {
    if (m_filter_stack_depth <= 0) return;
    m_filter_stack_depth--;

    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::FilterPop));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        } else {
            m_canvas->restore();
        }
    }
}
