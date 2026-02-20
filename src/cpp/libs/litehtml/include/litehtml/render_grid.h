#ifndef LITEHTML_RENDER_GRID_H
#define LITEHTML_RENDER_GRID_H

#include "render_block.h"

namespace litehtml
{
	class render_item_grid : public render_item_block
	{
		pixel_t _render_content(pixel_t x, pixel_t y, bool second_pass, const containing_block_context &self_size, formatting_context* fmt_ctx) override;

		vector<pixel_t> m_column_widths;
		vector<pixel_t> m_col_offsets;
		pixel_t m_column_gap;
		bool m_initialized = false;

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
