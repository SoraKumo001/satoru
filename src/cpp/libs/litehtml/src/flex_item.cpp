#include <cmath>
#include <cstdio>
#include "flex_item.h"
#include "flex_line.h"
#include "types.h"
#include <iostream>

void litehtml::flex_item::init(const litehtml::containing_block_context &self_size_const,
							   litehtml::formatting_context *fmt_ctx, flex_align_items align_items)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

	m_container_wm = satoru::WritingModeContext(self_size.mode, (pixel_t) self_size.width, (pixel_t) self_size.height);

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
								const containing_block_context &self_size_const,
								formatting_context *fmt_ctx)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

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
	} else {
		set_cross_position(ln.cross_start);
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

void litehtml::flex_item_row_direction::direction_specific_init(const litehtml::containing_block_context &self_size_const,
																litehtml::formatting_context *fmt_ctx)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

    // Map logical margins to auto_margin flags
    const auto& margins = el->css().get_margins();
    if(m_container_wm.inline_start(margins).is_predefined()) auto_margin_main_start = 0;
    if(m_container_wm.inline_end(margins).is_predefined()) auto_margin_main_end = 0;
    if(m_container_wm.block_start(margins).is_predefined()) auto_margin_cross_start = true;
    if(m_container_wm.block_end(margins).is_predefined()) auto_margin_cross_end = true;

	def_value<pixel_t> content_size(0);
    
    // No helpers for min/max properties in WritingModeContext, so manual branching
    const css_length& min_size_prop = m_container_wm.is_vertical() ? el->css().get_min_height() : el->css().get_min_width();
    const css_length& max_size_prop = m_container_wm.is_vertical() ? el->css().get_max_height() : el->css().get_max_width();
    const css_length& size_prop = m_container_wm.get_inline_size(el->css()); // Main size prop

    pixel_t render_offset = el->render_offset_width(); 
    if(m_container_wm.is_vertical()) render_offset = el->render_offset_height();

	pixel_t content_offset = el->content_offset_inline(m_container_wm);
	pixel_t parent_size = self_size.render_inline_size();

	if (min_size_prop.is_predefined())
	{
		if (el->css().get_overflow() == overflow_visible ||
			el->src_el()->css().get_display() == display_webkit_box)
		{
			formatting_context fmt_ctx_copy;
			if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
            
            containing_block_context::typed_pixel tp_content_offset(content_offset, containing_block_context::cbc_value_type_auto);
            containing_block_context measure_cb = m_container_wm.update_logical(self_size, tp_content_offset, std::nullopt);
            measure_cb.size_mode = containing_block_context::size_mode_content | containing_block_context::size_mode_measure;
            el->measure(measure_cb, fmt_ctx ? &fmt_ctx_copy : nullptr);

			min_main_size = get_el_main_size();
			content_size = min_main_size;
		} else
		{
			min_main_size = render_offset;
			content_size = min_main_size;
		}
	} else
	{
		min_main_size = min_size_prop.calc_percent(parent_size) + render_offset - el->box_sizing_inline(m_container_wm);
	}
	if (!max_size_prop.is_predefined())
	{
		max_main_size = max_size_prop.calc_percent(parent_size) + render_offset - el->box_sizing_inline(m_container_wm);
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
				flex_base_size = size_prop.calc_percent(parent_size) + render_offset - el->box_sizing_inline(m_container_wm);
				break;
			case flex_basis_fit_content:
			case flex_basis_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
                    
                    pixel_t logical_is_val = m_container_wm.to_logical(self_size.width, self_size.height).inline_size + content_offset;
                    containing_block_context::typed_pixel tp_logical_is(logical_is_val, containing_block_context::cbc_value_type_auto);
                    
                    containing_block_context measure_cb = m_container_wm.update_logical(self_size, tp_logical_is, std::nullopt);
                    measure_cb.size_mode = containing_block_context::size_mode_measure | containing_block_context::size_mode_content;
                    
					el->measure(measure_cb, fmt_ctx ? &fmt_ctx_copy : nullptr);
					flex_base_size = get_el_main_size();
				}
				break;
			case flex_basis_min_content:
				if(content_size.is_default())
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
                    
                    containing_block_context::typed_pixel tp_content_offset(content_offset, containing_block_context::cbc_value_type_auto);
                    containing_block_context measure_cb = m_container_wm.update_logical(self_size, tp_content_offset, std::nullopt);
                    measure_cb.size_mode = containing_block_context::size_mode_content | containing_block_context::size_mode_measure;
                    
					el->measure(measure_cb, fmt_ctx ? &fmt_ctx_copy : nullptr);
					content_size = get_el_main_size();
				}
				flex_base_size = content_size;
				break;
			case flex_basis_max_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
                    
                    containing_block_context::typed_pixel tp_zero(0, containing_block_context::cbc_value_type_auto);
                    containing_block_context measure_cb = m_container_wm.update_logical(self_size, tp_zero, std::nullopt);
                    measure_cb.size_mode = containing_block_context::size_mode_measure | containing_block_context::size_mode_content;
                    
					el->measure(measure_cb, fmt_ctx ? &fmt_ctx_copy : nullptr);
					flex_base_size = get_el_main_size();
				}
				break;
			default:
				flex_base_size = 0;
				break;
		}
	} else
	{
		flex_base_size = el->css().get_flex_basis().calc_percent(parent_size) + render_offset - el->box_sizing_inline(m_container_wm);
	}

	scaled_flex_shrink_factor = (flex_base_size - render_offset) * shrink;
}

void litehtml::flex_item_row_direction::apply_main_auto_margins()
{
    if(!auto_margin_main_start.is_default()) m_container_wm.set_inline_start(el->get_margins(), auto_margin_main_start);
    if(!auto_margin_main_end.is_default()) m_container_wm.set_inline_end(el->get_margins(), auto_margin_main_end);
}

bool litehtml::flex_item_row_direction::apply_cross_auto_margins(pixel_t cross_size)
{
	if(auto_margin_cross_end || auto_margin_cross_start)
	{
		int margins_num = 0;
		if(auto_margin_cross_end) margins_num++;
		if(auto_margin_cross_start) margins_num++;
		pixel_t margin = (cross_size - get_el_cross_size()) / margins_num;

        if(auto_margin_cross_start) m_container_wm.set_block_start(el->get_margins(), margin);
        if(auto_margin_cross_end) m_container_wm.set_block_end(el->get_margins(), margin);
        
		return true;
	}
	return false;
}

void litehtml::flex_item_row_direction::set_main_position(pixel_t pos)
{
    main_pos = pos;
}

void litehtml::flex_item_row_direction::set_cross_position(pixel_t pos)
{
    cross_pos = pos;
}

void litehtml::flex_item_row_direction::layout_item(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size_const,
													   litehtml::formatting_context *fmt_ctx)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

	containing_block_context child_cb = self_size;
	child_cb.mode = m_container_wm.mode();
    // Main Axis = Inline
    child_cb.inline_size() = main_size - el->content_offset_inline(m_container_wm) + el->box_sizing_inline(m_container_wm);
    child_cb.render_inline_size() = child_cb.inline_size();

	bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
	const css_length& cross_prop = m_container_wm.get_block_size(el->css());

	if (stretch && cross_prop.is_predefined())
	{
        // Cross Axis = Block
        child_cb.block_size() = ln.cross_size - el->content_offset_block(m_container_wm) + el->box_sizing_block(m_container_wm);
        child_cb.render_block_size() = child_cb.block_size();
		child_cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;
	} else {
        child_cb.block_size().type = containing_block_context::cbc_value_type_auto;
        child_cb.render_block_size().type = containing_block_context::cbc_value_type_auto;
        child_cb.size_mode &= ~(containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height);
        
        if (m_container_wm.is_vertical())
        {
            child_cb.size_mode |= containing_block_context::size_mode_exact_height;
        } else
        {
            child_cb.size_mode |= containing_block_context::size_mode_exact_width;
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

void litehtml::flex_item_row_direction::align_stretch(flex_line &ln, const containing_block_context &self_size_const,
													  formatting_context *fmt_ctx)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

	set_cross_position(ln.cross_start);
    const css_length& cross_prop = m_container_wm.get_block_size(el->css());
	if (cross_prop.is_predefined())
	{
        pixel_t new_block_size = ln.cross_size - el->content_offset_block(m_container_wm) + el->box_sizing_block(m_container_wm);
        
        // Use current physical dimensions converted to logical inline size
        satoru::logical_size current_content_size = m_container_wm.to_logical(el->pos().width, el->pos().height);
        pixel_t new_inline_size = current_content_size.inline_size + el->box_sizing_inline(m_container_wm);

        containing_block_context::typed_pixel tp_is(new_inline_size, containing_block_context::cbc_value_type_auto);
        containing_block_context::typed_pixel tp_bs(new_block_size, containing_block_context::cbc_value_type_auto);

        containing_block_context cb = m_container_wm.update_logical(self_size, tp_is, tp_bs);
        cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;

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
	return el->inline_size(m_container_wm);
}

litehtml::pixel_t litehtml::flex_item_row_direction::get_el_cross_size()
{
	return el->block_size(m_container_wm);
}

////////////////////////////////////////////////////////////////////////////////////

void litehtml::flex_item_column_direction::direction_specific_init(const litehtml::containing_block_context &self_size_const,
																   litehtml::formatting_context *fmt_ctx)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

    // Map logical margins to auto_margin flags (Main = Block, Cross = Inline)
    const auto& margins = el->css().get_margins();
    if(m_container_wm.block_start(margins).is_predefined()) auto_margin_main_start = 0;
    if(m_container_wm.block_end(margins).is_predefined()) auto_margin_main_end = 0;
    if(m_container_wm.inline_start(margins).is_predefined()) auto_margin_cross_start = true;
    if(m_container_wm.inline_end(margins).is_predefined()) auto_margin_cross_end = true;
	
    // Manual branching for min/max
    const css_length& min_size_prop = m_container_wm.is_vertical() ? el->css().get_min_width() : el->css().get_min_height();
    const css_length& max_size_prop = m_container_wm.is_vertical() ? el->css().get_max_width() : el->css().get_max_height();
    const css_length& size_prop = m_container_wm.get_block_size(el->css()); // Main size
    
    pixel_t render_offset = el->render_offset_height();
    if(m_container_wm.is_vertical()) render_offset = el->render_offset_width();

    pixel_t content_offset = el->content_offset_block(m_container_wm);
	pixel_t parent_size = self_size.render_block_size();

	if (min_size_prop.is_predefined())
	{
		if (el->css().get_overflow() == overflow_visible ||
			el->src_el()->css().get_display() == display_webkit_box)
		{
			formatting_context fmt_ctx_copy;
			if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
			int mode = containing_block_context::size_mode_measure;
			bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
			if (!stretch) mode |= containing_block_context::size_mode_content;
			
            // Reset both? Original logic: 
            // child_cb.width.type = auto; child_cb.height.type = auto;
            // child_cb.size_mode &= ~exact; size_mode |= mode;
            containing_block_context measure_cb = m_container_wm.update_logical(self_size, std::nullopt, std::nullopt); 
            measure_cb.width.type = containing_block_context::cbc_value_type_auto;
            measure_cb.height.type = containing_block_context::cbc_value_type_auto;
            measure_cb.size_mode &= ~(containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height);
            measure_cb.size_mode |= mode;

			el->measure(measure_cb, fmt_ctx ? &fmt_ctx_copy : nullptr);
			min_main_size = get_el_main_size();
		} else
		{
			min_main_size = render_offset;
		}
	} else
	{
		min_main_size = min_size_prop.calc_percent(parent_size) + render_offset - el->box_sizing_block(m_container_wm);
	}
	if (!max_size_prop.is_predefined())
	{
		max_main_size = max_size_prop.calc_percent(parent_size) + render_offset - el->box_sizing_block(m_container_wm);
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
				flex_base_size = size_prop.calc_percent(parent_size) + render_offset - el->box_sizing_block(m_container_wm);
				break;
			case flex_basis_max_content:
			case flex_basis_fit_content:
				{
					containing_block_context measure_size = self_size;
                    measure_size.width.type = containing_block_context::cbc_value_type_auto;
                    measure_size.height.type = containing_block_context::cbc_value_type_auto;
                    measure_size.size_mode &= ~(containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height);
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
		flex_base_size = el->css().get_flex_basis().calc_percent(parent_size) + render_offset - el->box_sizing_block(m_container_wm);
	}

	scaled_flex_shrink_factor = (flex_base_size - render_offset) * shrink;
}

void litehtml::flex_item_column_direction::apply_main_auto_margins()
{
    if(!auto_margin_main_start.is_default()) m_container_wm.set_block_start(el->get_margins(), auto_margin_main_start);
    if(!auto_margin_main_end.is_default()) m_container_wm.set_block_end(el->get_margins(), auto_margin_main_end);
}

bool litehtml::flex_item_column_direction::apply_cross_auto_margins(pixel_t cross_size)
{
	if(auto_margin_cross_end || auto_margin_cross_start)
	{
		int margins_num = 0;
		if(auto_margin_cross_end) margins_num++;
		if(auto_margin_cross_start) margins_num++;
		pixel_t margin = (cross_size - get_el_cross_size()) / margins_num;

        if(auto_margin_cross_start) m_container_wm.set_inline_start(el->get_margins(), margin);
        if(auto_margin_cross_end) m_container_wm.set_inline_end(el->get_margins(), margin);

		return true;
	}
	return false;
}

void litehtml::flex_item_column_direction::set_main_position(pixel_t pos)
{
    main_pos = pos;
}

void litehtml::flex_item_column_direction::set_cross_position(pixel_t pos)
{
    cross_pos = pos;
}

void litehtml::flex_item_column_direction::layout_item(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size_const,
													   litehtml::formatting_context *fmt_ctx)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

	containing_block_context child_cb = self_size;
	child_cb.mode = m_container_wm.mode();
    // Main Axis = Block
    child_cb.block_size() = main_size - el->content_offset_block(m_container_wm) + el->box_sizing_block(m_container_wm);
    child_cb.render_block_size() = child_cb.block_size();

	bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
	const css_length& cross_prop = m_container_wm.get_inline_size(el->css());

	if (stretch && cross_prop.is_predefined())
	{
        // Cross Axis = Inline
        child_cb.inline_size() = ln.cross_size - el->content_offset_inline(m_container_wm) + el->box_sizing_inline(m_container_wm);
        child_cb.render_inline_size() = child_cb.inline_size();
		child_cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;
	} else {
        child_cb.width.type = containing_block_context::cbc_value_type_auto;
        child_cb.height.type = containing_block_context::cbc_value_type_auto;
        child_cb.size_mode &= ~(containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height);

		if (m_container_wm.is_vertical())
		{
            child_cb.width = main_size - el->content_offset_width() + el->box_sizing_width();
            child_cb.render_width = child_cb.width;
			child_cb.size_mode |= containing_block_context::size_mode_exact_width;
		} else
		{
            child_cb.height = main_size - el->content_offset_height() + el->box_sizing_height();
            child_cb.render_height = child_cb.height;
			child_cb.size_mode |= containing_block_context::size_mode_exact_height;
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

void litehtml::flex_item_column_direction::align_stretch(flex_line &ln, const containing_block_context &self_size_const,
													  formatting_context *fmt_ctx)
{
	containing_block_context self_size = self_size_const;
	self_size.width = self_size.render_width;
	self_size.height = self_size.render_height;

	set_cross_position(ln.cross_start);
	const css_length& cross_prop = m_container_wm.get_inline_size(el->css());
	if (cross_prop.is_predefined())
	{
        pixel_t new_inline_size = ln.cross_size - el->content_offset_inline(m_container_wm) + el->box_sizing_inline(m_container_wm);
        
        // Use current physical dimensions converted to logical block size
        satoru::logical_size current_content_size = m_container_wm.to_logical(el->pos().width, el->pos().height);
        pixel_t new_block_size = current_content_size.block_size + el->box_sizing_block(m_container_wm);

        containing_block_context::typed_pixel tp_is(new_inline_size, containing_block_context::cbc_value_type_auto);
        containing_block_context::typed_pixel tp_bs(new_block_size, containing_block_context::cbc_value_type_auto);

        containing_block_context cb = m_container_wm.update_logical(self_size, tp_is, tp_bs);
        cb.size_mode = containing_block_context::size_mode_exact_width | containing_block_context::size_mode_exact_height;

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
	return el->block_size(m_container_wm);
}

litehtml::pixel_t litehtml::flex_item_column_direction::get_el_cross_size()
{
	return el->inline_size(m_container_wm);
}
