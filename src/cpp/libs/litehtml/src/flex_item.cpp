#include <cmath>
#include <cstdio>
#include "flex_item.h"
#include "flex_line.h"
#include "types.h"

void litehtml::flex_item::init(const litehtml::containing_block_context &self_size,
							   litehtml::formatting_context *fmt_ctx, flex_align_items align_items)
{
	m_container_wm = satoru::WritingModeContext(self_size.mode, self_size.width, self_size.height);

	grow = (int) std::nearbyint(el->css().get_flex_grow() * 1000.0);
	if(grow < 0) grow = 0;

	shrink = (int) std::nearbyint(el->css().get_flex_shrink() * 1000.0);
	if(shrink < 0) shrink = 1000;

	el->calc_outlines(self_size.render_inline_size());
	order = el->css().get_order();

	if (el->css().get_flex_align_self() == flex_align_items_auto)
	{
		align = align_items;
	} else
	{
		align = el->css().get_flex_align_self();
	}

	direction_specific_init(self_size, fmt_ctx);

	if (flex_base_size < min_main_size)
	{
		hypothetical_main_size = min_main_size;
	} else if (!max_main_size.is_default() && flex_base_size > max_main_size)
	{
		hypothetical_main_size = max_main_size;
	} else
	{
		hypothetical_main_size = flex_base_size;
	}

	main_size = hypothetical_main_size;
	frozen = false;
}

void litehtml::flex_item::place(flex_line &ln, pixel_t main_pos,
								const containing_block_context &self_size,
								formatting_context *fmt_ctx)
{
	layout_item(ln, self_size, fmt_ctx);
	apply_main_auto_margins();
	set_main_position(main_pos);

	if(!apply_cross_auto_margins(ln.cross_size))
	{
		switch (align & 0xFF)
		{
			case flex_align_items_baseline:
				align_baseline(ln, self_size, fmt_ctx);
				break;
			case flex_align_items_flex_end:
				if(ln.reverse_cross)
				{
					set_cross_position(ln.cross_start);
				} else
				{
					set_cross_position(ln.cross_start + ln.cross_size - get_el_cross_size());
				}
				break;
			case flex_align_items_end:
				set_cross_position(ln.cross_start + ln.cross_size - get_el_cross_size());
				break;
			case flex_align_items_center:
				set_cross_position(ln.cross_start + ln.cross_size / 2 - get_el_cross_size() / 2);
				break;
			case flex_align_items_flex_start:
				if(ln.reverse_cross)
				{
					set_cross_position(ln.cross_start + ln.cross_size - get_el_cross_size());
				} else
				{
					set_cross_position(ln.cross_start);
				}
				break;
			case flex_align_items_start:
				set_cross_position(ln.cross_start);
				break;
			default:
				align_stretch(ln, self_size, fmt_ctx);
				break;
		}
	}
}

litehtml::pixel_t litehtml::flex_item::get_last_baseline(baseline::_baseline_type type) const
{
	if(type == baseline::baseline_type_top)
	{
		return el->get_last_baseline();
	} else if(type == baseline::baseline_type_bottom)
	{
		return el->height() - el->get_last_baseline();
	}
	return 0;
}

litehtml::pixel_t litehtml::flex_item::get_first_baseline(litehtml::baseline::_baseline_type type) const
{
	if(type == baseline::baseline_type_top)
	{
		return el->get_first_baseline();
	} else if(type == baseline::baseline_type_bottom)
	{
		return el->height() - el->get_first_baseline();
	}
	return 0;
}


////////////////////////////////////////////////////////////////////////////////////

void litehtml::flex_item_row_direction::direction_specific_init(const litehtml::containing_block_context &self_size,
																litehtml::formatting_context *fmt_ctx)
{
	if (m_container_wm.is_vertical())
	{
		if(el->css().get_margins().top.is_predefined()) auto_margin_main_start = 0;
		if(el->css().get_margins().bottom.is_predefined()) auto_margin_main_end = 0;
		if(el->css().get_margins().left.is_predefined()) auto_margin_cross_start = true;
		if(el->css().get_margins().right.is_predefined()) auto_margin_cross_end = true;
	} else
	{
		if(el->css().get_margins().left.is_predefined()) auto_margin_main_start = 0;
		if(el->css().get_margins().right.is_predefined()) auto_margin_main_end = 0;
		if(el->css().get_margins().top.is_predefined()) auto_margin_cross_start = true;
		if(el->css().get_margins().bottom.is_predefined()) auto_margin_cross_end = true;
	}
	
	def_value<pixel_t> content_size(0);
	const css_length& min_size_prop = m_container_wm.is_vertical() ? el->css().get_min_height() : el->css().get_min_width();
	const css_length& max_size_prop = m_container_wm.is_vertical() ? el->css().get_max_height() : el->css().get_max_width();
	const css_length& size_prop = m_container_wm.is_vertical() ? el->css().get_height() : el->css().get_width();
	pixel_t render_offset = m_container_wm.is_vertical() ? el->render_offset_height() : el->render_offset_width();
	pixel_t content_offset = m_container_wm.is_vertical() ? el->content_offset_height() : el->content_offset_width();
	pixel_t parent_size = m_container_wm.is_vertical() ? self_size.height : self_size.width;

	if (min_size_prop.is_predefined())
	{
		formatting_context fmt_ctx_copy;
		if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
		min_main_size = el->measure(
							  m_container_wm.is_vertical() ? self_size.new_height(content_offset, containing_block_context::size_mode_content | containing_block_context::size_mode_measure)
														   : self_size.new_width(content_offset, containing_block_context::size_mode_content | containing_block_context::size_mode_measure),
							  fmt_ctx ? &fmt_ctx_copy : nullptr);
		content_size = min_main_size;
	} else
	{
		min_main_size = min_size_prop.calc_percent(parent_size) + render_offset;
	}
	if (!max_size_prop.is_predefined())
	{
		max_main_size = max_size_prop.calc_percent(parent_size) + render_offset;
	}

	bool flex_basis_predefined = el->css().get_flex_basis().is_predefined();
	int predef = flex_basis_auto;
	if(flex_basis_predefined) predef = el->css().get_flex_basis().predef();
	else if(el->css().get_flex_basis().val() < 0) flex_basis_predefined = true;

	if (flex_basis_predefined)
	{
		if(predef == flex_basis_auto && size_prop.is_predefined()) predef = flex_basis_content;

		switch (predef)
		{
			case flex_basis_auto:
				flex_base_size = size_prop.calc_percent(parent_size) + render_offset;
				break;
			case flex_basis_fit_content:
			case flex_basis_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					flex_base_size = el->measure(
						m_container_wm.is_vertical() ? self_size.new_height(self_size.height + content_offset, containing_block_context::size_mode_measure | containing_block_context::size_mode_content)
													 : self_size.new_width(self_size.width + content_offset, containing_block_context::size_mode_measure | containing_block_context::size_mode_content),
						fmt_ctx ? &fmt_ctx_copy : nullptr);
				}
				break;
			case flex_basis_min_content:
				if(content_size.is_default())
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					content_size = el->measure(
						m_container_wm.is_vertical() ? self_size.new_height(content_offset, containing_block_context::size_mode_content | containing_block_context::size_mode_measure)
													 : self_size.new_width(content_offset, containing_block_context::size_mode_content | containing_block_context::size_mode_measure),
						fmt_ctx ? &fmt_ctx_copy : nullptr);
				}
				flex_base_size = content_size;
				break;
			case flex_basis_max_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					el->measure(
						m_container_wm.is_vertical() ? self_size.new_height(0, containing_block_context::size_mode_measure | containing_block_context::size_mode_content)
													 : self_size.new_width(0, containing_block_context::size_mode_measure | containing_block_context::size_mode_content),
						fmt_ctx ? &fmt_ctx_copy : nullptr);
					flex_base_size = get_el_main_size();
				}
				break;
			default:
				flex_base_size = 0;
				break;
		}
	} else
	{
		flex_base_size = el->css().get_flex_basis().calc_percent(parent_size) + render_offset;
	}

	scaled_flex_shrink_factor = (flex_base_size - render_offset) * shrink;
}

void litehtml::flex_item_row_direction::apply_main_auto_margins()
{
	if (m_container_wm.is_vertical())
	{
		if(!auto_margin_main_start.is_default())
		{
			el->get_margins().top = auto_margin_main_start;
			el->pos().y += auto_margin_main_start;
		}
		if(!auto_margin_main_end.is_default()) el->get_margins().bottom = auto_margin_main_end;
	} else
	{
		if(!auto_margin_main_start.is_default())
		{
			el->get_margins().left = auto_margin_main_start;
			el->pos().x += auto_margin_main_start;
		}
		if(!auto_margin_main_end.is_default()) el->get_margins().right = auto_margin_main_end;
	}
}

bool litehtml::flex_item_row_direction::apply_cross_auto_margins(pixel_t cross_size)
{
	if(auto_margin_cross_end || auto_margin_cross_start)
	{
		int margins_num = 0;
		if(auto_margin_cross_end) margins_num++;
		if(auto_margin_cross_start) margins_num++;
		pixel_t margin = (cross_size - get_el_cross_size()) / margins_num;
		if (m_container_wm.is_vertical())
		{
			if(auto_margin_cross_start)
			{
                // In vertical-rl, "cross-start" is physically left, but logical block-start is right.
                // However, litehtml's flex engine treats cross-start as the start of the cross axis.
                // For vertical-rl, cross axis is physical X.
				el->get_margins().left = margin;
                if (m_container_wm.mode() == writing_mode_vertical_rl)
                {
                    // This will be handled by set_cross_position calling with appropriate cross_start
                }
                else
                {
                    el->pos().x = el->content_offset_left();
                }
			}
			if(auto_margin_cross_end) el->get_margins().right = margin;
		} else
		{
			if(auto_margin_cross_start)
			{
				el->get_margins().top = margin;
				el->pos().y = el->content_offset_top();
			}
			if(auto_margin_cross_end) el->get_margins().bottom = margin;
		}
		return true;
	}
	return false;
}

void litehtml::flex_item_row_direction::set_main_position(pixel_t pos)
{
    // Inline axis
    if (m_container_wm.is_vertical())
        el->pos().y = pos + el->content_offset_top();
    else
        el->pos().x = pos + el->content_offset_left();
}

void litehtml::flex_item_row_direction::set_cross_position(pixel_t pos)
{
    // Block axis
    if (m_container_wm.is_vertical())
    {
        // For vertical-rl, the physical X is calculated from the right edge.
        if (m_container_wm.mode() == writing_mode_vertical_rl)
        {
            el->pos().x = m_container_wm.container_width() - pos - get_el_cross_size() + el->content_offset_left();
        }
        else
        {
            el->pos().x = pos + el->content_offset_left();
        }
    }
    else
    {
        el->pos().y = pos + el->content_offset_top();
    }
}

void litehtml::flex_item_row_direction::layout_item(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size,
													   litehtml::formatting_context *fmt_ctx)
{
	containing_block_context child_cb = self_size;
    child_cb.inline_size() = main_size - el->content_offset_inline(m_container_wm) + el->box_sizing_inline(m_container_wm);
    child_cb.render_inline_size() = child_cb.inline_size();

	bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
	const css_length& cross_prop = m_container_wm.is_vertical() ? el->css().get_width() : el->css().get_height();

	if (stretch && cross_prop.is_predefined())
	{
        child_cb.block_size() = ln.cross_size - el->content_offset_block(m_container_wm) + el->box_sizing_block(m_container_wm);
        child_cb.render_block_size() = child_cb.block_size();
		child_cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;
	} else {
        if (child_cb.block_size().type == containing_block_context::cbc_value_type_auto)
        {
            child_cb.block_size().type = containing_block_context::cbc_value_type_auto;
            child_cb.render_block_size().type = containing_block_context::cbc_value_type_auto;
        } else
        {
            child_cb.block_size() = self_size.render_block_size();
            child_cb.render_block_size() = self_size.render_block_size();
        }
        
        if (m_container_wm.is_vertical())
        {
            child_cb.size_mode = containing_block_context::size_mode_exact_height;
        } else
        {
            child_cb.size_mode = containing_block_context::size_mode_exact_width;
        }
		if (cross_prop.is_predefined())
		{
			child_cb.size_mode |= containing_block_context::size_mode_content;
		}
	}

	el->measure(child_cb, fmt_ctx);
	if (!(self_size.size_mode & containing_block_context::size_mode_measure))
	{
		el->place(0, 0, child_cb, fmt_ctx);
	}
}

void litehtml::flex_item_row_direction::align_stretch(flex_line &ln, const containing_block_context &self_size,
													  formatting_context *fmt_ctx)
{
	set_cross_position(ln.cross_start);
	const css_length& cross_prop = m_container_wm.is_vertical() ? el->css().get_width() : el->css().get_height();
	if (cross_prop.is_predefined())
	{
		containing_block_context cb;
		if (m_container_wm.is_vertical())
		{
			cb = self_size.new_width_height(
					ln.cross_size - el->content_offset_width() + el->box_sizing_width(),
					el->pos().height + el->box_sizing_height(),
					containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height
					);
		} else
		{
			cb = self_size.new_width_height(
					el->pos().width + el->box_sizing_width(),
					ln.cross_size - el->content_offset_height() + el->box_sizing_height(),
					containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height
					);
		}
		if (self_size.size_mode & containing_block_context::size_mode_measure)
		{
			el->measure(cb, fmt_ctx);
		} else
		{
			el->measure(cb, fmt_ctx);
			el->place(el->left(), el->top(), cb, fmt_ctx);
		}
		apply_main_auto_margins();
	}
}

void litehtml::flex_item_row_direction::align_baseline(litehtml::flex_line &ln,
													   const containing_block_context &/*self_size*/,
													   formatting_context */*fmt_ctx*/)
{
	if (align & flex_align_items_last)
	{
		set_cross_position(ln.cross_start + ln.last_baseline.get_offset_from_top(ln.cross_size) - el->get_last_baseline());
	} else
	{
		set_cross_position(ln.cross_start + ln.first_baseline.get_offset_from_top(ln.cross_size) - el->get_first_baseline());
	}
}

litehtml::pixel_t litehtml::flex_item_row_direction::get_el_main_size()
{
	return m_container_wm.is_vertical() ? el->height() : el->width();
}

litehtml::pixel_t litehtml::flex_item_row_direction::get_el_cross_size()
{
	return m_container_wm.is_vertical() ? el->width() : el->height();
}

////////////////////////////////////////////////////////////////////////////////////

void litehtml::flex_item_column_direction::direction_specific_init(const litehtml::containing_block_context &self_size,
																   litehtml::formatting_context *fmt_ctx)
{
	if (m_container_wm.is_vertical())
	{
		if(el->css().get_margins().left.is_predefined()) auto_margin_main_start = 0;
		if(el->css().get_margins().right.is_predefined()) auto_margin_main_end = 0;
		if(el->css().get_margins().top.is_predefined()) auto_margin_cross_start = true;
		if(el->css().get_margins().bottom.is_predefined()) auto_margin_cross_end = true;
	} else
	{
		if(el->css().get_margins().top.is_predefined()) auto_margin_main_start = 0;
		if(el->css().get_margins().bottom.is_predefined()) auto_margin_main_end = 0;
		if(el->css().get_margins().left.is_predefined()) auto_margin_cross_start = true;
		if(el->css().get_margins().right.is_predefined()) auto_margin_cross_end = true;
	}
	
	const css_length& min_size_prop = m_container_wm.is_vertical() ? el->css().get_min_width() : el->css().get_min_height();
	const css_length& max_size_prop = m_container_wm.is_vertical() ? el->css().get_max_width() : el->css().get_max_height();
	const css_length& size_prop = m_container_wm.is_vertical() ? el->css().get_width() : el->css().get_height();
	pixel_t render_offset = m_container_wm.is_vertical() ? el->render_offset_width() : el->render_offset_height();
	pixel_t content_offset = m_container_wm.is_vertical() ? el->content_offset_width() : el->content_offset_height();
	pixel_t parent_size = m_container_wm.is_vertical() ? self_size.width : self_size.height;

	if (min_size_prop.is_predefined())
	{
		formatting_context fmt_ctx_copy;
		if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
		int mode = containing_block_context::size_mode_measure;
		bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
		if (!stretch) mode |= containing_block_context::size_mode_content;
		
		el->measure(
			m_container_wm.is_vertical() ? self_size.new_height(self_size.render_height, mode)
										 : self_size.new_width(self_size.render_width, mode),
			fmt_ctx ? &fmt_ctx_copy : nullptr);
		min_main_size = get_el_main_size();
	} else
	{
		min_main_size = min_size_prop.calc_percent(parent_size) + render_offset;
	}
	if (!max_size_prop.is_predefined())
	{
		max_main_size = max_size_prop.calc_percent(parent_size) + render_offset;
	}

	bool flex_basis_predefined = el->css().get_flex_basis().is_predefined();
	int predef = flex_basis_auto;
	if(flex_basis_predefined) predef = el->css().get_flex_basis().predef();
	else if(el->css().get_flex_basis().val() < 0) flex_basis_predefined = true;

	if (flex_basis_predefined)
	{
		if(predef == flex_basis_auto && size_prop.is_predefined()) predef = flex_basis_fit_content;
		switch (predef)
		{
			case flex_basis_auto:
				flex_base_size = size_prop.calc_percent(parent_size) + render_offset;
				break;
			case flex_basis_max_content:
			case flex_basis_fit_content:
				{
					containing_block_context measure_size = self_size;
					if (m_container_wm.is_vertical())
					{
						measure_size.width.type = containing_block_context::cbc_value_type_auto;
						measure_size.render_width.type = containing_block_context::cbc_value_type_auto;
						measure_size.size_mode &= ~containing_block_context::size_mode_exact_width;
					} else
					{
						measure_size.height.type = containing_block_context::cbc_value_type_auto;
						measure_size.render_height.type = containing_block_context::cbc_value_type_auto;
						measure_size.size_mode &= ~containing_block_context::size_mode_exact_height;
					}
					measure_size.size_mode |= containing_block_context::size_mode_measure;
					bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
					if (!stretch) measure_size.size_mode |= containing_block_context::size_mode_content;
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					el->measure(measure_size, fmt_ctx ? &fmt_ctx_copy : nullptr);
					flex_base_size = get_el_main_size();
				}
				break;
			case flex_basis_min_content:
				flex_base_size = min_main_size;
				break;
			default:
				flex_base_size = 0;
		}
	} else
	{
		flex_base_size = el->css().get_flex_basis().calc_percent(parent_size) + render_offset;
	}

	scaled_flex_shrink_factor = (flex_base_size - render_offset) * shrink;
}

void litehtml::flex_item_column_direction::apply_main_auto_margins()
{
	if (m_container_wm.is_vertical())
	{
		if(!auto_margin_main_start.is_default())
		{
			el->get_margins().left = auto_margin_main_start;
			el->pos().x += auto_margin_main_start;
		}
		if(!auto_margin_main_end.is_default()) el->get_margins().right = auto_margin_main_end;
	} else
	{
		if(!auto_margin_main_start.is_default())
		{
			el->get_margins().top = auto_margin_main_start;
			el->pos().y += auto_margin_main_start;
		}
		if(!auto_margin_main_end.is_default()) el->get_margins().bottom = auto_margin_main_end;
	}
}

bool litehtml::flex_item_column_direction::apply_cross_auto_margins(pixel_t cross_size)
{
	if(auto_margin_cross_end || auto_margin_cross_start)
	{
		int margins_num = 0;
		if(auto_margin_cross_end) margins_num++;
		if(auto_margin_cross_start) margins_num++;
		pixel_t margin = (cross_size - get_el_cross_size()) / margins_num;
		if (m_container_wm.is_vertical())
		{
			if(auto_margin_cross_start)
			{
				el->get_margins().top = margin;
				el->pos().y = el->content_offset_top();
			}
			if(auto_margin_cross_end) el->get_margins().bottom = margin;
		} else
		{
			if(auto_margin_cross_start)
			{
				el->get_margins().left = margin;
                if (m_container_wm.mode() == writing_mode_vertical_rl)
                {
                    // For vertical-rl, cross axis is physical X, but in column direction, cross axis is Inline.
                    // This will be handled by set_cross_position
                }
                else
                {
                    el->pos().x = el->content_offset_left();
                }
			}
			if(auto_margin_cross_end) el->get_margins().right = margin;
		}
		return true;
	}
	return false;
}

void litehtml::flex_item_column_direction::set_main_position(pixel_t pos)
{
    // Inline axis
    if (m_container_wm.is_vertical())
    {
        if (m_container_wm.mode() == writing_mode_vertical_rl)
        {
            el->pos().x = m_container_wm.container_width() - pos - get_el_main_size() + el->content_offset_left();
        }
        else
        {
            el->pos().x = pos + el->content_offset_left();
        }
    }
    else
    {
        el->pos().y = pos + el->content_offset_top();
    }
}

void litehtml::flex_item_column_direction::set_cross_position(pixel_t pos)
{
    // Block axis
    if (m_container_wm.is_vertical())
    {
        el->pos().y = pos + el->content_offset_top();
    }
    else
    {
        el->pos().x = pos + el->content_offset_left();
    }
}

void litehtml::flex_item_column_direction::layout_item(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size,
													   litehtml::formatting_context *fmt_ctx)
{
	containing_block_context child_cb = self_size;
	if (m_container_wm.is_vertical())
	{
		child_cb.width = main_size - el->content_offset_width() + el->box_sizing_width();
		child_cb.render_width = child_cb.width;
	} else
	{
		child_cb.height = main_size - el->content_offset_height() + el->box_sizing_height();
		child_cb.render_height = child_cb.height;
	}

	bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
	const css_length& cross_prop = m_container_wm.is_vertical() ? el->css().get_height() : el->css().get_width();

	if (stretch && cross_prop.is_predefined())
	{
		if (m_container_wm.is_vertical())
		{
			child_cb.height = ln.cross_size - el->content_offset_height() + el->box_sizing_height();
			child_cb.render_height = child_cb.height;
		} else
		{
			child_cb.width = ln.cross_size - el->content_offset_width() + el->box_sizing_width();
			child_cb.render_width = child_cb.width;
		}
		child_cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;
	} else {
		if (m_container_wm.is_vertical())
		{
			child_cb.height = self_size.render_height;
			child_cb.render_height = self_size.render_height;
			child_cb.size_mode = containing_block_context::size_mode_exact_width;
		} else
		{
			child_cb.width = self_size.render_width;
			child_cb.render_width = self_size.render_width;
			child_cb.size_mode = containing_block_context::size_mode_exact_height;
		}
		if (cross_prop.is_predefined())
		{
			child_cb.size_mode |= containing_block_context::size_mode_content;
		}
	}

	el->measure(child_cb, fmt_ctx);
	if (!(self_size.size_mode & containing_block_context::size_mode_measure))
	{
		el->place(0, 0, child_cb, fmt_ctx);
	}
}

void litehtml::flex_item_column_direction::align_stretch(flex_line &ln, const containing_block_context &self_size,
													  formatting_context *fmt_ctx)
{
	set_cross_position(ln.cross_start);
	const css_length& cross_prop = m_container_wm.is_vertical() ? el->css().get_height() : el->css().get_width();
	if (cross_prop.is_predefined())
	{
		containing_block_context cb;
		if (m_container_wm.is_vertical())
		{
			cb = self_size.new_width_height(
					el->pos().width + el->box_sizing_width(),
					ln.cross_size - el->content_offset_height() + el->box_sizing_height(),
					containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height
					);
		} else
		{
			cb = self_size.new_width_height(
					ln.cross_size - el->content_offset_width() + el->box_sizing_width(),
					el->pos().height + el->box_sizing_height(),
					containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height
					);
		}

		if (self_size.size_mode & containing_block_context::size_mode_measure)
		{
			el->measure(cb, fmt_ctx);
		} else
		{
			el->measure(cb, fmt_ctx);
			el->place(el->left(), el->top(), cb, fmt_ctx);
		}
		apply_main_auto_margins();
	}
}

void litehtml::flex_item_column_direction::align_baseline(litehtml::flex_line &ln,
													   const containing_block_context &self_size,
													   formatting_context *fmt_ctx)
{
	set_cross_position(ln.cross_start);
}

litehtml::pixel_t litehtml::flex_item_column_direction::get_el_main_size()
{
	return m_container_wm.is_vertical() ? el->width() : el->height();
}

litehtml::pixel_t litehtml::flex_item_column_direction::get_el_cross_size()
{
	return m_container_wm.is_vertical() ? el->height() : el->width();
}
