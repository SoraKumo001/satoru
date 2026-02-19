#include "render_image.h"
#include "document.h"
#include "types.h"

litehtml::pixel_t litehtml::render_item_image::_render(pixel_t x, pixel_t y, const containing_block_context &containing_block_size, formatting_context* /*fmt_ctx*/, bool /*second_pass*/)
{
    pixel_t parent_width = containing_block_size.width;
	containing_block_context self_size = calculate_containing_block_context(containing_block_size);

    calc_outlines(parent_width);

    m_pos.move_to(x, y);

    document::ptr doc = src_el()->get_document();

    litehtml::size sz;
    src_el()->get_content_size(sz, containing_block_size.width);

    // Initial width/height from CSS or Intrinsic size
    if(!src_el()->css().get_width().is_predefined())
    {
        m_pos.width = src_el()->css().get_width().calc_percent(parent_width);
    } else {
        m_pos.width = sz.width;
    }

    if(!src_el()->css().get_height().is_predefined())
    {
        m_pos.height = src_el()->css().get_height().calc_percent(containing_block_size.height);
    } else {
        m_pos.height = sz.height;
    }

    // Handle box-sizing: if border-box, the width/height include padding/borders, so we must subtract them to get content size
    if (src_el()->css().get_box_sizing() == box_sizing_border_box)
    {
        m_pos.width  -= m_padding.width() + m_borders.width();
        m_pos.height -= m_padding.height() + m_borders.height();
    }
    
    m_pos.width  = std::max(0.0f, m_pos.width);
    m_pos.height = std::max(0.0f, m_pos.height);

    // Handle aspect ratio if one dimension is auto
    if(sz.width != 0 && sz.height != 0)
    {
        if(src_el()->css().get_width().is_predefined() && !src_el()->css().get_height().is_predefined())
        {
            m_pos.width = m_pos.height * sz.width / sz.height;
        }
        else if(!src_el()->css().get_width().is_predefined() && src_el()->css().get_height().is_predefined())
        {
            m_pos.height = m_pos.width * sz.height / sz.width;
        }
    }

    // Check min/max constraints
    // (Simplified for brevity, standard litehtml would use calc_max_height etc.)

    src_el()->css_w().line_height_w().computed_value = height();

    // Standard litehtml: m_pos is the corner of the CONTENT box
    m_pos.x += content_offset_left();
    m_pos.y += content_offset_top();

    // Return the total width including margins
    return m_pos.width + content_offset_left() + content_offset_right();
}

litehtml::pixel_t litehtml::render_item_image::calc_max_height(pixel_t image_height, pixel_t containing_block_height)
{
    document::ptr doc = src_el()->get_document();
    return doc->to_pixels(css().get_max_height(), css().get_font_metrics(),
						  containing_block_height == 0 ? image_height : containing_block_height);
}
