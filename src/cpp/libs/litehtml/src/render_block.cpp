#include "render_block.h"
#include "render_inline_context.h"
#include "render_block_context.h"
#include "document.h"
#include "document_container.h"
#include "html_tag.h"
#include "types.h"

litehtml::pixel_t litehtml::render_item_block::place_float(const std::shared_ptr<render_item> &el, pixel_t block_offset, const containing_block_context &self_size, formatting_context* fmt_ctx)
{
    pixel_t line_block_start = fmt_ctx->get_cleared_top(el, block_offset);
    pixel_t line_inline_start = 0;
    pixel_t line_inline_end   = self_size.render_inline_size();
	fmt_ctx->get_line_left_right(line_block_start, self_size.render_inline_size(), line_inline_start, line_inline_end);

    pixel_t ret_inline_size = 0;

	pixel_t min_rendered_inline_size = el->measure(self_size.new_inline_size(line_inline_end, self_size.size_mode & containing_block_context::size_mode_content), fmt_ctx);
	if (!(self_size.size_mode & containing_block_context::size_mode_measure))
	{
		el->place_logical(line_inline_start, line_block_start, self_size.new_inline_size(line_inline_end, self_size.size_mode & containing_block_context::size_mode_content), fmt_ctx);
	}

	if(min_rendered_inline_size < el->inline_size() && el->src_el()->css().get_width().is_predefined())
	{
		min_rendered_inline_size = el->measure(self_size.new_inline_size(min_rendered_inline_size, self_size.size_mode & containing_block_context::size_mode_content), fmt_ctx);
		if (!(self_size.size_mode & containing_block_context::size_mode_measure))
		{
			el->place_logical(line_inline_start, line_block_start, self_size.new_inline_size(min_rendered_inline_size, self_size.size_mode & containing_block_context::size_mode_content), fmt_ctx);
		}
	}

    if (el->src_el()->css().get_float() == float_left)
    {
        if(el->inline_end_pos() > line_inline_end)
        {
			line_block_start = fmt_ctx->find_next_line_top(el->block_start_pos(), el->inline_size(), self_size.render_inline_size());
			if (!(self_size.size_mode & containing_block_context::size_mode_measure))
			{
				el->place_logical(fmt_ctx->get_line_left(line_block_start), line_block_start, self_size.new_inline_size(line_inline_end, self_size.size_mode & containing_block_context::size_mode_content), fmt_ctx);
			}
        }
		fmt_ctx->add_float(el, min_rendered_inline_size, self_size.context_idx);
		fix_line_width(float_left, self_size, fmt_ctx);

		ret_inline_size = fmt_ctx->find_min_left(line_block_start, self_size.context_idx);
    } else if (el->src_el()->css().get_float() == float_right)
    {
        if(line_inline_start + el->inline_size() > line_inline_end)
        {
            pixel_t new_block_start = fmt_ctx->find_next_line_top(el->block_start_pos(), el->inline_size(), self_size.render_inline_size());
			if (!(self_size.size_mode & containing_block_context::size_mode_measure))
			{
				el->place_logical(fmt_ctx->get_line_right(new_block_start, self_size.render_inline_size()) - el->inline_size(), new_block_start, self_size.new_inline_size(self_size.render_inline_size(), self_size.size_mode & containing_block_context::size_mode_content), fmt_ctx);
			}
        } else
        {
			if (!(self_size.size_mode & containing_block_context::size_mode_measure))
			{
				el->place_logical(line_inline_end - el->inline_size(), line_block_start, self_size.new_inline_size(self_size.render_inline_size(), self_size.size_mode & containing_block_context::size_mode_content), fmt_ctx);
			}
        }
		fmt_ctx->add_float(el, min_rendered_inline_size, self_size.context_idx);
		fix_line_width(float_right, self_size, fmt_ctx);
		line_inline_end = fmt_ctx->find_min_right(line_block_start, self_size.render_inline_size(), self_size.context_idx);
		ret_inline_size = self_size.render_inline_size() - line_inline_end;
    }
    return ret_inline_size;
}

std::shared_ptr<litehtml::render_item> litehtml::render_item_block::init()
{
    std::shared_ptr<render_item> ret;

    // Initialize indexes for list items
    if(src_el()->css().get_display() == display_list_item && src_el()->css().get_list_style_type() >= list_style_type_armenian)
    {
        if (auto p = src_el()->parent())
        {
            int val = atoi(p->get_attr("start", "1"));
			for(const auto &child : p->children())
            {
                if (child == src_el())
                {
                    src_el()->set_attr("list_index", std::to_string(val).c_str());
                    break;
                }
                else if (child->css().get_display() == display_list_item)
                    val++;
            }
        }
    }
    // Split inline blocks with box blocks inside
    auto iter = m_children.begin();
    while (iter != m_children.end())
    {
        const auto& el = *iter;
        if(el->src_el()->css().get_display() == display_inline && !el->children().empty())
        {
            auto split_el = el->split_inlines();
            if(std::get<0>(split_el))
            {
                iter = m_children.erase(iter);
                iter = m_children.insert(iter, std::get<2>(split_el));
                iter = m_children.insert(iter, std::get<1>(split_el));
                iter = m_children.insert(iter, std::get<0>(split_el));

                std::get<0>(split_el)->parent(shared_from_this());
                std::get<1>(split_el)->parent(shared_from_this());
                std::get<2>(split_el)->parent(shared_from_this());
                continue;
            }
        }
        ++iter;
    }

    bool has_block_level = false;
	bool has_inlines = false;
    for (const auto& el : m_children)
    {
	if(!el->src_el()->is_float())
		{
			if (el->src_el()->is_block_box())
			{
				has_block_level = true;
			} else if (el->src_el()->is_inline())
			{
				has_inlines = true;
			}
		}
        if(has_block_level && has_inlines)
            break;
    }
    if(has_block_level)
    {
        ret = std::make_shared<render_item_block_context>(src_el());
        ret->parent(parent());

        auto doc = src_el()->get_document();
        decltype(m_children) new_children;
        decltype(m_children) inlines;
        bool not_ws_added = false;
        for (const auto& el : m_children)
        {
            if(el->src_el()->is_inline())
            {
                inlines.push_back(el);
                if(!el->src_el()->is_white_space())
                    not_ws_added = true;
            } else
            {
                if(not_ws_added)
                {
                    auto anon_el = std::make_shared<html_tag>(src_el());
                    auto anon_ri = std::make_shared<render_item_block>(anon_el);
                    for(const auto& inl : inlines)
                    {
                        anon_ri->add_child(inl);
                    }

                    not_ws_added = false;
                    new_children.push_back(anon_ri);
                    anon_ri->parent(ret);
                }
                new_children.push_back(el);
                el->parent(ret);
                inlines.clear();
            }
        }
        if(!inlines.empty() && not_ws_added)
        {
            auto anon_el = std::make_shared<html_tag>(src_el());
            auto anon_ri = std::make_shared<render_item_block>(anon_el);
            for(const auto& inl : inlines)
            {
                anon_ri->add_child(inl);
            }

            new_children.push_back(anon_ri);
            anon_ri->parent(ret);
        }
        ret->children() = new_children;
    }

    if(!ret)
    {
        ret = std::make_shared<render_item_inline_context>(src_el());
        ret->parent(parent());
        ret->children() = children();
        for (const auto &el: ret->children())
        {
            el->parent(ret);
        }
    }

    ret->src_el()->add_render(ret);

    for(auto& el : ret->children())
    {
        el = el->init();
    }

    return ret;
}



litehtml::pixel_t litehtml::render_item_block::_measure(const containing_block_context &containing_block_size, formatting_context* fmt_ctx)
{
	containing_block_context self_size = m_self_size;
	
	// If the block has max-inline-size, we should restrict the available inline-size for children
	if (self_size.max_inline_size().type != containing_block_context::cbc_value_type_none)
	{
		if (self_size.render_inline_size() > self_size.max_inline_size())
		{
			self_size.render_inline_size() = self_size.max_inline_size();
			self_size.inline_size() = self_size.render_inline_size();
		}
	}

	// For measurement, we pass size_mode_measure
	containing_block_context measure_size = self_size;
	measure_size.size_mode |= containing_block_context::size_mode_measure;

	pixel_t ret_inline_size = _render_content(0, 0, false, measure_size, fmt_ctx);

	if (src_el()->css().get_display() == display_list_item)
	{
		if(m_pos.height == 0)
		{
			m_pos.height = css().line_height().computed_value;
		}
	}

	// Set block inline-size (and physical width/height)
	if(!(containing_block_size.size_mode & containing_block_context::size_mode_content))
	{
		if(self_size.render_inline_size().type == containing_block_context::cbc_value_type_absolute)
		{
			ret_inline_size = self_size.render_inline_size();
		}
        if (self_size.mode == writing_mode_horizontal_tb)
        {
            m_pos.width = self_size.render_width;
        }
        else
        {
            // Vertical blocks should fit content height unless height is explicit
            m_pos.height = ret_inline_size;
        }
	} else
	{
		pixel_t calculated_size = (self_size.inline_size().type != containing_block_context::cbc_value_type_auto) ? (pixel_t)self_size.render_inline_size() : ret_inline_size;

		if (containing_block_size.size_mode & containing_block_context::size_mode_exact_width)
		{
			calculated_size = self_size.render_inline_size();
		}
		if(self_size.render_inline_size().type == containing_block_context::cbc_value_type_absolute)
		{
			ret_inline_size = self_size.render_inline_size();
		}
        
        if (self_size.mode == writing_mode_horizontal_tb)
            m_pos.width = calculated_size;
        else
            m_pos.height = calculated_size;
	}

	// Fix inline-size with max-inline-size attribute
	if(self_size.max_inline_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_inline_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.width : m_pos.height;
		if(physical_inline_size > self_size.max_inline_size())
		{
			physical_inline_size = self_size.max_inline_size();
		}
	}
	
	// The returned inline-size should not exceed the element's actual inline-size
    pixel_t current_inline_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.width : m_pos.height;
	ret_inline_size = std::min(ret_inline_size, current_inline_size);

	// Fix inline-size with min-inline-size attribute
	if(self_size.min_inline_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_inline_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.width : m_pos.height;
		if(physical_inline_size < self_size.min_inline_size())
		{
			physical_inline_size = self_size.min_inline_size();
		}
	} else {
        pixel_t& physical_inline_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.width : m_pos.height;
        if(physical_inline_size < 0) physical_inline_size = 0;
    }

	// Set block-size
	if (self_size.render_block_size().type != containing_block_context::cbc_value_type_auto &&
	    (!(containing_block_size.size_mode & containing_block_context::size_mode_content) ||
         src_el()->css().get_display() == display_table_cell))
	{
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if (src_el()->css().get_display() == display_table_cell)
		{
			physical_block_size = std::max(physical_block_size, (pixel_t)self_size.render_block_size());
		}
		else
		{
			physical_block_size = self_size.render_block_size();
		}
	} else if(!css().get_aspect_ratio().is_auto())
	{
		aspect_ratio ar = css().get_aspect_ratio();
        if (self_size.mode == writing_mode_horizontal_tb)
    		m_pos.height = m_pos.width * ar.height / ar.width;
        else
            m_pos.width = m_pos.height * ar.width / ar.height;
	} else if (src_el()->is_block_formatting_context())
	{
		// add the floats' height to the block height
		pixel_t floats_block_size = fmt_ctx->get_floats_height();
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if (floats_block_size > physical_block_size)
		{
			physical_block_size = floats_block_size;
		}
	}
	if(containing_block_size.size_mode & containing_block_context::size_mode_content)
	{
		if(self_size.render_block_size().type == containing_block_context::cbc_value_type_absolute)
		{
            pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
			if(physical_block_size > self_size.render_block_size())
			{
				physical_block_size = self_size.render_block_size();
			}
		}
	}

	// Fix block-size with min-block-size attribute
	if(self_size.min_block_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if(physical_block_size < self_size.min_block_size())
		{
			physical_block_size = self_size.min_block_size();
		}
	} else {
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
        if(physical_block_size < 0) physical_block_size = 0;
    }

	// Fix block-size with max-block-size attribute
	if(self_size.max_block_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if(physical_block_size > self_size.max_block_size())
		{
			physical_block_size = self_size.max_block_size();
		}
	}

    if (src_el()->css().get_display() == display_list_item)
    {
        string list_image = src_el()->css().get_list_style_image();
        if (!list_image.empty())
        {
            size sz;
            string list_image_baseurl = src_el()->css().get_list_style_image_baseurl();
            src_el()->get_document()->container()->get_image_size(list_image.c_str(), list_image_baseurl.c_str(), sz);
            pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
            if (physical_block_size < sz.height)
            {
			    physical_block_size = sz.height;
            }
        }
    }

	return ret_inline_size + content_offset_inline(get_wm_context());
}

void litehtml::render_item_block::_place(pixel_t inline_pos, pixel_t block_pos, const containing_block_context &containing_block_size, formatting_context* fmt_ctx)
{
	containing_block_context self_size = m_self_size;
	
	// If the block has max-inline-size, we should restrict the available inline-size for children
	if (self_size.max_inline_size().type != containing_block_context::cbc_value_type_none)
	{
		if (self_size.render_inline_size() > self_size.max_inline_size())
		{
			self_size.render_inline_size() = self_size.max_inline_size();
			self_size.inline_size() = self_size.render_inline_size();
		}
	}

	_render_content(inline_pos, block_pos, false, self_size, fmt_ctx);

	// Re-apply constraints after _render_content because it might have overwritten m_pos.width/height
	
	// Set block inline-size
	if(!(containing_block_size.size_mode & containing_block_context::size_mode_content))
	{
        if (self_size.mode == writing_mode_horizontal_tb)
            m_pos.width = self_size.render_width;
        else
            m_pos.height = m_pos.height; // Keep height from _render_content
	} else
	{
        pixel_t& physical_inline_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.width : m_pos.height;
		if (self_size.inline_size().type != containing_block_context::cbc_value_type_auto)
		{
			physical_inline_size = self_size.render_inline_size();
		}
		if (containing_block_size.size_mode & containing_block_context::size_mode_exact_width)
		{
			physical_inline_size = self_size.render_inline_size();
		}
	}

	// Fix inline-size with max-inline-size attribute
	if(self_size.max_inline_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_inline_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.width : m_pos.height;
		if(physical_inline_size > self_size.max_inline_size())
		{
			physical_inline_size = self_size.max_inline_size();
		}
	}
	
	// Fix inline-size with min-inline-size attribute
	if(self_size.min_inline_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_inline_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.width : m_pos.height;
		if(physical_inline_size < self_size.min_inline_size())
		{
			physical_inline_size = self_size.min_inline_size();
		}
	}

	// Set block-size
	if (self_size.render_block_size().type != containing_block_context::cbc_value_type_auto &&
	    (!(containing_block_size.size_mode & containing_block_context::size_mode_content) ||
         src_el()->css().get_display() == display_table_cell))
	{
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if (src_el()->css().get_display() == display_table_cell)
		{
			physical_block_size = std::max(physical_block_size, (pixel_t)self_size.render_block_size());
		}
		else
		{
			physical_block_size = self_size.render_block_size();
		}
	} else if(!css().get_aspect_ratio().is_auto())
	{
		aspect_ratio ar = css().get_aspect_ratio();
        if (self_size.mode == writing_mode_horizontal_tb)
    		m_pos.height = m_pos.width * ar.height / ar.width;
        else
            m_pos.width = m_pos.height * ar.width / ar.height;
	} else if (src_el()->is_block_formatting_context())
	{
		pixel_t floats_block_size = fmt_ctx->get_floats_height();
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if (floats_block_size > physical_block_size)
		{
			physical_block_size = floats_block_size;
		}
	}

	// Fix block-size with min-block-size attribute
	if(self_size.min_block_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if(physical_block_size < self_size.min_block_size())
		{
			physical_block_size = self_size.min_block_size();
		}
	}

	// Fix block-size with max-block-size attribute
	if(self_size.max_block_size().type != containing_block_context::cbc_value_type_none)
	{
        pixel_t& physical_block_size = (self_size.mode == writing_mode_horizontal_tb) ? m_pos.height : m_pos.width;
		if(physical_block_size > self_size.max_block_size())
		{
			physical_block_size = self_size.max_block_size();
		}
	}
}

void litehtml::render_item_block::apply_vertical_align()
{
    pixel_t content_block_end = 0;
    satoru::WritingModeContext wm = get_wm_context();
    for (const auto& el : m_children)
    {
        if (el->src_el()->css().get_display() != display_none)
        {
            content_block_end = std::max(content_block_end, el->block_end_pos(wm));
        }
    }

    pixel_t shift = 0;
    vertical_align va = src_el()->css().get_vertical_align();
    if (va == va_middle)
    {
        shift = (m_pos.height - content_block_end) / 2;
    }
    else if (va == va_bottom)
    {
        shift = m_pos.height - content_block_end;
    }

    if (shift > 0)
    {
        for (auto& el : m_children)
        {
            el->block_shift(shift);
        }
    }
}

litehtml::pixel_t litehtml::render_item_block::get_first_baseline()
{
    for (const auto& el : m_children)
    {
        if (el->src_el()->css().get_display() != display_none &&
            el->src_el()->css().get_float() == float_none &&
            !el->src_el()->is_positioned())
        {
            return el->pos().y + el->get_first_baseline();
        }
    }
    return render_item::get_first_baseline();
}

litehtml::pixel_t litehtml::render_item_block::get_last_baseline()
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        const auto& el = *it;
        if (el->src_el()->css().get_display() != display_none &&
            el->src_el()->css().get_float() == float_none &&
            !el->src_el()->is_positioned())
        {
            return el->pos().y + el->get_last_baseline();
        }
    }
    return render_item::get_last_baseline();
}
