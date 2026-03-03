#include "el_svg.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <unordered_map>

#include "container_skia.h"
#include "bridge/magic_tags.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "libs/litehtml/include/litehtml/render_image.h"
#include "libs/litehtml/include/litehtml/render_item.h"
#include "modules/skresources/include/SkResources.h"
#include "modules/svg/include/SkSVGDOM.h"

namespace litehtml {

namespace {

class SatoruImageAsset : public skresources::ImageAsset {
   public:
    SatoruImageAsset(sk_sp<SkImage> image) : fImage(std::move(image)) {}
    bool isMultiFrame() override { return false; }
    skresources::ImageAsset::FrameData getFrameData(float t) override {
        return {fImage, SkSamplingOptions(), SkMatrix::I(),
                skresources::ImageAsset::SizeFit::kFill};
    }

   private:
    sk_sp<SkImage> fImage;
};

class SatoruResourceProvider : public skresources::ResourceProvider {
   public:
    SatoruResourceProvider(const std::map<std::string, image_info>& imageCache)
        : m_imageCache(imageCache) {}

    sk_sp<skresources::ImageAsset> loadImageAsset(const char path[], const char name[],
                                                  const char id[]) const override {
        std::string full_path;
        if (path && path[0]) {
            full_path = path;
            if (name && name[0]) {
                full_path += "/";
                full_path += name;
            }
        } else if (name && name[0]) {
            full_path = name;
        }

        if (full_path.empty()) return nullptr;

        auto it = m_imageCache.find(full_path);
        if (it != m_imageCache.end() && it->second.skImage) {
            return sk_make_sp<SatoruImageAsset>(it->second.skImage);
        }

        return nullptr;
    }

   private:
    const std::map<std::string, image_info>& m_imageCache;
};

const std::unordered_map<std::string, std::string>& get_svg_tag_mapping() {
    static const std::unordered_map<std::string, std::string> mapping = {
        {"clippath", "clipPath"},
        {"lineargradient", "linearGradient"},
        {"radialgradient", "radialGradient"},
        {"textpath", "textPath"},
        {"fegausianblur", "feGaussianBlur"},
        {"fecolormatrix", "feColorMatrix"},
        {"fecomponenttransfer", "feComponentTransfer"},
        {"fecomposite", "feComposite"},
        {"feconvolvematrix", "feConvolveMatrix"},
        {"fediffuselighting", "feDiffuseLighting"},
        {"fedisplacementmap", "feDisplacementMap"},
        {"fedistantlight", "feDistantLight"},
        {"fedropshadow", "feDropShadow"},
        {"feflood", "feFlood"},
        {"fefunca", "feFuncA"},
        {"fefuncb", "feFuncB"},
        {"fefuncg", "feFuncG"},
        {"fefuncr", "feFuncR"},
        {"feimage", "feImage"},
        {"femerge", "feMerge"},
        {"femergenode", "feMergeNode"},
        {"femorphology", "feMorphology"},
        {"feoffset", "feOffset"},
        {"fepointlight", "fePointLight"},
        {"fespecularlighting", "feSpecularLighting"},
        {"fespotlight", "feSpotLight"},
        {"fetile", "feTile"},
        {"feturbulence", "feTurbulence"},
        {"foreignobject", "foreignObject"},
    };
    return mapping;
}

const std::unordered_map<std::string, std::string>& get_svg_attr_mapping() {
    static const std::unordered_map<std::string, std::string> mapping = {
        {"viewbox", "viewBox"},
        {"preserveaspectratio", "preserveAspectRatio"},
        {"gradientunits", "gradientUnits"},
        {"gradienttransform", "gradientTransform"},
        {"patternunits", "patternUnits"},
        {"patterncontentunits", "patternContentUnits"},
        {"patterntransform", "patternTransform"},
        {"clippathunits", "clipPathUnits"},
        {"maskunits", "maskUnits"},
        {"maskcontentunits", "maskContentUnits"},
        {"spreadmethod", "spreadMethod"},
        {"startoffset", "startOffset"},
        {"pathlength", "pathLength"},
        {"lengthadjust", "lengthAdjust"},
        {"textlength", "textLength"},
        {"stddeviation", "stdDeviation"},
        {"surfacescale", "surfaceScale"},
        {"diffuseconstant", "diffuseConstant"},
        {"specularconstant", "specularConstant"},
        {"specularexponent", "specularExponent"},
        {"numoctaves", "numOctaves"},
        {"basefrequency", "baseFrequency"},
        {"stitchtiles", "stitchTiles"},
    };
    return mapping;
}

std::string map_tag(const std::string& tag) {
    auto& m = get_svg_tag_mapping();
    auto it = m.find(tag);
    return (it != m.end()) ? it->second : tag;
}

std::string map_attr(const std::string& attr) {
    auto& m = get_svg_attr_mapping();
    auto it = m.find(attr);
    return (it != m.end()) ? it->second : attr;
}
}  // namespace

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
            const char* str_vb = get_attr("viewbox");
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

std::shared_ptr<render_item> el_svg::create_render_item(
    const std::shared_ptr<render_item>& parent_ri) {
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

    std::string tag_name = map_tag(el->get_tagName());
    os << "<" << tag_name;

    bool is_image_or_use = (tag_name == "image" || tag_name == "use");

    auto tag_ptr = std::dynamic_pointer_cast<html_tag>(el);
    if (tag_ptr) {
        const auto& attrs = html_tag_accessor::get_attrs(tag_ptr.get());
        bool stroke_width_is_zero = false;
        for (auto const& attr : attrs) {
            if (attr.first == "stroke-width" && (attr.second == "0" || attr.second == "0px")) {
                stroke_width_is_zero = true;
                break;
            }
        }

        bool stroke_attr_written = false;
        for (auto const& attr : attrs) {
            std::string attr_name = map_attr(attr.first);
            std::string attr_val = attr.second;
            // Remove newlines and tabs from attribute values (common in base64 data URLs)
            attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\n'), attr_val.end());
            attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\r'), attr_val.end());
            attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\t'), attr_val.end());

            if (attr_name == "stroke") {
                if (stroke_width_is_zero) {
                    os << " stroke=\"none\"";
                } else {
                    os << " stroke=\"" << attr_val << "\"";
                }
                stroke_attr_written = true;
            } else if (is_image_or_use && (attr_name == "href" || attr_name == "xlink:href")) {
                os << " xlink:href=\"" << attr_val << "\"";
            } else if (attr_name != "xmlns" && attr_name != "xmlns:xlink") {
                os << " " << attr_name << "=\"" << attr_val << "\"";
            }
        }
        if (stroke_width_is_zero && !stroke_attr_written) {
            os << " stroke=\"none\"";
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

std::string el_svg::reconstruct_xml(int x, int y, int width, int height) const {
    std::stringstream ss;
    ss << "<svg x=\"" << x << "\" y=\"" << y
       << "\" width=\"" << width << "\" height=\"" << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"";

    for (auto const& attr : m_attrs) {
        std::string attr_name = map_attr(attr.first);
        if (attr_name == "viewBox") {
            ss << " " << attr_name << "=\"" << attr.second << "\"";
        }
    }

    bool stroke_width_is_zero = false;
    for (auto const& attr : m_attrs) {
        if (attr.first == "stroke-width" && (attr.second == "0" || attr.second == "0px")) {
            stroke_width_is_zero = true;
            break;
        }
    }

    bool stroke_attr_written = false;
    for (auto const& attr : m_attrs) {
        std::string attr_name = map_attr(attr.first);
        std::string attr_val = attr.second;
        // Remove newlines and tabs
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\n'), attr_val.end());
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\r'), attr_val.end());
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\t'), attr_val.end());

        if (attr_name == "xmlns" || attr_name == "xmlns:xlink" || attr_name == "x" ||
            attr_name == "y" || attr_name == "width" || attr_name == "height" ||
            attr_name == "viewBox")
            continue;
        if (attr_name == "stroke") {
            if (stroke_width_is_zero) {
                ss << " stroke=\"none\"";
            } else {
                ss << " stroke=\"" << attr_val << "\"";
            }
            stroke_attr_written = true;
        } else if (attr_name == "href" || attr_name == "xlink:href") {
            ss << " xlink:href=\"" << attr_val << "\"";
        } else {
            ss << " " << attr_name << "=\"" << attr_val << "\"";
        }
    }
    if (stroke_width_is_zero && !stroke_attr_written) {
        ss << " stroke=\"none\"";
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

    // For tagging (SVG output), we need absolute coordinates in the reconstructed XML
    // because svg_renderer will replace a magic-colored rect with this XML string.
    std::string xml = reconstruct_xml(container->is_tagging() ? (int)pos.x : 0,
                                      container->is_tagging() ? (int)pos.y : 0,
                                      (int)pos.width, (int)pos.height);

    // Replace "currentColor" with actual computed color
    litehtml::web_color color = css().get_color();
    std::string color_str;
    if (color.alpha < 255) {
        std::stringstream ss;
        ss << "rgba(" << (int)color.red << "," << (int)color.green << "," << (int)color.blue << ","
           << (float)color.alpha / 255.0f << ")";
        color_str = ss.str();
    } else {
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.red, color.green, color.blue);
        color_str = hex;
    }

    size_t start_pos = 0;
    while ((start_pos = xml.find("currentColor", start_pos)) != std::string::npos) {
        xml.replace(start_pos, 12, color_str);
        start_pos += color_str.length();
    }

    if (container->is_tagging()) {
        int index = container->add_inline_svg(xml, pos);
        SkPaint p;
        p.setColor(satoru::make_magic_color(satoru::MagicTagExtended::InlineSvg, index));
        canvas->drawRect(
            SkRect::MakeXYWH((float)pos.x, (float)pos.y, (float)pos.width, (float)pos.height), p);
    } else {
        SkMemoryStream stream(xml.c_str(), xml.size());
        auto resource_provider =
            sk_make_sp<SatoruResourceProvider>(container->get_context().imageCache);
        auto proxy_rp = skresources::DataURIResourceProviderProxy::Make(
            resource_provider, skresources::ImageDecodeStrategy::kPreDecode);

        SkSVGDOM::Builder builder;
        builder.setResourceProvider(proxy_rp);
        auto svg_dom = builder.make(stream);

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
