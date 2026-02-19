#include "el_image.h"
#include "render_image.h"
#include "document_container.h"

litehtml::el_image::el_image(const document::ptr& doc) : html_tag(doc)
{
	m_css.set_display(display_inline_block);
}

void litehtml::el_image::get_content_size( size& sz, pixel_t /*max_width*/ )
{
	get_document()->container()->get_image_size(m_src.c_str(), nullptr, sz);
}

bool litehtml::el_image::is_replaced() const
{
	return true;
}

void litehtml::el_image::parse_attributes()
{
	m_src = get_attr("src", "");

	// https://html.spec.whatwg.org/multipage/rendering.html#attributes-for-embedded-content-and-images:the-img-element-5
	const char* str = get_attr("width");
	if (str)
		map_to_dimension_property(_width_, str);

	str = get_attr("height");
	if (str)
		map_to_dimension_property(_height_, str);
}

void litehtml::el_image::draw(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip, const std::shared_ptr<render_item> &ri)
{
	html_tag::draw(hdc, x, y, clip, ri);
	position pos = ri->pos();
	pos.x += x;
	pos.y += y;
	pos.round();

	// draw image as background
	if(pos.does_intersect(clip))
	{
		if (pos.width > 0 && pos.height > 0)
		{
			position border_box = pos;
			border_box += ri->get_paddings();
			border_box += ri->get_borders();

			border_radiuses bdr_radius = css().get_borders().radius.calc_percents(border_box.width, border_box.height);

			// 画像領域（content-box）に合わせたクリップを計算
			border_radiuses content_radius = bdr_radius;
			content_radius.top_left_x = std::max(0.0f, bdr_radius.top_left_x - (float)ri->get_paddings().left);
			content_radius.top_left_y = std::max(0.0f, bdr_radius.top_left_y - (float)ri->get_paddings().top);
			content_radius.top_right_x = std::max(0.0f, bdr_radius.top_right_x - (float)ri->get_paddings().right);
			content_radius.top_right_y = std::max(0.0f, bdr_radius.top_right_y - (float)ri->get_paddings().top);
			content_radius.bottom_right_x = std::max(0.0f, bdr_radius.bottom_right_x - (float)ri->get_paddings().right);
			content_radius.bottom_right_y = std::max(0.0f, bdr_radius.bottom_right_y - (float)ri->get_paddings().bottom);
			content_radius.bottom_left_x = std::max(0.0f, bdr_radius.bottom_left_x - (float)ri->get_paddings().left);
			content_radius.bottom_left_y = std::max(0.0f, bdr_radius.bottom_left_y - (float)ri->get_paddings().bottom);

			background_layer layer;
			layer.clip_box = border_box;
			layer.origin_box = pos;
			layer.border_box = border_box;
			layer.repeat = background_repeat_no_repeat;
			layer.border_radius = bdr_radius;

			if (!content_radius.is_zero())
			{
				get_document()->container()->set_clip(pos, content_radius);
			}

			get_document()->container()->draw_image(hdc, layer, m_src, {});

			if (!content_radius.is_zero())
			{
				get_document()->container()->del_clip();
			}
		}
	}
}

void litehtml::el_image::compute_styles(bool recursive)
{
	html_tag::compute_styles(recursive);

	if(!m_src.empty())
	{
		if(!css().get_height().is_predefined() && !css().get_width().is_predefined())
		{
			get_document()->container()->load_image(m_src.c_str(), nullptr, true);
		} else
		{
			get_document()->container()->load_image(m_src.c_str(), nullptr, false);
		}
	}
}

litehtml::string litehtml::el_image::dump_get_name()
{
	return "img src=\"" + m_src + "\"";
}

std::shared_ptr<litehtml::render_item> litehtml::el_image::create_render_item(const std::shared_ptr<render_item>& parent_ri)
{
	auto ret = std::make_shared<render_item_image>(shared_from_this());
	ret->parent(parent_ri);
	return ret;
}