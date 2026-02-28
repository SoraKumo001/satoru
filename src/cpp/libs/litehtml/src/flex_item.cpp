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
	// Negative numbers are invalid.
	// https://www.w3.org/TR/css-flexbox-1/#valdef-flex-grow-number
	if(grow < 0) grow = 0;

	shrink = (int) std::nearbyint(el->css().get_flex_shrink() * 1000.0);
	// Negative numbers are invalid.
	// https://www.w3.org/TR/css-flexbox-1/#valdef-shrink-number
	if(shrink < 0) shrink = 1000;

	el->calc_outlines(self_size.render_width);
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
					/// If cross axis is reversed position item from start
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
				if(ln.reverse_cross)	/// If cross axis is reversed position item from end
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
	if(m_container_wm.inline_start(el->css().get_margins()).is_predefined())
	{
		auto_margin_main_start = 0;
	}
	if(m_container_wm.inline_end(el->css().get_margins()).is_predefined())
	{
		auto_margin_main_end = 0;
	}
	if(m_container_wm.block_start(el->css().get_margins()).is_predefined())
	{
		auto_margin_cross_start = true;
	}
	if(m_container_wm.block_end(el->css().get_margins()).is_predefined())
	{
		auto_margin_cross_end = true;
	}
	
	def_value<pixel_t> content_size(0);
	if (m_container_wm.get_min_inline_size(el->css()).is_predefined())
	{
		formatting_context fmt_ctx_copy;
		if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
		min_main_size = el->measure(
							  self_size.new_width(get_el_main_content_offset(),
												  containing_block_context::size_mode_content | containing_block_context::size_mode_measure),
							  fmt_ctx ? &fmt_ctx_copy : nullptr);
		content_size = min_main_size;
	} else
	{
		min_main_size = m_container_wm.get_min_inline_size(el->css()).calc_percent(m_container_wm.inline_size(self_size)) +
				   get_el_main_render_offset();
	}
	if (!m_container_wm.get_max_inline_size(el->css()).is_predefined())
	{
		max_main_size = m_container_wm.get_max_inline_size(el->css()).calc_percent(m_container_wm.inline_size(self_size)) +
				   get_el_main_render_offset();
	}
	bool flex_basis_predefined = el->css().get_flex_basis().is_predefined();
	int predef = flex_basis_auto;
	if(flex_basis_predefined)
	{
		predef = el->css().get_flex_basis().predef();
	} else
	{
		if(el->css().get_flex_basis().val() < 0)
		{
			flex_basis_predefined = true;
		}
	}

	if (flex_basis_predefined)
	{
		if(predef == flex_basis_auto && m_container_wm.get_inline_size(el->css()).is_predefined())
		{
			// if width is auto, use content size as base size
			predef = flex_basis_content;
		}

		switch (predef)
		{
			case flex_basis_auto:
				flex_base_size = m_container_wm.get_inline_size(el->css()).calc_percent(m_container_wm.inline_size(self_size)) +
							get_el_main_render_offset();
				break;
			case flex_basis_fit_content:
			case flex_basis_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					flex_base_size = el->measure(self_size.new_width(m_container_wm.inline_size(self_size) + get_el_main_content_offset(),
																	 containing_block_context::size_mode_measure | containing_block_context::size_mode_content),
										   fmt_ctx ? &fmt_ctx_copy : nullptr);
				}
				break;
			case flex_basis_min_content:
				if(content_size.is_default())
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					content_size = el->measure(
											  self_size.new_width(get_el_main_content_offset(),
																  containing_block_context::size_mode_content | containing_block_context::size_mode_measure),
											  fmt_ctx ? &fmt_ctx_copy : nullptr);
				}
				flex_base_size = content_size;
				break;
			case flex_basis_max_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					el->measure(self_size.new_width(0, containing_block_context::size_mode_measure | containing_block_context::size_mode_content), fmt_ctx ? &fmt_ctx_copy : nullptr);
					flex_base_size = get_el_main_size();
				}
				break;
			default:
				flex_base_size = 0;
				break;
		}
	} else
	{
		flex_base_size = el->css().get_flex_basis().calc_percent(m_container_wm.inline_size(self_size)) +
					get_el_main_render_offset();
	}

	scaled_flex_shrink_factor = (flex_base_size - get_el_main_render_offset()) * shrink;
}

void litehtml::flex_item_row_direction::apply_main_auto_margins()
{
	// apply auto margins to item
	if(!auto_margin_main_start.is_default())
	{
		m_container_wm.set_inline_start(el->get_margins(), (pixel_t) auto_margin_main_start);
		if (m_container_wm.is_vertical())
			el->pos().y += (pixel_t) auto_margin_main_start;
		else
			el->pos().x += (pixel_t) auto_margin_main_start;
	}
	if(!auto_margin_main_end.is_default())
		m_container_wm.set_inline_end(el->get_margins(), (pixel_t) auto_margin_main_end);
}

bool litehtml::flex_item_row_direction::apply_cross_auto_margins(pixel_t cross_size)
{
	if(auto_margin_cross_end || auto_margin_cross_start)
	{
		int margins_num = 0;
		if(auto_margin_cross_end)
		{
			margins_num++;
		}
		if(auto_margin_cross_start)
		{
			margins_num++;
		}
		pixel_t margin = (cross_size - get_el_cross_size()) / margins_num;
		if(auto_margin_cross_start)
		{
			m_container_wm.set_block_start(el->get_margins(), margin);
			if (m_container_wm.is_vertical())
				el->pos().x = el->content_block_start();
			else
				el->pos().y = el->content_block_start();
		}
		if(auto_margin_cross_end)
		{
			m_container_wm.set_block_end(el->get_margins(), margin);
		}
		return true;
	}
	return false;
}

void litehtml::flex_item_row_direction::set_main_position(pixel_t pos)
{
	if (m_container_wm.is_vertical())
		el->pos().y = pos + m_container_wm.inline_start(el->get_margins()) + m_container_wm.inline_start(el->get_paddings()) + m_container_wm.inline_start(el->get_borders());
	else
		el->pos().x = pos + m_container_wm.inline_start(el->get_margins()) + m_container_wm.inline_start(el->get_paddings()) + m_container_wm.inline_start(el->get_borders());
}

void litehtml::flex_item_row_direction::set_cross_position(pixel_t pos)
{
	if (m_container_wm.is_vertical())
		el->pos().x = pos + m_container_wm.block_start(el->get_margins()) + m_container_wm.block_start(el->get_paddings()) + m_container_wm.block_start(el->get_borders());
	else
		el->pos().y = pos + m_container_wm.block_start(el->get_margins()) + m_container_wm.block_start(el->get_paddings()) + m_container_wm.block_start(el->get_borders());
}

void litehtml::flex_item_row_direction::layout_item(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size,
													   litehtml::formatting_context *fmt_ctx)
{
	// Create a new containing block context with the calculated width/height
	containing_block_context child_cb = self_size;
	
	pixel_t main_size_no_margins = main_size - get_el_main_offset() + get_el_main_box_sizing_offset();

	if (m_container_wm.is_vertical())
	{
		child_cb.height = main_size_no_margins;
		child_cb.render_height = child_cb.height;
	} else
	{
		child_cb.width = main_size_no_margins;
		child_cb.render_width = child_cb.width;
	}

	bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
	
	if (stretch && (m_container_wm.is_vertical() ? el->css().get_width().is_predefined() : el->css().get_height().is_predefined()))
	{
		pixel_t cross_size_no_margins = ln.cross_size - get_el_cross_offset() + get_el_cross_box_sizing_offset();
		if (m_container_wm.is_vertical())
		{
			child_cb.width = cross_size_no_margins;
			child_cb.render_width = child_cb.width;
		} else
		{
			child_cb.height = cross_size_no_margins;
			child_cb.render_height = child_cb.height;
		}
		child_cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;
	} else {
		if(m_container_wm.block_size(self_size).type == containing_block_context::cbc_value_type_auto)
		{
			if (m_container_wm.is_vertical())
			{
				child_cb.width.type = containing_block_context::cbc_value_type_auto;
				child_cb.render_width.type = containing_block_context::cbc_value_type_auto;
			} else
			{
				child_cb.height.type = containing_block_context::cbc_value_type_auto;
				child_cb.render_height.type = containing_block_context::cbc_value_type_auto;
			}
		} else {
			if (m_container_wm.is_vertical())
			{
				child_cb.width = self_size.render_width;
				child_cb.render_width = self_size.render_width;
			} else
			{
				child_cb.height = self_size.render_height;
				child_cb.render_height = self_size.render_height;
			}
		}
		child_cb.size_mode = m_container_wm.is_vertical() ? containing_block_context::size_mode_exact_height : containing_block_context::size_mode_exact_width;
		if (m_container_wm.is_vertical() ? el->css().get_width().is_predefined() : el->css().get_height().is_predefined())
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
	bool is_cross_predefined = m_container_wm.is_vertical() ? el->css().get_width().is_predefined() : el->css().get_height().is_predefined();
	if (is_cross_predefined)
	{
		containing_block_context cb;
		pixel_t cross_size_no_margins = ln.cross_size - get_el_cross_offset() + get_el_cross_box_sizing_offset();
		if (m_container_wm.is_vertical())
		{
			cb = self_size.new_width_height(
					cross_size_no_margins,
					el->pos().height + get_el_main_box_sizing_offset(),
					containing_block_context::size_mode_exact_width |
					containing_block_context::size_mode_exact_height
					);
		} else
		{
			cb = self_size.new_width_height(
					el->pos().width + get_el_main_box_sizing_offset(),
					cross_size_no_margins,
					containing_block_context::size_mode_exact_width |
					containing_block_context::size_mode_exact_height
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
	return m_container_wm.to_logical(el->width(), el->height()).inline_size;
}

litehtml::pixel_t litehtml::flex_item_row_direction::get_el_cross_size()
{
	return m_container_wm.to_logical(el->width(), el->height()).block_size;
}

////////////////////////////////////////////////////////////////////////////////////

void litehtml::flex_item_column_direction::direction_specific_init(const litehtml::containing_block_context &self_size,
																   litehtml::formatting_context *fmt_ctx)
{
	if(m_container_wm.block_start(el->css().get_margins()).is_predefined())
	{
		auto_margin_main_start = 0;
	}
	if(m_container_wm.block_end(el->css().get_margins()).is_predefined())
	{
		auto_margin_main_end = 0;
	}
	if(m_container_wm.inline_start(el->css().get_margins()).is_predefined())
	{
		auto_margin_cross_start = true;
	}
	if(m_container_wm.inline_end(el->css().get_margins()).is_predefined())
	{
		auto_margin_cross_end = true;
	}
	
	if (m_container_wm.get_min_block_size(el->css()).is_predefined())
	{
		formatting_context fmt_ctx_copy;
		if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
		int mode = containing_block_context::size_mode_measure;
		bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
		if (!stretch) mode |= containing_block_context::size_mode_content;
		
		// When measuring min-block-size, we must respect the available inline-size to allow correct text wrapping.
		el->measure(self_size.new_width(m_container_wm.inline_size(self_size), mode), fmt_ctx ? &fmt_ctx_copy : nullptr);
		min_main_size = get_el_main_size();
	} else
	{
		min_main_size = m_container_wm.get_min_block_size(el->css()).calc_percent(m_container_wm.block_size(self_size)) +
				   get_el_main_render_offset();
	}
	if (!m_container_wm.get_max_block_size(el->css()).is_predefined())
	{
		max_main_size = m_container_wm.get_max_block_size(el->css()).calc_percent(m_container_wm.block_size(self_size)) +
				   get_el_main_render_offset();
	}

	bool flex_basis_predefined = el->css().get_flex_basis().is_predefined();
	int predef = flex_basis_auto;
	if(flex_basis_predefined)
	{
		predef = el->css().get_flex_basis().predef();
	} else
	{
		if(el->css().get_flex_basis().val() < 0)
		{
			flex_basis_predefined = true;
		}
	}

	if (flex_basis_predefined)
	{
		if(predef == flex_basis_auto && m_container_wm.get_block_size(el->css()).is_predefined())
		{
			predef = flex_basis_fit_content;
		}
		switch (predef)
		{
			case flex_basis_auto:
				flex_base_size = m_container_wm.get_block_size(el->css()).calc_percent(m_container_wm.block_size(self_size)) +
							get_el_main_render_offset();
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
		flex_base_size = el->css().get_flex_basis().calc_percent(m_container_wm.block_size(self_size)) +
					get_el_main_render_offset();
	}

	scaled_flex_shrink_factor = (flex_base_size - get_el_main_render_offset()) * shrink;
}

void litehtml::flex_item_column_direction::apply_main_auto_margins()
{
	// apply auto margins to item
	if(!auto_margin_main_start.is_default())
	{
		m_container_wm.set_block_start(el->get_margins(), (pixel_t) auto_margin_main_start);
		if (m_container_wm.is_vertical())
			el->pos().x += (pixel_t) auto_margin_main_start;
		else
			el->pos().y += (pixel_t) auto_margin_main_start;
	}
	if(!auto_margin_main_end.is_default())
		m_container_wm.set_block_end(el->get_margins(), (pixel_t) auto_margin_main_end);
}

bool litehtml::flex_item_column_direction::apply_cross_auto_margins(pixel_t cross_size)
{
	if(auto_margin_cross_end || auto_margin_cross_start)
	{
		int margins_num = 0;
		if(auto_margin_cross_end)
		{
			margins_num++;
		}
		if(auto_margin_cross_start)
		{
			margins_num++;
		}
		pixel_t margin = (cross_size - get_el_cross_size()) / margins_num;
		if(auto_margin_cross_start)
		{
			m_container_wm.set_inline_start(el->get_margins(), margin);
			if (m_container_wm.is_vertical())
				el->pos().y = el->content_inline_start();
			else
				el->pos().x = el->content_inline_start();
		}
		if(auto_margin_cross_end)
		{
			m_container_wm.set_inline_end(el->get_margins(), margin);
		}
		return true;
	}
	return false;
}

void litehtml::flex_item_column_direction::set_main_position(pixel_t pos)
{
	if (m_container_wm.is_vertical())
		el->pos().x = pos + m_container_wm.block_start(el->get_margins()) + m_container_wm.block_start(el->get_paddings()) + m_container_wm.block_start(el->get_borders());
	else
		el->pos().y = pos + m_container_wm.block_start(el->get_margins()) + m_container_wm.block_start(el->get_paddings()) + m_container_wm.block_start(el->get_borders());
}

void litehtml::flex_item_column_direction::set_cross_position(pixel_t pos)
{
	if (m_container_wm.is_vertical())
		el->pos().y = pos + m_container_wm.inline_start(el->get_margins()) + m_container_wm.inline_start(el->get_paddings()) + m_container_wm.inline_start(el->get_borders());
	else
		el->pos().x = pos + m_container_wm.inline_start(el->get_margins()) + m_container_wm.inline_start(el->get_paddings()) + m_container_wm.inline_start(el->get_borders());
}

void litehtml::flex_item_column_direction::layout_item(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size,
													   litehtml::formatting_context *fmt_ctx)
{
	containing_block_context child_cb = self_size;

	pixel_t main_size_no_margins = main_size - get_el_main_offset() + get_el_main_box_sizing_offset();

	if (m_container_wm.is_vertical())
	{
		child_cb.width = main_size_no_margins;
		child_cb.render_width = child_cb.width;
	} else
	{
		child_cb.height = main_size_no_margins;
		child_cb.render_height = child_cb.height;
	}

	bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;

	if (stretch && (m_container_wm.is_vertical() ? el->css().get_height().is_predefined() : el->css().get_width().is_predefined()))
	{
		pixel_t cross_size_no_margins = ln.cross_size - get_el_cross_offset() + get_el_cross_box_sizing_offset();
		if (m_container_wm.is_vertical())
		{
			child_cb.height = cross_size_no_margins;
			child_cb.render_height = child_cb.height;
		} else
		{
			child_cb.width = cross_size_no_margins;
			child_cb.render_width = child_cb.width;
		}
		child_cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;
	} else {
		if (m_container_wm.is_vertical())
		{
			child_cb.height = self_size.render_height;
			child_cb.render_height = self_size.render_height;
		} else
		{
			child_cb.width = self_size.render_width;
			child_cb.render_width = self_size.render_width;
		}
		child_cb.size_mode = m_container_wm.is_vertical() ? containing_block_context::size_mode_exact_width : containing_block_context::size_mode_exact_height;
		if (m_container_wm.is_vertical() ? el->css().get_height().is_predefined() : el->css().get_width().is_predefined())
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
	bool is_cross_predefined = m_container_wm.is_vertical() ? el->css().get_height().is_predefined() : el->css().get_width().is_predefined();
	if (is_cross_predefined)
	{
		containing_block_context cb;
		pixel_t cross_size_no_margins = ln.cross_size - get_el_cross_offset() + get_el_cross_box_sizing_offset();
		if (m_container_wm.is_vertical())
		{
			cb = self_size.new_width_height(
					el->pos().width + get_el_main_box_sizing_offset(),
					cross_size_no_margins,
					containing_block_context::size_mode_exact_width |
					containing_block_context::size_mode_exact_height
					);
		} else
		{
			cb = self_size.new_width_height(
					cross_size_no_margins,
					el->pos().height + get_el_main_box_sizing_offset(),
					containing_block_context::size_mode_exact_width |
					containing_block_context::size_mode_exact_height
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
	// column direction doesn't support baseline alignment
	set_cross_position(ln.cross_start);
}

litehtml::pixel_t litehtml::flex_item_column_direction::get_el_main_size()
{
	return m_container_wm.to_logical(el->width(), el->height()).block_size;
}

litehtml::pixel_t litehtml::flex_item_column_direction::get_el_cross_size()
{
	return m_container_wm.to_logical(el->width(), el->height()).inline_size;
}
