#include "html.h"
#include "render_grid.h"
#include "render_inline.h"
#include "html_tag.h"
#include "document.h"
#include "el_text.h"
#include "el_space.h"
#include "el_div.h"
#include "document_container.h"

namespace litehtml
{

std::shared_ptr<render_item> render_item_grid::init()
{
    if (m_initialized) return shared_from_this();
    m_initialized = true;

    int column_count = src_el()->css().get_column_count();
    if (column_count <= 0) column_count = 1;

    std::vector<std::shared_ptr<render_item>> items;
    decltype(m_children) current_inlines;

    auto flush_inlines = [&]()
    {
        if(!current_inlines.empty())
        {
            if (column_count > 1)
            {
                // Multi-column flow simulation: split text into flowable words
                for (const auto& inl : current_inlines)
                {
                    if (inl->src_el()->is_text())
                    {
                        string text;
                        inl->src_el()->get_text(text);
                        std::vector<string> words = split_string(text, " \t\r\n", "", "");
                        for (const auto& word : words)
                        {
                            if (word.empty()) continue;
                            auto t_el = std::make_shared<el_text>((word + " ").c_str(), src_el()->get_document());
                            t_el->parent(src_el());
                            t_el->compute_styles(false);
                            auto t_ri = std::make_shared<render_item_inline>(t_el);
                            items.push_back(t_ri->init());
                        }
                    }
                    else
                    {
                        items.push_back(inl->init());
                    }
                }
            }
            else
            {
                // Normal Grid: wrap inlines in a single anonymous block
                auto anon_el = std::make_shared<el_anonymous>(src_el()->get_document());
                anon_el->parent(src_el());
                anon_el->compute_styles(false);
                auto anon_ri = std::make_shared<render_item_block>(anon_el);
                for(const auto& inl : current_inlines)
                {
                    anon_ri->add_child(inl);
                    inl->parent(anon_ri);
                }
                items.push_back(anon_ri->init());
            }
            current_inlines.clear();
        }
    };

    for (const auto& el : m_children)
    {
        if (el->src_el()->css().get_display() == display_inline_text || el->src_el()->is_text())
        {
            if (!el->src_el()->is_white_space())
            {
                current_inlines.push_back(el);
            }
        }
        else
        {
            flush_inlines();
            el->parent(shared_from_this());
            items.push_back(el->init());
        }
    }
    flush_inlines();

    if (column_count > 1 && !items.empty())
    {
        // 2. Group items into column containers to simulate flow
        std::vector<std::shared_ptr<render_item>> columns;
        for (int i = 0; i < column_count; i++)
        {
            auto anon_el = std::make_shared<el_div>(src_el()->get_document());
            anon_el->parent(src_el());
            string style = "display: block; width: 100%; grid-column-start: " + std::to_string(i + 1);
            anon_el->set_attr("style", style.c_str());
            anon_el->compute_styles(false);
            
            auto anon_ri = std::make_shared<render_item_block>(anon_el);
            anon_ri->parent(shared_from_this());
            columns.push_back(anon_ri);
        }

        size_t items_per_col = (items.size() + column_count - 1) / column_count;
        if (items_per_col == 0) items_per_col = 1;
        for (size_t i = 0; i < items.size(); i++)
        {
            size_t col_idx = std::min((size_t)(i / items_per_col), (size_t)(column_count - 1));
            columns[col_idx]->add_child(items[i]);
            items[i]->parent(columns[col_idx]);
        }

        children().clear();
        for (auto& col : columns)
        {
            children().push_back(col->init());
        }
    }
    else
    {
        children().clear();
        for (const auto& item : items)
        {
            children().push_back(item);
        }
    }

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

    m_column_widths.assign(num_columns, 0);
    float total_fr = 0;
    pixel_t fixed_width = 0;

    m_column_gap = css().get_column_gap().calc_percent(self_size.render_width);
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
            m_column_widths[i] = len.calc_percent(self_size.render_width);
            fixed_width += m_column_widths[i];
        }
    }
    
    fixed_width += m_column_gap * (num_columns - 1);

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
                m_column_widths[i] = remaining_width * (len.val() / total_fr);
            }
        }
    }
    else if (columns_template.empty())
    {
        m_column_widths[0] = self_size.render_width;
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
            cell_width += m_column_widths[i];
            if (i > p.pos.col_start) cell_width += m_column_gap;
        }

        containing_block_context cb = self_size;
        // Available content width = cell_width - margins - borders - padding
        cb.render_width = cell_width - p.el->margin_left() - p.el->margin_right() - p.el->content_offset_width();
        cb.width = cb.render_width;
        cb.height = containing_block_context::cbc_value_type_auto;

        // Render at (0,0) relative to parent to measure content height.
        p.el->measure(cb, fmt_ctx);
    }

    // Distribute heights: process 1-row items first, then spans
    for (int span = 1; span <= num_rows; span++)
    {
        for (auto& p : placements)
        {
            int item_span = p.pos.row_end - p.pos.row_start;
            if (item_span != span) continue;

            // total_h should be the margin-box height
            pixel_t total_h = p.el->height();
            
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
    for (int i = 0; i < num_columns; i++) { acc_x += m_column_widths[i] + m_column_gap; }
    pixel_t acc_y = 0;
    for (int i = 0; i < num_rows; i++) { acc_y += row_heights[i] + row_gap; }

    pixel_t total_grid_width = acc_x > 0 ? acc_x - m_column_gap : 0;
    pixel_t total_grid_height = acc_y > 0 ? acc_y - row_gap : 0;

    // Second pass: final placement with relative coordinates
    m_col_offsets.assign(num_columns, 0);
    vector<pixel_t> row_offsets(num_rows, 0);
    
	if (self_size.size_mode & containing_block_context::size_mode_measure)
	{
		m_pos.height = (self_size.render_height.type != containing_block_context::cbc_value_type_auto) ? (pixel_t)self_size.render_height : total_grid_height;
		m_pos.width = self_size.render_width;
		return m_pos.height;
	}

    pixel_t extra_x = 0;
    pixel_t extra_y = 0;
    pixel_t justify_gap = m_column_gap;
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
    for (int i = 0; i < num_columns; i++) { m_col_offsets[i] = acc_x; acc_x += m_column_widths[i] + justify_gap; }
    acc_y = extra_y;
    for (int i = 0; i < num_rows; i++) { row_offsets[i] = acc_y; acc_y += row_heights[i] + align_gap; }

    for (auto& p : placements)
    {
        pixel_t item_rel_x = m_col_offsets[p.pos.col_start];
        pixel_t item_rel_y = row_offsets[p.pos.row_start];
        
        pixel_t cell_width = 0;
        for (int i = p.pos.col_start; i < p.pos.col_end && i < num_columns; i++)
        {
            cell_width += m_column_widths[i];
            if (i > p.pos.col_start) cell_width += m_column_gap;
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

        // Render at relative coordinates. render() adds margins/offsets internally.
        p.el->measure(cb, fmt_ctx);
        p.el->place(base_rel_x + item_rel_x, base_rel_y + item_rel_y, cb, fmt_ctx);
    }

    m_pos.height = (self_size.render_height.type != containing_block_context::cbc_value_type_auto) ? (pixel_t)self_size.render_height : total_grid_height;
    m_pos.width = self_size.render_width;

    return m_pos.height;
}

void render_item_grid::draw_children(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, draw_flag flag, int zindex)
{
    render_item_block::draw_children(hdc, x, y, clip, flag, zindex);

    if (flag == draw_block && zindex == 0)
    {
        const css_border& rule = src_el()->css().get_column_rule();
        if (rule.style != border_style_none && rule.style != border_style_hidden && m_column_widths.size() > 1)
        {
            pixel_t rule_width = rule.width.val(); // Already converted to px
            if (rule_width <= 0) rule_width = 1;

            position pos = m_pos;
            pos.x += x - get_scroll_left();
            pos.y += y - get_scroll_top();

            for (size_t i = 0; i < m_column_widths.size() - 1; i++)
            {
                // The gap is between column i and column i+1
                pixel_t gap_start = m_col_offsets[i] + m_column_widths[i];
                pixel_t gap_end = m_col_offsets[i + 1];
                
                // Position the rule in the center of the actual gap
                pixel_t rule_x = pos.x + (gap_start + gap_end - rule_width) / 2;
                
                position rule_pos;
                rule_pos.x = rule_x;
                rule_pos.y = pos.y;
                rule_pos.width = rule_width;
                rule_pos.height = m_pos.height;

                if (rule_pos.does_intersect(clip))
                {
                    if (rule.style == border_style_solid)
                    {
                        background_layer layer;
                        layer.border_box = rule_pos;
                        layer.clip_box = *clip;
                        layer.origin_box = rule_pos;
                        src_el()->get_document()->container()->draw_solid_fill(hdc, layer, rule.color);
                    }
                    else
                    {
                        borders rules;
                        rules.left.width = rule_width;
                        rules.left.style = rule.style;
                        rules.left.color = rule.color;
                        src_el()->get_document()->container()->draw_borders(hdc, rules, rule_pos, false);
                    }
                }
            }
        }
    }
}

} // namespace litehtml
