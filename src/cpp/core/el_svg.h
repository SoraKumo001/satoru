#ifndef EL_SVG_H
#define EL_SVG_H

#include "libs/litehtml/include/litehtml/html_tag.h"

namespace litehtml {
class el_svg : public html_tag {
   public:
    el_svg(const std::shared_ptr<document>& doc);
    virtual ~el_svg();

    void draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
              const std::shared_ptr<render_item>& ri) override;
    void parse_attributes() override;
    bool is_replaced() const override { return true; }
    void get_content_size(size& sz, pixel_t max_width) override;
    std::shared_ptr<render_item> create_render_item(const std::shared_ptr<render_item>& parent_ri) override;

   private:
    std::string reconstruct_xml(int x, int y) const;
    void write_element(std::ostream& os, const element::ptr& el) const;
};
}  // namespace litehtml

#endif
