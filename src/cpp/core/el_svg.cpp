#include "el_svg.h"

#include <sstream>

#include "container_skia.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "libs/litehtml/include/litehtml/render_item.h"
#include "libs/litehtml/include/litehtml/render_image.h"
#include "modules/svg/include/SkSVGDOM.h"

namespace litehtml {

el_svg::el_svg(const std::shared_ptr<document>& doc) : html_tag(doc) {
    m_css.set_display(display_inline_block);
}
el_svg::~el_svg() {}

void el_svg::parse_attributes() {
    html_tag::parse_attributes();
    const char* str_w = get_attr("width");
    const char* str_h = get_attr("height");
    if (str_w) map_to_dimension_property(_width_, str_w);
    if (str_h) map_to_dimension_property(_height_, str_h);
}

void el_svg::get_content_size(size& sz, pixel_t max_width) {
    sz.width = (pixel_t)css().get_width().val();
    sz.height = (pixel_t)css().get_height().val();

    if (sz.width == 0 || sz.height == 0) {
        const char* str_w = get_attr("width");
        const char* str_h = get_attr("height");
        if (str_w) sz.width = (pixel_t)atof(str_w);
        if (str_h) sz.height = (pixel_t)atof(str_h);

        if (sz.width == 0 || sz.height == 0) {
            const char* str_vb = get_attr("viewBox");
            if (str_vb) {
                float vx, vy, vw, vh;
                // Try space-separated first
                if (sscanf(str_vb, "%f %f %f %f", &vx, &vy, &vw, &vh) == 4 ||
                    sscanf(str_vb, "%f,%f,%f,%f", &vx, &vy, &vw, &vh) == 4) {
                    if (vw > 0 && vh > 0) {
                        if (sz.width == 0 && sz.height == 0) {
                            sz.width = vw;
                            sz.height = vh;
                        } else if (sz.width == 0) {
                            sz.width = sz.height * vw / vh;
                        } else if (sz.height == 0) {
                            sz.height = sz.width * vh / vw;
                        }
                    }
                }
            }
        }
    }

    if (sz.width == 0) sz.width = 100;
    if (sz.height == 0) sz.height = 100;
}

std::shared_ptr<render_item> el_svg::create_render_item(const std::shared_ptr<render_item>& parent_ri) {
    auto ret = std::make_shared<render_item_image>(shared_from_this());
    ret->parent(parent_ri);
    return ret;
}

class html_tag_accessor : public html_tag {
   public:
    static const string_map& get_attrs(const html_tag* tag) {
        return ((const html_tag_accessor*)tag)->m_attrs;
    }
    static const elements_list& get_children(const html_tag* tag) {
        return ((const html_tag_accessor*)tag)->m_children;
    }
};

void el_svg::write_element(std::ostream& os, const element::ptr& el) const {
    if (el->is_text()) {
        string text;
        el->get_text(text);
        os << text;
        return;
    }
    if (el->is_comment()) return;

    const char* tag_name = el->get_tagName();
    os << "<" << tag_name;

    auto tag_ptr = std::dynamic_pointer_cast<html_tag>(el);
    if (tag_ptr) {
        const auto& attrs = html_tag_accessor::get_attrs(tag_ptr.get());
        for (auto const& attr : attrs) {
            os << " " << attr.first << "=\"" << attr.second << "\"";
        }
    }

    auto children = el->children();
    if (children.empty()) {
        os << "/>";
    } else {
        os << ">";
        for (auto const& child : children) {
            write_element(os, child);
        }
        os << "</" << tag_name << ">";
    }
}

std::string el_svg::reconstruct_xml(int x, int y) const {
    std::stringstream ss;
    ss << "<svg x=\"" << x << "\" y=\"" << y << "\" xmlns=\"http://www.w3.org/2000/svg\"";
    for (auto const& attr : m_attrs) {
        if (attr.first == "xmlns" || attr.first == "x" || attr.first == "y") continue;
        ss << " " << attr.first << "=\"" << attr.second << "\"";
    }
    ss << ">";
    for (auto const& child : m_children) {
        write_element(ss, child);
    }
    ss << "</svg>";
    return ss.str();
}

void el_svg::draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
                  const std::shared_ptr<render_item>& ri) {
    container_skia* container = dynamic_cast<container_skia*>(get_document()->container());
    if (!container) return;

    SkCanvas* canvas = container->get_canvas();
    if (!canvas) return;

    position pos = ri->pos();
    pos.x += x;
    pos.y += y;

    std::string xml = reconstruct_xml(pos.x, pos.y);

    if (container->is_tagging()) {
        int index = container->add_inline_svg(xml, pos);
        SkPaint p;
        p.setColor(SkColorSetARGB(255, 1, 4, (index & 0xFF)));
        canvas->drawRect(
            SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height), p);
    } else {
        SkMemoryStream stream(xml.c_str(), xml.size());
        auto svg_dom = SkSVGDOM::MakeFromStream(stream);
        if (svg_dom) {
            canvas->save();
            canvas->translate((float)pos.x, (float)pos.y);

            SkSize container_size = SkSize::Make((float)pos.width, (float)pos.height);
            svg_dom->setContainerSize(container_size);
            svg_dom->render(canvas);

            canvas->restore();
        }
    }
}

}  // namespace litehtml
