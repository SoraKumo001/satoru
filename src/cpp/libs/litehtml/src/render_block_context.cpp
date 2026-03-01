#include "render_block_context.h"
#include "document.h"
#include "types.h"

litehtml::pixel_t litehtml::render_item_block_context::_render_content(pixel_t /*x*/, pixel_t /*y*/, bool second_pass, const containing_block_context &self_size, formatting_context* fmt_ctx)
{
    element_position el_position;

	pixel_t max_inline_size = 0;
    pixel_t block_offset = 0;
    pixel_t last_block_margin = 0;
	std::shared_ptr<render_item> last_margin_el;
    bool is_first = true;
    satoru::WritingModeContext wm = get_wm_context();
    for (const auto& el : m_children)
    {
        // we don't need to process absolute and fixed positioned element on the second pass
        if (second_pass)
        {
            el_position = el->src_el()->css().get_position();
            if ((el_position == element_position_absolute || el_position == element_position_fixed)) continue;
        }

        if(el->src_el()->css().get_float() != float_none)
        {
            pixel_t rw = place_float(el, block_offset, self_size, fmt_ctx);
            if (rw > max_inline_size)
            {
                max_inline_size = rw;
            }
        } else if(el->src_el()->css().get_display() != display_none)
        {
            if(el->src_el()->css().get_position() == element_position_absolute || el->src_el()->css().get_position() == element_position_fixed)
            {
				pixel_t min_rendered_width = el->measure(self_size, fmt_ctx);
				if(min_rendered_width < el->width() && el->src_el()->css().get_width().is_predefined())
				{
					min_rendered_width = el->measure(self_size.new_inline_size(min_rendered_width), fmt_ctx);
				}
				if (!(self_size.size_mode & containing_block_context::size_mode_measure))
				{
					el->place_logical(0, block_offset, self_size.new_inline_size(min_rendered_width), fmt_ctx);
				}
            } else
            {
                block_offset = fmt_ctx->get_cleared_top(el, block_offset);
                pixel_t inline_offset  = 0;
				pixel_t inline_available_size = self_size.render_inline_size();
				pixel_t line_right	= self_size.render_inline_size();
				pixel_t block_start_margin = margin_block_start();

                el->calc_outlines(self_size.inline_size());

				// Adjust child width for tables and replaced elements with floting blocks width
				if(el->src_el()->is_replaced() ||
				   el->src_el()->is_block_formatting_context() ||
				   el->src_el()->css().get_display() == display_table)
                {
                    pixel_t line_left = 0;
					fmt_ctx->get_line_left_right(block_offset, inline_available_size, line_left, line_right);
					if(line_left != inline_offset)
					{
						inline_offset = line_left - el->margin_inline_start();
					}
					if(line_right != self_size.render_inline_size())
                    {
                        line_right += el->margin_inline_end();
                    }
					if(el->css().get_width().is_predefined())
					{
						inline_available_size = line_right - line_left;
                    }
                }

				// Collapse top margin
				if(is_first && collapse_top_margin())
				{
					if(wm.block_start(el->get_margins()) > 0)
					{
						block_offset -= wm.block_start(el->get_margins());
						if (wm.block_start(el->get_margins()) > block_start_margin)
						{
							block_start_margin = wm.block_start(el->get_margins());
						}
					}
				} else
				{
					if(wm.block_start(el->get_margins()) > 0)
					{
						if (last_block_margin > wm.block_start(el->get_margins()))
						{
							block_offset -= wm.block_start(el->get_margins());
						} else
						{
							block_offset -= last_block_margin;
						}
					}
				}

				pixel_t rw = el->measure(self_size.new_inline_size(inline_available_size), fmt_ctx);
				if (!(self_size.size_mode & containing_block_context::size_mode_measure))
				{
					el->place_logical(inline_offset, block_offset, self_size.new_inline_size(inline_available_size), fmt_ctx);
				}

				// Render table with "width: auto" into returned width
				if(el->src_el()->css().get_display() == display_table && rw < inline_available_size && el->src_el()->css().get_width().is_predefined())
				{
					rw = el->measure(self_size.new_inline_size(rw), fmt_ctx);
					if (!(self_size.size_mode & containing_block_context::size_mode_measure))
					{
						el->place_logical(inline_offset, block_offset, self_size.new_inline_size(rw), fmt_ctx);
					}
				}

				// Check if we need to move block to the next line
				if(el->src_el()->is_replaced() ||
				   el->src_el()->is_block_formatting_context() ||
				   el->src_el()->css().get_display() == display_table)
				{
					if(el->inline_size() > line_right)
					{
                        pixel_t ln_left  = 0;
                        pixel_t ln_right = el->inline_size();
						pixel_t new_block_offset = fmt_ctx->find_next_line_top(block_offset, el->inline_size(), ln_right);
						// If block was moved to the next line, recalculate its position
						if(new_block_offset != block_offset)
						{
							block_offset = new_block_offset;
							fmt_ctx->get_line_left_right(block_offset, el->inline_size(), ln_left, ln_right);
							if (!(self_size.size_mode & containing_block_context::size_mode_measure))
							{
								el->place_logical(ln_left, block_offset, self_size.new_inline_size(inline_available_size), fmt_ctx);
							}
							block_offset -= wm.block_start(el->get_margins());
							// Rollback top margin collapse
							if(is_first && collapse_top_margin())
							{
								block_start_margin = margin_block_start();
							}
						}
                    }
                }

				if (!(self_size.size_mode & containing_block_context::size_mode_measure))
				{
					pixel_t auto_margin = el->calc_auto_margins(inline_available_size);
					if(auto_margin != 0)
					{
						el->inline_shift(auto_margin);
					}
				}
                if (rw > max_inline_size)
                {
                    max_inline_size = rw;
				}
				margin_block_start(block_start_margin);
                block_offset += el->block_size(wm);
                last_block_margin = wm.block_end(el->get_margins());
				last_margin_el = el;
                is_first = false;

                if (el->src_el()->css().get_position() == element_position_relative)
                {
                    el->apply_relative_shift(self_size);
                }
            }
        }
    }

    pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;

    if (self_size.height.type != containing_block_context::cbc_value_type_auto  && self_size.height > 0)
    {
        physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? (pixel_t)self_size.height : (pixel_t)self_size.width;
        if(src_el()->css().get_display() == display_table_cell)
        {
            physical_block_size = std::max(physical_block_size, block_offset);
            if(collapse_bottom_margin())
            {
                physical_block_size -= last_block_margin;
                if(margin_block_end() < last_block_margin)
                {
                    margin_block_end(last_block_margin);
                }
                if(last_margin_el)
                {
                    last_margin_el->margin_block_end(0);
                }
            }
        }
    } else
    {
        physical_block_size = block_offset;
        if(collapse_bottom_margin())
        {
            physical_block_size -= last_block_margin;
            if(margin_block_end() < last_block_margin)
            {
                margin_block_end(last_block_margin);
            }
			if(last_margin_el)
			{
				last_margin_el->margin_block_end(0);
			}
        }
    }

    return max_inline_size;
}

litehtml::pixel_t litehtml::render_item_block_context::get_first_baseline()
{
    satoru::WritingModeContext wm = get_wm_context();
	if(m_children.empty())
	{
		return block_size(wm) - margin_block_end();
	}
	const auto &item = m_children.front();
	return content_offset_block(wm) + item->block_start_pos(wm) + item->get_first_baseline();
}

litehtml::pixel_t litehtml::render_item_block_context::get_last_baseline()
{
    satoru::WritingModeContext wm = get_wm_context();
	if(m_children.empty())
	{
		return block_size(wm) - margin_block_end();
	}
	const auto &item = m_children.back();
	return content_offset_block(wm) + item->block_start_pos(wm) + item->get_last_baseline();
}


