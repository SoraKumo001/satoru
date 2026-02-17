#include "html.h"
#include "render_grid.h"
#include "html_tag.h"
#include "document.h"

namespace litehtml
{

std::shared_ptr<render_item> render_item_grid::init()
{
    decltype(m_children) new_children;
    decltype(m_children) inlines;

    auto convert_inlines = [&]()
    {
        if(!inlines.empty())
        {
            // Find last not space
            auto not_space = std::find_if(inlines.rbegin(), inlines.rend(), [&](const std::shared_ptr<render_item>& el)
            {
                return !el->src_el()->is_space();
            });
            if(not_space != inlines.rend())
            {
                // Erase all spaces at the end
                inlines.erase((not_space.base()), inlines.end());
            }

            if (!inlines.empty())
            {
                auto anon_el = std::make_shared<html_tag>(src_el());
                auto anon_ri = std::make_shared<render_item_block>(anon_el);
                for(const auto& inl : inlines)
                {
                    anon_ri->add_child(inl);
                }
                anon_ri->parent(shared_from_this());

                new_children.push_back(anon_ri->init());
            }
            inlines.clear();
        }
    };

    for (const auto& el : m_children)
    {
        if(el->src_el()->css().get_display() == display_inline_text)
        {
            if(!inlines.empty())
            {
                inlines.push_back(el);
            } else
            {
                if (!el->src_el()->is_white_space())
                {
                    inlines.push_back(el);
                }
            }
        } else
        {
            convert_inlines();
            if(el->src_el()->is_block_box())
            {
                el->parent(shared_from_this());
                new_children.push_back(el->init());
            } else
            {
                auto anon_el = std::make_shared<html_tag>(el->src_el());
                auto anon_ri = std::make_shared<render_item_block>(anon_el);
                anon_ri->add_child(el->init());
                anon_ri->parent(shared_from_this());
                new_children.push_back(anon_ri->init());
            }
        }
    }
    convert_inlines();
    children() = new_children;

    return shared_from_this();
}

pixel_t render_item_grid::_render_content(pixel_t x, pixel_t y, bool /*second_pass*/, const containing_block_context &self_size, formatting_context* fmt_ctx)
{
    const auto& columns_template = css().get_grid_template_columns();
    const auto& rows_template = css().get_grid_template_rows();

    int num_columns = (int)columns_template.size();
    if (num_columns == 0) num_columns = 1;

    vector<pixel_t> column_widths(num_columns, 0);
    float total_fr = 0;
    pixel_t fixed_width = 0;

    pixel_t column_gap = css().get_column_gap().calc_percent(self_size.render_width);
    pixel_t row_gap = css().get_row_gap().calc_percent(self_size.render_height);

    // Calculate fixed and percentage columns
    for (int i = 0; i < (int)columns_template.size(); i++)
    {
        const auto& len = columns_template[i];
        if (len.units() == css_units_fr)
        {
            total_fr += len.val();
        }
        else
        {
            column_widths[i] = len.calc_percent(self_size.render_width);
            fixed_width += column_widths[i];
        }
    }
    
    fixed_width += column_gap * (num_columns - 1);

    // Distribute remaining space to fr columns
    if (total_fr > 0)
    {
        pixel_t remaining_width = self_size.render_width - fixed_width;
        if (remaining_width < 0) remaining_width = 0;
        for (int i = 0; i < (int)columns_template.size(); i++)
        {
            const auto& len = columns_template[i];
            if (len.units() == css_units_fr)
            {
                column_widths[i] = remaining_width * (len.val() / total_fr);
            }
        }
    }
    else if (columns_template.empty())
    {
        column_widths[0] = self_size.render_width;
    }

    // Calculate row heights
    int num_rows = (int)rows_template.size();
    if (num_rows == 0)
    {
        // Estimate number of rows based on children and columns
        int num_children = 0;
        for (const auto& el : m_children)
        {
            if (el->src_el()->css().get_position() == element_position_absolute || el->src_el()->css().get_position() == element_position_fixed)
                continue;
            num_children++;
        }
        num_rows = (num_children + num_columns - 1) / num_columns;
    }
    
    vector<pixel_t> row_heights(num_rows, 0);
    float total_row_fr = 0;
    pixel_t fixed_height = 0;

    for (int i = 0; i < (int)rows_template.size(); i++)
    {
        const auto& len = rows_template[i];
        if (len.units() == css_units_fr)
        {
            total_row_fr += len.val();
        }
        else
        {
            row_heights[i] = len.calc_percent(self_size.render_height);
            fixed_height += row_heights[i];
        }
    }
    fixed_height += row_gap * (num_rows - 1);

    if (total_row_fr > 0 && self_size.render_height > 0)
    {
        pixel_t remaining_height = self_size.render_height - fixed_height;
        if (remaining_height < 0) remaining_height = 0;
        for (int i = 0; i < (int)rows_template.size(); i++)
        {
            const auto& len = rows_template[i];
            if (len.units() == css_units_fr)
            {
                row_heights[i] = remaining_height * (len.val() / total_row_fr);
            }
        }
    }

    // Basic placement: fill row by row
    int col = 0;
    int row = 0;
    pixel_t current_x = 0;
    pixel_t current_y = 0;
    
    for (const auto& el : m_children)
    {
        if (el->src_el()->css().get_position() == element_position_absolute || el->src_el()->css().get_position() == element_position_fixed)
            continue;

        containing_block_context cb = self_size;
        cb.render_width = column_widths[col];
        cb.width = cb.render_width;
        if (row < (int)row_heights.size() && row_heights[row] > 0)
        {
            cb.render_height = row_heights[row];
            cb.height = cb.render_height;
            cb.size_mode |= containing_block_context::size_mode_exact_height;
        }
        else
        {
            cb.height = containing_block_context::cbc_value_type_auto;
        }

        el->render(current_x, current_y, cb, fmt_ctx);
        if (row < (int)row_heights.size())
        {
            row_heights[row] = std::max(row_heights[row], el->height());
        }
        else
        {
            // Auto row growth
            row_heights.push_back(el->height());
        }

        current_x += column_widths[col] + column_gap;
        col++;
        if (col >= num_columns)
        {
            col = 0;
            current_x = 0;
            current_y += row_heights[row] + row_gap;
            row++;
        }
    }
    
    if (col != 0)
    {
        current_y += row_heights[row];
    }
    else if (row > 0)
    {
        current_y -= row_gap;
    }

    m_pos.height = current_y;
    m_pos.width = self_size.render_width;

    return m_pos.width;
}

} // namespace litehtml
