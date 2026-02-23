#include <cmath>
#include <cstdio>
#include "flex_item.h"
#include "flex_line.h"
#include "types.h"

void litehtml::flex_item::init(const litehtml::containing_block_context &self_size,
							   litehtml::formatting_context *fmt_ctx, flex_align_items align_items)
{
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

	if (base_size < min_size)
	{
		main_size = min_size;
	} else if (!max_size.is_default() && base_size > max_size)
	{
		main_size = max_size;
	} else
	{
		main_size = base_size;
	}

	frozen = false;
}

void litehtml::flex_item::place(flex_line &ln, pixel_t main_pos,
								const containing_block_context &self_size,
								formatting_context *fmt_ctx)
{
	perform_render(ln, self_size, fmt_ctx);
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
	if(el->css().get_margins().left.is_predefined())
	{
		auto_margin_main_start = 0;
	}
	if(el->css().get_margins().right.is_predefined())
	{
		auto_margin_main_end = 0;
	}
	if(el->css().get_margins().top.is_predefined())
	{
		auto_margin_cross_start = true;
	}
	if(el->css().get_margins().bottom.is_predefined())
	{
		auto_margin_cross_end = true;
	}
	
	def_value<pixel_t> content_size(0);
	if (el->css().get_min_width().is_predefined())
	{
		formatting_context fmt_ctx_copy;
		if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
		min_size = el->render(0, 0,
							  self_size.new_width(el->content_offset_width(),
												  containing_block_context::size_mode_content | containing_block_context::size_mode_measure),
							  fmt_ctx ? &fmt_ctx_copy : nullptr);
		content_size = min_size;
	} else
	{
		min_size = el->css().get_min_width().calc_percent(self_size.render_width) +
				   el->render_offset_width();
	}
	if (!el->css().get_max_width().is_predefined())
	{
		max_size = el->css().get_max_width().calc_percent(self_size.render_width) +
				   el->render_offset_width();
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
		if(predef == flex_basis_auto && el->css().get_width().is_predefined())
		{
			// if width is not predefined, use content size as base size
			predef = flex_basis_content;
		}

		switch (predef)
		{
			case flex_basis_auto:
				base_size = el->css().get_width().calc_percent(self_size.render_width) +
							el->render_offset_width();
				break;
			case flex_basis_fit_content:
			case flex_basis_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					base_size = el->render(0, 0, self_size.new_width(self_size.render_width + el->content_offset_width(),
																	 containing_block_context::size_mode_measure),
										   fmt_ctx ? &fmt_ctx_copy : nullptr);
				}
				break;
			case flex_basis_min_content:
				if(content_size.is_default())
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					content_size = el->render(0, 0,
											  self_size.new_width(el->content_offset_width(),
																  containing_block_context::size_mode_content | containing_block_context::size_mode_measure),
											  fmt_ctx ? &fmt_ctx_copy : nullptr);
				}
				base_size = content_size;
				break;
			case flex_basis_max_content:
				{
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					el->render(0, 0, self_size.new_width(0, containing_block_context::size_mode_measure), fmt_ctx ? &fmt_ctx_copy : nullptr);
					base_size = el->width();
				}
				break;
			default:
				base_size = 0;
				break;
		}
	} else
	{
		base_size = el->css().get_flex_basis().calc_percent(self_size.render_width) +
					el->render_offset_width();
	}

	scaled_flex_shrink_factor = (base_size - el->render_offset_width()) * shrink;
}

void litehtml::flex_item_row_direction::apply_main_auto_margins()
{
	// apply auto margins to item
	if(!auto_margin_main_start.is_default())
	{
		el->get_margins().left = auto_margin_main_start;
		el->pos().x += auto_margin_main_start;
	}
	if(!auto_margin_main_end.is_default()) el->get_margins().right = auto_margin_main_end;
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
		pixel_t margin = (cross_size - el->height()) / margins_num;
		if(auto_margin_cross_start)
		{
			el->get_margins().top = margin;
			el->pos().y = el->content_offset_top();
		}
		if(auto_margin_cross_end)
		{
			el->get_margins().bottom = margin;
		}
		return true;
	}
	return false;
}

void litehtml::flex_item_row_direction::set_main_position(pixel_t pos)
{
	el->pos().x = pos + el->content_offset_left();
}

void litehtml::flex_item_row_direction::set_cross_position(pixel_t pos)
{
	el->pos().y = pos + el->content_offset_top();
}

void litehtml::flex_item_row_direction::perform_render(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size,
													   litehtml::formatting_context *fmt_ctx)
{
	// Create a new containing block context with the calculated width/height
	// We cannot use self_size.new_width_height() because it preserves the diff from the parent.
	containing_block_context child_cb = self_size;
	child_cb.width = main_size - el->content_offset_width() + el->box_sizing_width();
	child_cb.render_width = child_cb.width;
	child_cb.height = self_size.height; // keep height auto or percent
	child_cb.render_height = self_size.render_height; // This might be incorrect if diff matters for height? 
	// For height, if self_size.height is auto, child_cb.height should be auto. 
	// If self_size.height is absolute, child_cb.height should probably strip diff?
	// But let's assume height diff handling in new_width_height was desired? 
	// new_width_height: ret.height = h + diff.
	// If we want exact height? No, here height is passed as self_size.height (which might include diff).
	// If self_size.height is auto, diff is irrelevant.
	// If self_size.height is absolute, it includes padding of parent.
	// But we are setting child's constraint.
	// child_cb.height should be the "available height" for the child.
	// If parent has height, child sees that height.
	// But self_size represents Parent's Content Box usually.
	// So self_size.height IS the available height.
	// So we should set child_cb.height = self_size.render_height (Available Content Height)?
	// If we use self_size.height (Outer Height of Parent?), that's wrong.
	// self_size.height usually means "Height of Containing Block".
	// For a child of a flex item, the containing block is the flex item content box.
	// So self_size.width should be flex_item_content_width.
	// But `self_size` passed to `place` comes from `render_flex`.
	// `render_flex` `self_size` is Header's CBC.
	// Header's CBC: width=800, render_width=784.
	// So `self_size.height` = Header's Outer Height? No, Header's containing block height.
	// `self_size` is calculated relative to Header's parent (`div.flex`).
	// So `self_size.height` is `div.flex` content height?
	// `calculate_cbc`: `ret.height.value = cb_context.height - content_offset`.
	// So `self_size.height` is Header's Content Height (Available space).
	// `self_size.render_height` is `self_size.height - box_sizing_width`.
	// Wait, `calculate_cbc` sets `ret.height`.
	// If `header` is `border-box`.
	// `ret.height` = `cb.height` (800) if fixed.
	// `ret.render_height` = `800 - 16 = 784`.
	// So `self_size.height` = 800. `self_size.render_height` = 784.
	// The available height for child is 784.
	// So we should pass 784 as `height`.
	// But we also want `render_height` to be 784?
	// `new_width_height` preserves diff. `diff` = 16.
	// If we pass `h = self_size.height` (800).
	// `ret.render_height` = 800.
	// `ret.height` = 816.
	// This seems wrong. We want child to see 784 as available height.
	// So `child_cb.height` should be `self_size.render_height` (784).
	// And `child_cb.render_height` = 784.
	// `diff` = 0.
	
	// But wait, `self_size.height` might be `auto`.
	if(self_size.height.type == containing_block_context::cbc_value_type_auto)
	{
		child_cb.height.type = containing_block_context::cbc_value_type_auto;
		child_cb.render_height.type = containing_block_context::cbc_value_type_auto;
	} else {
		child_cb.height = self_size.render_height;
		child_cb.render_height = self_size.render_height;
	}

	child_cb.size_mode = containing_block_context::size_mode_exact_width;

	bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
	if (!stretch && el->css().get_height().is_predefined())
	{
		child_cb.size_mode |= containing_block_context::size_mode_content;
	}

	if (el->css().get_height().is_predefined())
	{
		// If height is auto - render with content size
		if (self_size.size_mode & containing_block_context::size_mode_measure)
		{
			el->measure(child_cb, fmt_ctx);
		} else
		{
			el->measure(child_cb, fmt_ctx);
			el->place(el->left(), el->top(), child_cb, fmt_ctx);
		}
	} else
	{
		if (self_size.size_mode & containing_block_context::size_mode_measure)
		{
			el->measure(child_cb, fmt_ctx);
		} else
		{
			el->measure(child_cb, fmt_ctx);
			el->place(el->left(), el->top(), child_cb, fmt_ctx);
		}
	}
}

void litehtml::flex_item_row_direction::align_stretch(flex_line &ln, const containing_block_context &self_size,
													  formatting_context *fmt_ctx)
{
	set_cross_position(ln.cross_start);
	if (el->css().get_height().is_predefined())
	{
		auto cb = self_size.new_width_height(
				el->pos().width + el->box_sizing_width(),
				ln.cross_size - el->content_offset_height() + el->box_sizing_height(),
				containing_block_context::size_mode_exact_width |
				containing_block_context::size_mode_exact_height
				);
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
	return el->width();
}

litehtml::pixel_t litehtml::flex_item_row_direction::get_el_cross_size()
{
	return el->height();
}

////////////////////////////////////////////////////////////////////////////////////

void litehtml::flex_item_column_direction::direction_specific_init(const litehtml::containing_block_context &self_size,
																   litehtml::formatting_context *fmt_ctx)
{
	if(el->css().get_margins().top.is_predefined())
	{
		auto_margin_main_start = 0;
	}
	if(el->css().get_margins().bottom.is_predefined())
	{
		auto_margin_main_end = 0;
	}
	if(el->css().get_margins().left.is_predefined())
	{
		auto_margin_cross_start = true;
	}
	if(el->css().get_margins().right.is_predefined())
	{
		auto_margin_cross_end = true;
	}
	
	if (el->css().get_min_height().is_predefined())
	{
		formatting_context fmt_ctx_copy;
		if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
		int mode = containing_block_context::size_mode_measure;
		bool stretch = (align & 0xFF) == flex_align_items_stretch || (align & 0xFF) == flex_align_items_normal;
		
		// When measuring min-height, we must respect the available width to allow correct text wrapping.
		// Use self_size.render_width as the constraint.
		el->render(0, 0, self_size.new_width(self_size.render_width, mode), fmt_ctx ? &fmt_ctx_copy : nullptr);
		min_size = el->height();
	} else
	{
		min_size = el->css().get_min_height().calc_percent(self_size.height) +
				   el->render_offset_height();
	}
	if (!el->css().get_max_height().is_predefined())
	{
		max_size = el->css().get_max_height().calc_percent(self_size.height) +
				   el->render_offset_height();
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
		if(predef == flex_basis_auto && el->css().get_height().is_predefined())
		{
			predef = flex_basis_fit_content;
		}
		switch (predef)
		{
			case flex_basis_auto:
				base_size = el->css().get_height().calc_percent(self_size.height) +
							el->render_offset_height();
				break;
			case flex_basis_max_content:
			case flex_basis_fit_content:
				{
					containing_block_context measure_size = self_size;
					measure_size.height.type = containing_block_context::cbc_value_type_auto;
					measure_size.render_height.type = containing_block_context::cbc_value_type_auto;
					measure_size.size_mode &= ~containing_block_context::size_mode_exact_height;
					measure_size.size_mode |= containing_block_context::size_mode_measure;
					formatting_context fmt_ctx_copy;
					if(fmt_ctx) fmt_ctx_copy = *fmt_ctx;
					el->render(0, 0, measure_size, fmt_ctx ? &fmt_ctx_copy : nullptr);
					base_size = el->height();
				}
				break;
			case flex_basis_min_content:
				base_size = min_size;
				break;
			default:
				base_size = 0;
		}
	} else
	{
		base_size = el->css().get_flex_basis().calc_percent(self_size.height) +
					el->render_offset_height();
	}

	scaled_flex_shrink_factor = (base_size - el->render_offset_height()) * shrink;
}

void litehtml::flex_item_column_direction::apply_main_auto_margins()
{
	// apply auto margins to item
	if(!auto_margin_main_start.is_default())
	{
		el->get_margins().top = auto_margin_main_start;
		el->pos().y += auto_margin_main_start;
	}
	if(!auto_margin_main_end.is_default()) el->get_margins().bottom = auto_margin_main_end;
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
		pixel_t margin = (cross_size - el->width()) / margins_num;
		if(auto_margin_cross_start)
		{
			el->get_margins().left = margin;
			el->pos().x = el->content_offset_left();
		}
		if(auto_margin_cross_end)
		{
			el->get_margins().right = margin;
		}
		return true;
	}
	return false;
}

void litehtml::flex_item_column_direction::set_main_position(pixel_t pos)
{
	el->pos().y = pos + el->content_offset_top();
}

void litehtml::flex_item_column_direction::set_cross_position(pixel_t pos)
{
	el->pos().x = pos + el->content_offset_left();
}

void litehtml::flex_item_column_direction::perform_render(litehtml::flex_line &ln,
													   const litehtml::containing_block_context &self_size,
													   litehtml::formatting_context *fmt_ctx)
{
	int mode = containing_block_context::size_mode_exact_height;
	bool stretch = (align & 0xFF) == flex_align_items_stretch;
	if (!stretch && el->css().get_width().is_predefined())
	{
		mode |= containing_block_context::size_mode_content;
	}
	auto cb = self_size.new_width_height(
			stretch ? self_size.width : self_size.render_width,
			main_size - el->content_offset_height() + el->box_sizing_height(),
			mode
	);
	if (self_size.size_mode & containing_block_context::size_mode_measure)
	{
		el->measure(cb, fmt_ctx);
	} else
	{
		el->measure(cb, fmt_ctx);
		el->place(el->left(), el->top(), cb, fmt_ctx);
	}
}

void litehtml::flex_item_column_direction::align_stretch(flex_line &ln, const containing_block_context &self_size,
													  formatting_context *fmt_ctx)
{
	set_cross_position(ln.cross_start);
	if (el->css().get_width().is_predefined())
	{
		auto cb = self_size.new_width_height(
				ln.cross_size - el->content_offset_width() + el->box_sizing_width(),
				el->pos().height + el->box_sizing_height(),
				containing_block_context::size_mode_exact_width |
				containing_block_context::size_mode_exact_height
		);
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
	return el->height();
}

litehtml::pixel_t litehtml::flex_item_column_direction::get_el_cross_size()
{
	return el->width();
}
