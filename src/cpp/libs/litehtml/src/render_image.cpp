#include "render_image.h"
#include "document.h"
#include "types.h"
#include <cstdio>

litehtml::pixel_t litehtml::render_item_image::_measure(const containing_block_context &containing_block_size, formatting_context* /*fmt_ctx*/)
{
	containing_block_context self_size = m_self_size;

    litehtml::size sz;
    src_el()->get_content_size(sz, containing_block_size.width);

    m_pos.width = 0;
    m_pos.height = 0;

	bool is_content_size = (containing_block_size.size_mode & containing_block_context::size_mode_content) != 0;

    if(self_size.width.type != containing_block_context::cbc_value_type_auto && !is_content_size)
    {
        m_pos.width = self_size.render_width;
    }
    if(self_size.height.type != containing_block_context::cbc_value_type_auto && !is_content_size)
    {
        m_pos.height = self_size.render_height;
    }

    if(m_pos.width == 0 && m_pos.height == 0)
    {
        m_pos.width = sz.width;
        m_pos.height = sz.height;
    } else if(m_pos.width == 0 && sz.height != 0)
    {
        m_pos.width = m_pos.height * sz.width / sz.height;
    } else if(m_pos.height == 0 && sz.width != 0)
    {
        m_pos.height = m_pos.width * sz.height / sz.width;
    }

    // Constraints
    if(!css().get_max_width().is_predefined())
    {
        pixel_t max_width = src_el()->get_document()->to_pixels(css().get_max_width(), css().get_font_metrics(), containing_block_size.width);
        if(m_pos.width > max_width)
        {
            m_pos.width = max_width;
            if(sz.width != 0) m_pos.height = m_pos.width * sz.height / sz.width;
        }
    }
    if(!css().get_max_height().is_predefined())
    {
        pixel_t max_height = src_el()->get_document()->to_pixels(css().get_max_height(), css().get_font_metrics(), containing_block_size.height);
        if(m_pos.height > max_height)
        {
            m_pos.height = max_height;
            if(sz.height != 0) m_pos.width = m_pos.height * sz.width / sz.height;
        }
    }

    src_el()->css_w().line_height_w().computed_value = height();

    return m_pos.width + content_offset_width();
}

void litehtml::render_item_image::_place(pixel_t x, pixel_t y, const containing_block_context &/*containing_block_size*/, formatting_context* /*fmt_ctx*/)
{
    m_pos.move_to(x, y);
    m_pos.x += content_offset_left();
    m_pos.y += content_offset_top();
}



litehtml::pixel_t litehtml::render_item_image::calc_max_height(pixel_t image_height, pixel_t containing_block_height)
{
    document::ptr doc = src_el()->get_document();
    return doc->to_pixels(css().get_max_height(), css().get_font_metrics(),
						  containing_block_height == 0 ? image_height : containing_block_height);
}
