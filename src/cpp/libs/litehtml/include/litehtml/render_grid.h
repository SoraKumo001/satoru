#ifndef LITEHTML_RENDER_GRID_H
#define LITEHTML_RENDER_GRID_H

#include "render_block.h"

namespace litehtml
{
	class render_item_grid : public render_item_block
	{
		struct grid_item_pos
		{
			int col_start = 0;
			int col_end = 0;
			int row_start = 0;
			int row_end = 0;

			int col_span() const { return col_end - col_start; }
			int row_span() const { return row_end - row_start; }
		};

		struct grid_item
		{
			std::shared_ptr<render_item> el;
			grid_item_pos pos;
		};

		struct grid_layout
		{
			vector<pixel_t> column_widths;
			vector<pixel_t> col_offsets;
			vector<pixel_t> row_heights;
			vector<pixel_t> row_offsets;
			vector<grid_item> items;
			pixel_t total_grid_width = 0;
			pixel_t total_grid_height = 0;
			pixel_t column_gap = 0;
			pixel_t row_gap = 0;

			void clear()
			{
				column_widths.clear();
				col_offsets.clear();
				row_heights.clear();
				row_offsets.clear();
				items.clear();
				total_grid_width = 0;
				total_grid_height = 0;
			}
		};

		grid_layout m_grid_layout;
		bool m_initialized = false;

		void calculate_grid_layout(const containing_block_context& self_size, formatting_context* fmt_ctx);
		void place_grid_items(pixel_t x, pixel_t y, const containing_block_context& self_size, formatting_context* fmt_ctx);

		pixel_t _render_content(pixel_t x, pixel_t y, bool second_pass, const containing_block_context &self_size, formatting_context* fmt_ctx) override;

	public:
		explicit render_item_grid(std::shared_ptr<element> src_el) : render_item_block(std::move(src_el))
		{}

		std::shared_ptr<render_item> clone() override
		{
			return std::make_shared<render_item_grid>(src_el());
		}
		std::shared_ptr<render_item> init() override;
		void draw_children(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, draw_flag flag, int zindex) override;
	};
}

#endif // LITEHTML_RENDER_GRID_H
