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

namespace
{
struct grid_line
{
    int index = 0;
    int span = 1;
    bool is_span = false;

    void parse(const css_token_vector& tokens)
    {
        if (tokens.empty()) return;
        
        for (int i = 0; i < (int)tokens.size(); i++)
        {
            const auto& tok = tokens[i];
            if (tok.type == IDENT && tok.ident() == "span")
            {
                is_span = true;
            }
            else if (tok.type == NUMBER)
            {
                if (is_span) span = (int)tok.n.number;
                else index = (int)tok.n.number;
            }
        }
    }
};

struct grid_item_pos
{
    int col_start = 0;
    int col_end = 0;
    int row_start = 0;
    int row_end = 0;
};
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

    // Determine placements and number of rows
    struct item_placement
    {
        std::shared_ptr<render_item> el;
        grid_item_pos pos;
    };
    vector<item_placement> placements;
    int max_row = 0;

    // Occupancy map: row -> vector<bool> (cols)
    vector<vector<bool>> occupied;
    auto is_occupied = [&](int r, int c) -> bool {
        if (r < (int)occupied.size()) {
            if (c < (int)occupied[r].size()) return (bool)occupied[r][c];
        }
        return false;
    };
    auto mark_occupied = [&](int r, int c, int rs, int cs) {
        if (r + rs > (int)occupied.size()) occupied.resize(r + rs, vector<bool>(num_columns, false));
        for (int i = r; i < r + rs; i++) {
            for (int j = c; j < c + cs; j++) {
                if (j < num_columns) occupied[i][j] = true;
            }
        }
    };

    int curr_row = 0;
    int curr_col = 0;

    for (const auto& el : m_children)
    {
        if (el->src_el()->css().get_position() == element_position_absolute || el->src_el()->css().get_position() == element_position_fixed)
            continue;

        grid_line cs, ce, rs, re;
        cs.parse(el->src_el()->css().get_grid_column_start());
        ce.parse(el->src_el()->css().get_grid_column_end());
        rs.parse(el->src_el()->css().get_grid_row_start());
        re.parse(el->src_el()->css().get_grid_row_end());

        int item_col_start = 0;
        int item_col_span = 1;
        int item_row_start = 0;
        int item_row_span = 1;

        // Resolve spans and fixed positions
        if (cs.is_span)
        {
            item_col_span = cs.span;
        }
        else if (cs.index > 0)
        {
            item_col_start = cs.index - 1;
        }

        if (ce.is_span)
        {
            item_col_span = ce.span;
        }
        else if (ce.index > cs.index && cs.index > 0)
        {
            item_col_span = ce.index - cs.index;
        }

        if (rs.is_span)
        {
            item_row_span = rs.span;
        }
        else if (rs.index > 0)
        {
            item_row_start = rs.index - 1;
        }

        if (re.is_span)
        {
            item_row_span = re.span;
        }
        else if (re.index > rs.index && rs.index > 0)
        {
            item_row_span = re.index - rs.index;
        }

        // Auto placement: find a big enough empty rectangle
        if (cs.index <= 0 && !cs.is_span)
        {
            bool found = false;
            while (!found)
            {
                if (curr_col + item_col_span > num_columns)
                {
                    curr_col = 0;
                    curr_row++;
                }

                bool conflict = false;
                for (int r = curr_row; r < curr_row + item_row_span; r++)
                {
                    for (int c = curr_col; c < curr_col + item_col_span; c++)
                    {
                        if (is_occupied(r, c))
                        {
                            conflict = true;
                            break;
                        }
                    }
                    if (conflict) break;
                }

                if (!conflict)
                {
                    item_col_start = curr_col;
                    item_row_start = curr_row;
                    found = true;
                    // Move cursor for next auto-placed item
                    curr_col += item_col_span;
                }
                else
                {
                    curr_col++;
                    if (curr_col >= num_columns)
                    {
                        curr_col = 0;
                        curr_row++;
                    }
                }
            }
        }
        else if (cs.is_span && cs.index <= 0)
        {
            // Handle span without fixed start
            bool found = false;
            while (!found)
            {
                if (curr_col + item_col_span > num_columns)
                {
                    curr_col = 0;
                    curr_row++;
                }

                bool conflict = false;
                for (int r = curr_row; r < curr_row + item_row_span; r++)
                {
                    for (int c = curr_col; c < curr_col + item_col_span; c++)
                    {
                        if (is_occupied(r, c))
                        {
                            conflict = true;
                            break;
                        }
                    }
                    if (conflict) break;
                }

                if (!conflict)
                {
                    item_col_start = curr_col;
                    item_row_start = curr_row;
                    found = true;
                    curr_col += item_col_span;
                }
                else
                {
                    curr_col++;
                    if (curr_col >= num_columns)
                    {
                        curr_col = 0;
                        curr_row++;
                    }
                }
            }
        }

        mark_occupied(item_row_start, item_col_start, item_row_span, item_col_span);
        
        item_placement p;
        p.el = el;
        p.pos = { item_col_start, item_col_start + item_col_span, item_row_start, item_row_start + item_row_span };
        placements.push_back(p);
        max_row = std::max(max_row, p.pos.row_end);
    }

    int num_rows = std::max((int)rows_template.size(), max_row);
    vector<pixel_t> row_heights(num_rows, 0);
    float total_row_fr = 0;
    pixel_t fixed_height = 0;

    // Calculate fixed and percentage rows
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

    // Distribute remaining space to fr rows if container height is fixed
    if (total_row_fr > 0 && self_size.render_height.type != containing_block_context::cbc_value_type_auto)
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

    // Use relative coordinates (0,0) as the base for child rendering.
    // litehtml's render(x, y) adds these to the parent's position internally.
    pixel_t base_rel_x = 0;
    pixel_t base_rel_y = 0;

    // First pass: render items to determine required heights
    for (auto& p : placements)
    {
        pixel_t cell_width = 0;
        for (int i = p.pos.col_start; i < p.pos.col_end && i < num_columns; i++)
        {
            cell_width += column_widths[i];
            if (i > p.pos.col_start) cell_width += column_gap;
        }

        containing_block_context cb = self_size;
        // Correctly calculate available content width by subtracting margins AND padding/borders
        cb.render_width = cell_width - p.el->margin_left() - p.el->margin_right() - p.el->content_offset_width();
        cb.width = cb.render_width;
        cb.height = containing_block_context::cbc_value_type_auto;

        // Render at relative coordinates. We add margins because litehtml's render(x, y) expects the border-box origin.
        p.el->render(base_rel_x + p.el->margin_left(), base_rel_y + p.el->margin_top(), cb, fmt_ctx);
    }

    // Distribute heights: process 1-row items first, then spans
    for (int span = 1; span <= num_rows; span++)
    {
        for (auto& p : placements)
        {
            int item_span = p.pos.row_end - p.pos.row_start;
            if (item_span != span) continue;

            pixel_t total_h = p.el->pos().height + p.el->margin_top() + p.el->margin_bottom();
            
            // Heuristic: Ensure a minimum height based on font size and content offsets 
            // to handle cases where fonts are not yet loaded.
            pixel_t min_content_h = p.el->src_el()->css().get_font_size() * 1.2f;
            pixel_t min_h = min_content_h + p.el->content_offset_height() + p.el->margin_top() + p.el->margin_bottom();
            if (total_h < min_h) total_h = min_h;

            pixel_t current_total = 0;
            for (int i = p.pos.row_start; i < p.pos.row_end && i < num_rows; i++)
            {
                current_total += row_heights[i];
                if (i > p.pos.row_start) current_total += row_gap;
            }

            if (total_h > current_total)
            {
                pixel_t diff = total_h - current_total;
                for (int i = p.pos.row_start; i < p.pos.row_end && i < num_rows; i++)
                {
                    row_heights[i] += diff / item_span;
                }
            }
        }
    }

    // Calculate total grid size for justify-content/align-content
    pixel_t acc_x = 0;
    for (int i = 0; i < num_columns; i++) { acc_x += column_widths[i] + column_gap; }
    pixel_t acc_y = 0;
    for (int i = 0; i < num_rows; i++) { acc_y += row_heights[i] + row_gap; }

    pixel_t total_grid_width = acc_x > 0 ? acc_x - column_gap : 0;
    pixel_t total_grid_height = acc_y > 0 ? acc_y - row_gap : 0;

    // Second pass: final placement with relative coordinates
    vector<pixel_t> col_offsets(num_columns, 0);
    vector<pixel_t> row_offsets(num_rows, 0);
    
    pixel_t extra_x = 0;
    pixel_t extra_y = 0;
    pixel_t justify_gap = column_gap;
    pixel_t align_gap = row_gap;

    // justify-content
    auto jc = css().get_flex_justify_content();
    if (jc == flex_justify_content_center) extra_x = (self_size.render_width - total_grid_width) / 2;
    else if (jc == flex_justify_content_end || jc == flex_justify_content_flex_end) extra_x = self_size.render_width - total_grid_width;
    else if (jc == flex_justify_content_space_between && num_columns > 1) justify_gap += (self_size.render_width - total_grid_width) / (num_columns - 1);
    else if (jc == flex_justify_content_space_around && num_columns > 0)
    {
        pixel_t diff = self_size.render_width - total_grid_width;
        extra_x = diff / (num_columns * 2);
        justify_gap += diff / num_columns;
    }
    else if (jc == flex_justify_content_space_evenly && num_columns > 0)
    {
        pixel_t diff = self_size.render_width - total_grid_width;
        extra_x = diff / (num_columns + 1);
        justify_gap += diff / (num_columns + 1);
    }

    // align-content
    auto ac = css().get_flex_align_content();
    if (self_size.render_height.type != containing_block_context::cbc_value_type_auto)
    {
        pixel_t container_h = self_size.render_height;
        pixel_t diff = container_h - total_grid_height;
        if (ac == flex_align_content_center) extra_y = diff / 2;
        else if (ac == flex_align_content_end || ac == flex_align_content_flex_end) extra_y = diff;
        else if (ac == flex_align_content_space_between && num_rows > 1) align_gap += diff / (num_rows - 1);
        else if (ac == flex_align_content_space_around && num_rows > 0)
        {
            extra_y = diff / (num_rows * 2);
            align_gap += diff / num_rows;
        }
        else if (ac == flex_align_content_space_evenly && num_rows > 0)
        {
            extra_y = diff / (num_rows + 1);
            align_gap += diff / (num_rows + 1);
        }
    }

    acc_x = extra_x;
    for (int i = 0; i < num_columns; i++) { col_offsets[i] = acc_x; acc_x += column_widths[i] + justify_gap; }
    acc_y = extra_y;
    for (int i = 0; i < num_rows; i++) { row_offsets[i] = acc_y; acc_y += row_heights[i] + align_gap; }

    for (auto& p : placements)
    {
        pixel_t item_rel_x = col_offsets[p.pos.col_start];
        pixel_t item_rel_y = row_offsets[p.pos.row_start];
        
        pixel_t cell_width = 0;
        for (int i = p.pos.col_start; i < p.pos.col_end && i < num_columns; i++)
        {
            cell_width += column_widths[i];
            if (i > p.pos.col_start) cell_width += column_gap;
        }
        pixel_t cell_height = 0;
        for (int i = p.pos.row_start; i < p.pos.row_end && i < num_rows; i++)
        {
            cell_height += row_heights[i];
            if (i > p.pos.row_start) cell_height += row_gap;
        }

        containing_block_context cb = self_size;
        cb.render_width = cell_width - p.el->margin_left() - p.el->margin_right() - p.el->content_offset_width();
        cb.width = cb.render_width;
        cb.render_height = cell_height - p.el->margin_top() - p.el->margin_bottom() - p.el->content_offset_height();
        cb.height = cb.render_height;
        cb.size_mode |= containing_block_context::size_mode_exact_height;
        cb.size_mode |= containing_block_context::size_mode_exact_width;

        // Render at relative coordinates + margins
        p.el->render(base_rel_x + item_rel_x + p.el->margin_left(), base_rel_y + item_rel_y + p.el->margin_top(), cb, fmt_ctx);
    }

    m_pos.height = total_grid_height;
    m_pos.width = self_size.render_width;

    return m_pos.height;
}

} // namespace litehtml
