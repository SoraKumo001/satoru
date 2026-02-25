#include "svg_renderer.h"

#include <litehtml/master_css.h>

#include <cstdio>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "api/satoru_api.h"
#include "bridge/magic_tags.h"
#include "core/container_skia.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkStream.h"
#include "include/effects/SkGradient.h"
#include "include/encode/SkPngEncoder.h"
#include "include/svg/SkSVGCanvas.h"
#include "include/utils/SkParsePath.h"
#include "utils/skia_utils.h"
#include "utils/logging.h"

namespace litehtml {
std::vector<css_token_vector> parse_comma_separated_list(const css_token_vector &tokens);
}

namespace {
static std::string bitmapToDataUrl(const SkBitmap &bitmap) {
    SkDynamicMemoryWStream stream;
    if (!SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) return "";
    sk_sp<SkData> data = stream.detachAsData();
    return "data:image/png;base64," + base64_encode((const uint8_t *)data->data(), data->size());
}

static bool has_radius(const litehtml::border_radiuses &r) {
    return r.top_left_x > 0 || r.top_left_y > 0 || r.top_right_x > 0 || r.top_right_y > 0 ||
           r.bottom_right_x > 0 || r.bottom_right_y > 0 || r.bottom_left_x > 0 ||
           r.bottom_left_y > 0;
}

static std::string path_from_rrect(const litehtml::position &pos,
                                   const litehtml::border_radiuses &r) {
    SkRRect rrect = make_rrect(pos, r);

    SkPathBuilder pathBuilder;
    pathBuilder.addRRect(rrect);
    SkPath path = pathBuilder.detach();

    SkString svgPath = SkParsePath::ToSVGString(path);

    return std::string(svgPath.c_str());
}

struct TagAttr {
    std::string_view name;
    std::string_view value;
};

struct FastTag {
    std::string_view name;
    std::vector<TagAttr> attrs;
    bool closing = false;
    bool selfClosing = false;

    std::string_view getAttr(std::string_view attrName) const {
        for (const auto &a : attrs) {
            if (a.name == attrName) return a.value;
        }
        return {};
    }

    bool isTag(std::string_view target) const {
        if (name == target) return true;
        size_t colon = name.find(':');
        if (colon != std::string_view::npos) {
            return name.substr(colon + 1) == target;
        }
        return false;
    }

    satoru::DecodedMagicTag getMagicTag() const {
        std::string_view colorVal;
        for (const auto &a : attrs) {
            if (a.name == "fill") {
                colorVal = a.value;
                break;
            }
        }
        if (colorVal.empty()) {
            for (const auto &a : attrs) {
                if (a.name == "style") {
                    size_t f = a.value.find("fill:");
                    if (f != std::string_view::npos) {
                        size_t vStart = f + 5;
                        while (vStart < a.value.size() && isspace(a.value[vStart])) vStart++;
                        size_t vEnd = a.value.find_first_of(";\"", vStart);
                        colorVal = a.value.substr(vStart, vEnd - vStart);
                    }
                    break;
                }
            }
        }

        if (colorVal.empty()) return {};

        if (colorVal.empty() ||
            (colorVal[0] != '#' && colorVal.find("rgb") == std::string_view::npos)) {
            return {};
        }

        if (colorVal.size() >= 7 && colorVal[0] == '#') {
            try {
                int r = std::stoi(std::string(colorVal.substr(1, 2)), nullptr, 16);
                int g = std::stoi(std::string(colorVal.substr(3, 2)), nullptr, 16);
                int b = std::stoi(std::string(colorVal.substr(5, 2)), nullptr, 16);
                return satoru::decode_magic_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
            } catch (...) {
            }
        } else if (colorVal.find("rgb(") == 0) {
            int r, g, b;
            if (sscanf(std::string(colorVal).c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3 ||
                sscanf(std::string(colorVal).c_str(), "rgb(%d, %d, %d)", &r, &g, &b) == 3) {
                return satoru::decode_magic_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
            }
        }
        return {};
    }
};

class SvgScanner {
    std::string_view svg;
    size_t pos = 0;

   public:
    SvgScanner(std::string_view s) : svg(s) {}

    bool isAtEnd() const { return pos >= svg.size(); }

    std::string_view scanTo(char c) {
        size_t start = pos;
        size_t end = svg.find(c, pos);
        if (end == std::string_view::npos) {
            pos = svg.size();
            return svg.substr(start);
        }
        pos = end;
        return svg.substr(start, end - start);
    }

    FastTag parseTag() {
        FastTag tag;
        if (svg[pos] != '<') return tag;
        pos++;  // skip '<'

        if (pos < svg.size() && svg[pos] == '/') {
            tag.closing = true;
            pos++;
        }

        size_t nameStart = pos;
        while (pos < svg.size() && !isspace(svg[pos]) && svg[pos] != '/' && svg[pos] != '>') pos++;
        tag.name = svg.substr(nameStart, pos - nameStart);

        while (pos < svg.size()) {
            while (pos < svg.size() && isspace(svg[pos])) pos++;
            if (pos >= svg.size() || svg[pos] == '>') break;
            if (svg[pos] == '/') {
                tag.selfClosing = true;
                pos++;
                continue;
            }

            size_t attrNameStart = pos;
            while (pos < svg.size() && !isspace(svg[pos]) && svg[pos] != '=' && svg[pos] != '>' &&
                   svg[pos] != '/')
                pos++;
            std::string_view attrName = svg.substr(attrNameStart, pos - attrNameStart);

            while (pos < svg.size() && isspace(svg[pos])) pos++;
            if (pos < svg.size() && svg[pos] == '=') {
                pos++;
                while (pos < svg.size() && isspace(svg[pos])) pos++;
                if (pos < svg.size() && (svg[pos] == '"' || svg[pos] == '\'')) {
                    char quote = svg[pos++];
                    size_t valStart = pos;
                    while (pos < svg.size() && svg[pos] != quote) pos++;
                    tag.attrs.push_back({attrName, svg.substr(valStart, pos - valStart)});
                    if (pos < svg.size()) pos++;  // skip quote
                } else {
                    size_t valStart = pos;
                    while (pos < svg.size() && !isspace(svg[pos]) && svg[pos] != '/' &&
                           svg[pos] != '>')
                        pos++;
                    tag.attrs.push_back({attrName, svg.substr(valStart, pos - valStart)});
                }
            } else {
                tag.attrs.push_back({attrName, {}});
            }
        }

        if (pos < svg.size() && svg[pos] == '>') pos++;
        return tag;
    }

    size_t getPos() const { return pos; }
};

static void serializeFastTag(std::string &out, const FastTag &tag) {
    out.append("<");
    if (tag.closing) out.append("/");
    out.append(tag.name);
    for (const auto &attr : tag.attrs) {
        out.append(" ");
        out.append(attr.name);
        if (attr.value.data()) {
            out.append("=\"");
            out.append(attr.value);
            out.append("\"");
        }
    }
    if (tag.selfClosing) out.append(" /");
    out.append(">");
}

static void processTextDraw(FastTag &info, std::string &out, const text_draw_info &drawInfo) {
    std::string textColor = "rgb(" + std::to_string((int)drawInfo.color.red) + "," +
                            std::to_string((int)drawInfo.color.green) + "," +
                            std::to_string((int)drawInfo.color.blue) + ")";
    float opacity = ((float)drawInfo.color.alpha / 255.0f) * drawInfo.opacity;

    out.append("<");
    out.append(info.name);
    out.append(" font-weight=\"");
    out.append(std::to_string(drawInfo.weight));
    out.append("\" font-style=\"");
    out.append(drawInfo.italic ? "italic" : "normal");
    out.append("\" fill=\"");
    out.append(textColor);
    out.append("\" fill-opacity=\"");
    out.append(std::to_string(opacity));
    out.append("\"");

    for (const auto &attr : info.attrs) {
        if (attr.name == "font-weight" || attr.name == "font-style" || attr.name == "fill" ||
            attr.name == "fill-opacity")
            continue;
        out.append(" ");
        out.append(attr.name);
        out.append("=\"");
        if (attr.name == "stroke")
            out.append(textColor);
        else
            out.append(attr.value);
        out.append("\"");
    }

    if (info.selfClosing) out.append(" /");
    out.append(">");
}

static std::string generateDefs(const container_skia &render_container,
                                const SatoruContext &context, const RenderOptions &options) {
    std::stringstream defs;

    if (!options.svgTextToPaths) {
        std::string fontFaceCss = context.fontManager.generateFontFaceCSS();
        if (!fontFaceCss.empty()) {
            defs << "<style type=\"text/css\"><![CDATA[\n" << fontFaceCss << "]]></style>\n";
        }
    }

    const auto &shadows = render_container.get_used_shadows();
    for (size_t i = 0; i < shadows.size(); ++i) {
        const auto &s = shadows[i];
        int index = (int)(i + 1);
        defs << "<filter id=\"shadow-" << index
             << "\" x=\"-100%\" y=\"-100%\" width=\"300%\" height=\"300%\">";

        std::string floodColor = "rgb(" + std::to_string((int)s.color.red) + "," +
                                 std::to_string((int)s.color.green) + "," +
                                 std::to_string((int)s.color.blue) + ")";
        float floodOpacity = ((float)s.color.alpha / 255.0f) * s.opacity;

        if (s.inset) {
            defs << "<feFlood flood-color=\"" << floodColor << "\" flood-opacity=\"" << floodOpacity
                 << "\" result=\"color\"/>"
                 << "<feComposite in=\"color\" in2=\"SourceAlpha\" operator=\"out\" "
                    "result=\"inverse\"/>"
                 << "<feGaussianBlur in=\"inverse\" stdDeviation=\"" << s.blur * 0.5f
                 << "\" result=\"blur\"/>"
                 << "<feOffset dx=\"" << s.x << "\" dy=\"" << s.y << "\" result=\"offset\"/>"
                 << "<feComposite in=\"offset\" in2=\"SourceAlpha\" operator=\"in\" "
                    "result=\"inset-shadow\"/>"
                 << "<feMerge><feMergeNode in=\"inset-shadow\"/></feMerge>";
        } else {
            defs << "<feGaussianBlur in=\"SourceAlpha\" stdDeviation=\"" << s.blur * 0.5f
                 << "\" result=\"blur\"/>"
                 << "<feOffset in=\"blur\" dx=\"" << s.x << "\" dy=\"" << s.y
                 << "\" result=\"offset\"/>"
                 << "<feFlood flood-color=\"" << floodColor << "\" flood-opacity=\"" << floodOpacity
                 << "\" result=\"color\"/>"
                 << "<feComposite in=\"color\" in2=\"offset\" operator=\"in\" result=\"shadow\"/>"
                 << "<feComposite in=\"shadow\" in2=\"SourceAlpha\" operator=\"out\" "
                    "result=\"clipped-shadow\"/>"
                 << "<feMerge><feMergeNode in=\"clipped-shadow\"/></feMerge>";
        }
        defs << "</filter>";
    }

    const auto &textShadows = render_container.get_used_text_shadows();
    for (size_t i = 0; i < textShadows.size(); ++i) {
        const auto &ts = textShadows[i];
        int index = (int)(i + 1);
        defs << "<filter id=\"text-shadow-" << index
             << "\" x=\"-100%\" y=\"-100%\" width=\"300%\" height=\"300%\">";

        for (size_t si = 0; si < ts.shadows.size(); ++si) {
            const auto &s = ts.shadows[si];
            std::string resultName = "shadow-" + std::to_string(si);
            std::string floodColor = "rgb(" + std::to_string((int)s.color.red) + "," +
                                     std::to_string((int)s.color.green) + "," +
                                     std::to_string((int)s.color.blue) + ")";
            float floodOpacity = ((float)s.color.alpha / 255.0f) * ts.opacity;

            defs << "<feGaussianBlur in=\"SourceAlpha\" stdDeviation=\"" << s.blur.val() * 0.5f
                 << "\" result=\"blur-" << si << "\"/>"
                 << "<feOffset in=\"blur-" << si << "\" dx=\"" << s.x.val() << "\" dy=\""
                 << s.y.val() << "\" result=\"offset-" << si << "\"/>"
                 << "<feFlood flood-color=\"" << floodColor << "\" flood-opacity=\"" << floodOpacity
                 << "\" result=\"color-" << si << "\"/>"
                 << "<feComposite in=\"color-" << si << "\" in2=\"offset-" << si
                 << "\" operator=\"in\" result=\"" << resultName << "\"/>";
        }

        defs << "<feMerge>";
        for (size_t si = 0; si < ts.shadows.size(); ++si) {
            defs << "<feMergeNode in=\"shadow-" << si << "\"/>";
        }
        defs << "<feMergeNode in=\"SourceGraphic\"/>"
             << "</feMerge>"
             << "</filter>";
    }

    const auto &images = render_container.get_used_image_draws();
    for (size_t i = 0; i < images.size(); ++i) {
        const auto &draw = images[i];
        int index = (int)(i + 1);
        if (draw.has_clip) {
            defs << "<clipPath id=\"clip-img-" << index << "\">";
            defs << "<path d=\"" << path_from_rrect(draw.clip_pos, draw.clip_radius) << "\" />";
            defs << "</clipPath>";
        }
        if (draw.layer.repeat != litehtml::background_repeat_no_repeat) {
            auto it = context.imageCache.find(draw.url);
            if (it != context.imageCache.end() && it->second.skImage) {
                std::string dataUrl;
                if (!it->second.data_url.empty() && it->second.data_url.substr(0, 5) == "data:") {
                    dataUrl = it->second.data_url;
                } else {
                    SkBitmap bitmap;
                    bitmap.allocN32Pixels(it->second.skImage->width(),
                                          it->second.skImage->height());
                    SkCanvas bitmapCanvas(bitmap);
                    bitmapCanvas.drawImage(it->second.skImage, 0, 0);
                    dataUrl = bitmapToDataUrl(bitmap);
                }

                float pW = (float)draw.layer.origin_box.width;
                float pH = (float)draw.layer.origin_box.height;

                if (draw.layer.repeat == litehtml::background_repeat_repeat_x) {
                    pH = (float)draw.layer.clip_box.height + 1.0f;
                } else if (draw.layer.repeat == litehtml::background_repeat_repeat_y) {
                    pW = (float)draw.layer.clip_box.width + 1.0f;
                }

                defs << "<pattern id=\"pattern-img-" << index
                     << "\" patternUnits=\"userSpaceOnUse\" x=\"" << draw.layer.origin_box.x
                     << "\" y=\"" << draw.layer.origin_box.y << "\" width=\"" << pW
                     << "\" height=\"" << pH << "\">";
                defs << "<image x=\"0\" y=\"0\" width=\"" << draw.layer.origin_box.width
                     << "\" height=\"" << draw.layer.origin_box.height
                     << "\" preserveAspectRatio=\"none\" href=\"" << dataUrl << "\" />";
                defs << "</pattern>";
            }
        }
    }

    const auto &conics = render_container.get_used_conic_gradients();
    for (size_t i = 0; i < conics.size(); ++i) {
        if (has_radius(conics[i].layer.border_radius)) {
            defs << "<clipPath id=\"clip-gradient-1-" << (i + 1) << "\">";
            defs << "<path d=\""
                 << path_from_rrect(conics[i].layer.border_box, conics[i].layer.border_radius)
                 << "\" />";
            defs << "</clipPath>";
        }
    }
    const auto &radials = render_container.get_used_radial_gradients();
    for (size_t i = 0; i < radials.size(); ++i) {
        if (has_radius(radials[i].layer.border_radius)) {
            defs << "<clipPath id=\"clip-gradient-2-" << (i + 1) << "\">";
            defs << "<path d=\""
                 << path_from_rrect(radials[i].layer.border_box, radials[i].layer.border_radius)
                 << "\" />";
            defs << "</clipPath>";
        }
    }
    const auto &linears = render_container.get_used_linear_gradients();
    for (size_t i = 0; i < linears.size(); ++i) {
        if (has_radius(linears[i].layer.border_radius)) {
            defs << "<clipPath id=\"clip-gradient-3-" << (i + 1) << "\">";
            defs << "<path d=\""
                 << path_from_rrect(linears[i].layer.border_box, linears[i].layer.border_radius)
                 << "\" />";
            defs << "</clipPath>";
        }
    }

    const auto &filters = render_container.get_used_filters();
    for (size_t i = 0; i < filters.size(); ++i) {
        const auto &f = filters[i];
        int index = (int)(i + 1);
        defs << "<filter id=\"filter-" << index
             << "\" x=\"-100%\" y=\"-100%\" width=\"300%\" height=\"300%\">";

        std::string currentIn = "SourceGraphic";
        int resIdx = 0;

        for (const auto &tok : f.tokens) {
            if (tok.type == litehtml::CV_FUNCTION) {
                std::string name = litehtml::lowcase(tok.name);
                auto args = litehtml::parse_comma_separated_list(tok.value);

                if (name == "blur") {
                    if (!args.empty() && !args[0].empty()) {
                        litehtml::css_length len;
                        len.from_token(args[0][0], litehtml::f_length | litehtml::f_positive);
                        float sigma = len.val();
                        if (sigma > 0) {
                            std::string nextIn = "res-" + std::to_string(++resIdx);
                            defs << "<feGaussianBlur in=\"" << currentIn << "\" stdDeviation=\""
                                 << sigma << "\" result=\"" << nextIn << "\"/>";
                            currentIn = nextIn;
                        }
                    }
                } else if (name == "drop-shadow") {
                    if (!args.empty()) {
                        float dx = 0, dy = 0, blur = 0;
                        litehtml::web_color color = litehtml::web_color::black;
                        int ti = 0;
                        for (const auto &t : args[0]) {
                            if (t.type == litehtml::WHITESPACE) continue;
                            if (ti == 0) {
                                litehtml::css_length l;
                                l.from_token(t, litehtml::f_length);
                                dx = l.val();
                            } else if (ti == 1) {
                                litehtml::css_length l;
                                l.from_token(t, litehtml::f_length);
                                dy = l.val();
                            } else if (ti == 2) {
                                litehtml::css_length l;
                                l.from_token(t, litehtml::f_length | litehtml::f_positive);
                                blur = l.val();
                            } else if (ti == 3) {
                                litehtml::parse_color(t, color, nullptr);
                            }
                            ti++;
                        }
                        std::string floodColor = "rgb(" + std::to_string((int)color.red) + "," +
                                                 std::to_string((int)color.green) + "," +
                                                 std::to_string((int)color.blue) + ")";
                        float floodOpacity = (float)color.alpha / 255.0f;

                        std::string nextIn = "res-" + std::to_string(++resIdx);
                        std::string blurRes = "blur-" + std::to_string(resIdx);
                        std::string offsetRes = "offset-" + std::to_string(resIdx);
                        std::string colorRes = "color-" + std::to_string(resIdx);
                        std::string shadowRes = "shadow-" + std::to_string(resIdx);

                        std::string alphaIn = (currentIn == "SourceGraphic")
                                                  ? "SourceAlpha"
                                                  : "alpha-" + std::to_string(resIdx);
                        if (currentIn != "SourceGraphic") {
                            defs << "<feColorMatrix in=\"" << currentIn << "\" type=\"matrix\" "
                                 << "values=\"0 0 0 0 0  0 0 0 0 0  0 0 0 1 0\" result=\""
                                 << alphaIn << "\"/>";
                        }

                        defs << "<feGaussianBlur in=\"" << alphaIn << "\" stdDeviation=\""
                             << blur * 0.5f << "\" result=\"" << blurRes << "\"/>"
                             << "<feOffset in=\"" << blurRes << "\" dx=\"" << dx << "\" dy=\"" << dy
                             << "\" result=\"" << offsetRes << "\"/>"
                             << "<feFlood flood-color=\"" << floodColor << "\" flood-opacity=\""
                             << floodOpacity << "\" result=\"color-" << resIdx << "\"/>"
                             << "<feComposite in=\"color-" << resIdx << "\" in2=\"" << offsetRes
                             << "\" operator=\"in\" result=\"" << shadowRes << "\"/>"
                             << "<feMerge result=\"" << nextIn << "\">"
                             << "<feMergeNode in=\"" << shadowRes << "\"/>"
                             << "<feMergeNode in=\"" << currentIn << "\"/>"
                             << "</feMerge>";
                        currentIn = nextIn;
                    }
                }
            }
        }
        defs << "</filter>";
    }

    const auto &clips = render_container.get_used_clips();
    for (size_t i = 0; i < clips.size(); ++i) {
        const auto &c = clips[i];
        int index = (int)(i + 1);
        defs << "<clipPath id=\"clip-path-" << index << "\">";
        defs << "<path d=\"" << path_from_rrect(c.pos, c.radius) << "\" />";
        defs << "</clipPath>";
    }

    const auto &clipPaths = render_container.get_used_clip_paths();
    for (size_t i = 0; i < clipPaths.size(); ++i) {
        const auto &cp = clipPaths[i];
        int index = (int)(i + 1);
        defs << "<clipPath id=\"adv-clip-path-" << index << "\">";
        SkPath path = container_skia::parse_clip_path(cp.tokens, cp.pos);
        SkString svgPath = SkParsePath::ToSVGString(path);
        defs << "<path d=\"" << svgPath.c_str() << "\" />";
        defs << "</clipPath>";
    }

    const auto &glyphs = render_container.get_used_glyphs();
    for (size_t i = 0; i < glyphs.size(); ++i) {
        defs << "<path id=\"glyph-" << (i + 1) << "\" d=\""
             << SkParsePath::ToSVGString(glyphs[i]).c_str() << "\" />";
    }

    return defs.str();
}

static std::string finalizeSvg(std::string_view svg, SatoruContext &context,
                               const container_skia &container, const RenderOptions &options) {
    std::string result;
    result.reserve(svg.size() + 8192);

    const auto &shadows = container.get_used_shadows();
    const auto &textShadows = container.get_used_text_shadows();
    const auto &images = container.get_used_image_draws();
    const auto &conics = container.get_used_conic_gradients();
    const auto &radials = container.get_used_radial_gradients();
    const auto &linears = container.get_used_linear_gradients();
    const auto &textDraws = container.get_used_text_draws();
    const auto &filters = container.get_used_filters();

    SvgScanner scanner(svg);
    bool defsInjected = false;

    while (!scanner.isAtEnd()) {
        std::string_view text = scanner.scanTo('<');
        result.append(text);
        if (scanner.isAtEnd()) break;

        size_t tagStart = scanner.getPos();
        FastTag tag = scanner.parseTag();
        size_t tagEnd = scanner.getPos();
        std::string_view rawTag = svg.substr(tagStart, tagEnd - tagStart);

        if (!defsInjected) {
            SATORU_LOG_INFO("finalizeSvg: Tag [%.*s] (closing=%d)", (int)tag.name.size(),
                            tag.name.data(), tag.closing);
            if (tag.name == "svg") {
                SATORU_LOG_INFO("finalizeSvg: matched 'svg' exactly. closing=%d, defsInjected=%d",
                                tag.closing, defsInjected);
            }
        }

        if (tag.name.empty() || tag.name[0] == '!' || tag.name[0] == '?') {
            serializeFastTag(result, tag);
            continue;
        }

        bool replaced = false;
        auto magic = tag.getMagicTag();

        if (magic.is_magic) {
            int fullIndex = magic.index;
            if (!magic.is_extended) {
                auto mtag = (satoru::MagicTag)magic.tag_value;
                switch (mtag) {
                    case satoru::MagicTag::Shadow:
                        if (fullIndex > 0 && fullIndex <= (int)shadows.size()) {
                            result.append("<");
                            result.append(tag.name);
                            result.append(" filter=\"url(#shadow-" + std::to_string(fullIndex) +
                                          ")\" fill=\"black\"");
                            for (const auto &a : tag.attrs) {
                                if (a.name != "filter" && a.name != "fill" && a.name != "style" &&
                                    a.name != "fill-opacity") {
                                    result.append(" ");
                                    result.append(a.name);
                                    result.append("=\"");
                                    result.append(a.value);
                                    result.append("\"");
                                }
                            }
                            if (tag.selfClosing) result.append(" /");
                            result.append(">");
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::TextShadow:
                        if (fullIndex > 0 && fullIndex <= (int)textShadows.size()) {
                            const auto &tsh = textShadows[fullIndex - 1];
                            std::string filterId = "text-shadow-" + std::to_string(fullIndex);
                            std::string textColor =
                                "rgb(" + std::to_string((int)tsh.text_color.red) + "," +
                                std::to_string((int)tsh.text_color.green) + "," +
                                std::to_string((int)tsh.text_color.blue) + ")";
                            float opacity = ((float)tsh.text_color.alpha / 255.0f) * tsh.opacity;
                            result.append("<");
                            result.append(tag.name);
                            result.append(" filter=\"url(#" + filterId + ")\" fill=\"" + textColor +
                                          "\" fill-opacity=\"" + std::to_string(opacity) + "\"");
                            for (const auto &a : tag.attrs) {
                                if (a.name != "filter" && a.name != "fill" &&
                                    a.name != "fill-opacity" && a.name != "style") {
                                    result.append(" ");
                                    result.append(a.name);
                                    result.append("=\"");
                                    result.append(a.value);
                                    result.append("\"");
                                }
                            }
                            if (tag.selfClosing) result.append(" /");
                            result.append(">");
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::LayerPush: {
                        float opacity = (float)(fullIndex & 0xFF) / 255.0f;
                        result.append("<g opacity=\"" + std::to_string(opacity) + "\">");
                        replaced = true;
                        break;
                    }
                    case satoru::MagicTag::LayerPop:
                        result.append("</g>");
                        replaced = true;
                        break;
                    case satoru::MagicTag::FilterPush:
                        if (fullIndex > 0 && fullIndex <= (int)filters.size()) {
                            const auto &finfo = filters[fullIndex - 1];
                            result.append("<g filter=\"url(#filter-" + std::to_string(fullIndex) +
                                          ")\"");
                            if (finfo.opacity < 1.0f)
                                result.append(" opacity=\"" + std::to_string(finfo.opacity) + "\"");
                            result.append(">");
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::FilterPop:
                        result.append("</g>");
                        replaced = true;
                        break;
                    case satoru::MagicTag::ClipPush:
                        if (fullIndex > 0 && fullIndex <= (int)container.get_used_clips().size()) {
                            result.append("<g clip-path=\"url(#clip-path-" +
                                          std::to_string(fullIndex) + ")\">");
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::ClipPop:
                        result.append("</g>");
                        replaced = true;
                        break;
                    case satoru::MagicTag::ClipPathPush:
                        if (fullIndex > 0 &&
                            fullIndex <= (int)container.get_used_clip_paths().size()) {
                            result.append("<g clip-path=\"url(#adv-clip-path-" +
                                          std::to_string(fullIndex) + ")\">");
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::ClipPathPop:
                        result.append("</g>");
                        replaced = true;
                        break;
                    case satoru::MagicTag::TextDraw:
                        if (fullIndex > 0 && fullIndex <= (int)textDraws.size()) {
                            processTextDraw(tag, result, textDraws[fullIndex - 1]);
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::GlyphPath:
                        if (fullIndex > 0 &&
                            fullIndex <= (int)container.get_used_glyph_draws().size()) {
                            const auto &drawInfo = container.get_used_glyph_draws()[fullIndex - 1];
                            result.append("<use href=\"#glyph-" +
                                          std::to_string(drawInfo.glyph_index) + "\"");
                            for (const auto &a : tag.attrs) {
                                if (a.name == "x" || a.name == "y" || a.name == "transform") {
                                    result.append(" ");
                                    result.append(a.name);
                                    result.append("=\"");
                                    result.append(a.value);
                                    result.append("\"");
                                }
                            }
                            if (drawInfo.style_tag == (int)satoru::MagicTag::TextDraw &&
                                drawInfo.style_index > 0 &&
                                drawInfo.style_index <= (int)textDraws.size()) {
                                const auto &td = textDraws[drawInfo.style_index - 1];
                                std::string textColor = "rgb(" + std::to_string((int)td.color.red) +
                                                        "," + std::to_string((int)td.color.green) +
                                                        "," + std::to_string((int)td.color.blue) +
                                                        ")";
                                float opacity = ((float)td.color.alpha / 255.0f) * td.opacity;
                                result.append(" fill=\"" + textColor + "\" fill-opacity=\"" +
                                              std::to_string(opacity) + "\"");
                            } else if (drawInfo.style_tag == (int)satoru::MagicTag::TextShadow &&
                                       drawInfo.style_index > 0 &&
                                       drawInfo.style_index <= (int)textShadows.size()) {
                                const auto &tsh = textShadows[drawInfo.style_index - 1];
                                std::string textColor =
                                    "rgb(" + std::to_string((int)tsh.text_color.red) + "," +
                                    std::to_string((int)tsh.text_color.green) + "," +
                                    std::to_string((int)tsh.text_color.blue) + ")";
                                float opacity =
                                    ((float)tsh.text_color.alpha / 255.0f) * tsh.opacity;
                                result.append(" filter=\"url(#text-shadow-" +
                                              std::to_string(drawInfo.style_index) + ")\" fill=\"" +
                                              textColor + "\" fill-opacity=\"" +
                                              std::to_string(opacity) + "\"");
                            }
                            result.append(" />");
                            replaced = true;
                        }
                        break;
                    default:
                        break;
                }
            } else {
                auto mtag = (satoru::MagicTagExtended)magic.tag_value;
                switch (mtag) {
                    case satoru::MagicTagExtended::ImageDraw:
                        if (fullIndex > 0 && fullIndex <= (int)images.size()) {
                            const auto &draw = images[fullIndex - 1];
                            auto it = context.imageCache.find(draw.url);
                            if (it != context.imageCache.end() && it->second.skImage) {
                                std::string dataUrl;
                                if (!it->second.data_url.empty() &&
                                    it->second.data_url.substr(0, 5) == "data:") {
                                    dataUrl = it->second.data_url;
                                } else {
                                    SkBitmap bitmap;
                                    bitmap.allocN32Pixels(it->second.skImage->width(),
                                                          it->second.skImage->height());
                                    SkCanvas bitmapCanvas(bitmap);
                                    bitmapCanvas.drawImage(it->second.skImage, 0, 0);
                                    dataUrl = bitmapToDataUrl(bitmap);
                                }
                                if (draw.layer.repeat == litehtml::background_repeat_no_repeat) {
                                    result.append(
                                        "<image x=\"" + std::to_string(draw.layer.origin_box.x) +
                                        "\" y=\"" + std::to_string(draw.layer.origin_box.y) +
                                        "\" width=\"" +
                                        std::to_string(draw.layer.origin_box.width) +
                                        "\" height=\"" +
                                        std::to_string(draw.layer.origin_box.height) +
                                        "\" preserveAspectRatio=\"none\" href=\"" + dataUrl + "\"");
                                    if (draw.opacity < 1.0f)
                                        result.append(" opacity=\"" + std::to_string(draw.opacity) +
                                                      "\"");
                                    if (draw.has_clip)
                                        result.append(" clip-path=\"url(#clip-img-" +
                                                      std::to_string(fullIndex) + ")\"");
                                    result.append(" />");
                                } else {
                                    result.append(
                                        "<rect x=\"" + std::to_string(draw.layer.clip_box.x) +
                                        "\" y=\"" + std::to_string(draw.layer.clip_box.y) +
                                        "\" width=\"" + std::to_string(draw.layer.clip_box.width) +
                                        "\" height=\"" +
                                        std::to_string(draw.layer.clip_box.height) +
                                        "\" fill=\"url(#pattern-img-" + std::to_string(fullIndex) +
                                        ")\"");
                                    if (draw.opacity < 1.0f)
                                        result.append(" opacity=\"" + std::to_string(draw.opacity) +
                                                      "\"");
                                    if (draw.has_clip)
                                        result.append(" clip-path=\"url(#clip-img-" +
                                                      std::to_string(fullIndex) + ")\"");
                                    result.append(" />");
                                }
                                replaced = true;
                            }
                        }
                        break;
                    case satoru::MagicTagExtended::InlineSvg:
                        if (fullIndex > 0 &&
                            fullIndex <= (int)container.get_used_inline_svgs().size()) {
                            result.append(container.get_used_inline_svgs()[fullIndex - 1]);
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTagExtended::ConicGradient:
                    case satoru::MagicTagExtended::RadialGradient:
                    case satoru::MagicTagExtended::LinearGradient: {
                        SkBitmap bitmap;
                        litehtml::position border_box;
                        litehtml::border_radiuses border_radius;
                        float opacity = 1.0f;
                        bool ok = false;
                        if (mtag == satoru::MagicTagExtended::ConicGradient && fullIndex > 0 &&
                            fullIndex <= (int)conics.size()) {
                            const auto &gradInfo = conics[fullIndex - 1];
                            border_box = gradInfo.layer.border_box;
                            border_radius = gradInfo.layer.border_radius;
                            opacity = gradInfo.opacity;
                            if (border_box.width > 0 && border_box.height > 0) {
                                bitmap.allocN32Pixels((int)border_box.width,
                                                      (int)border_box.height);
                                SkCanvas bitmapCanvas(bitmap);
                                bitmapCanvas.clear(SK_ColorTRANSPARENT);
                                SkPoint center = SkPoint::Make(
                                    (float)gradInfo.gradient.position.x - (float)border_box.x,
                                    (float)gradInfo.gradient.position.y - (float)border_box.y);
                                std::vector<SkColor4f> colors;
                                std::vector<float> pos_vec;
                                for (size_t i = 0; i < gradInfo.gradient.color_points.size(); ++i) {
                                    const auto &stop = gradInfo.gradient.color_points[i];
                                    colors.push_back(
                                        {stop.color.red / 255.0f, stop.color.green / 255.0f,
                                         stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
                                    float offset = stop.offset;
                                    if (i > 0 && offset <= pos_vec.back())
                                        offset = pos_vec.back() + 0.00001f;
                                    pos_vec.push_back(offset);
                                }
                                if (!pos_vec.empty() && pos_vec.back() > 1.0f) {
                                    float max_val = pos_vec.back();
                                    for (auto &p : pos_vec) p /= max_val;
                                    pos_vec.back() = 1.0f;
                                }
                                SkGradient sk_grad(
                                    SkGradient::Colors(SkSpan(colors), SkSpan(pos_vec),
                                                       SkTileMode::kClamp),
                                    SkGradient::Interpolation());
                                SkMatrix matrix;
                                matrix.setRotate(gradInfo.gradient.angle - 90.0f, center.x(),
                                                 center.y());
                                SkPaint p;
                                p.setShader(SkShaders::SweepGradient(center, sk_grad, &matrix));
                                p.setAntiAlias(true);
                                bitmapCanvas.drawRect(SkRect::MakeWH((float)border_box.width,
                                                                     (float)border_box.height),
                                                      p);
                                ok = true;
                            }
                        } else if (mtag == satoru::MagicTagExtended::RadialGradient &&
                                   fullIndex > 0 && fullIndex <= (int)radials.size()) {
                            const auto &gradInfo = radials[fullIndex - 1];
                            border_box = gradInfo.layer.border_box;
                            border_radius = gradInfo.layer.border_radius;
                            opacity = gradInfo.opacity;
                            if (border_box.width > 0 && border_box.height > 0) {
                                bitmap.allocN32Pixels((int)border_box.width,
                                                      (int)border_box.height);
                                SkCanvas bitmapCanvas(bitmap);
                                bitmapCanvas.clear(SK_ColorTRANSPARENT);
                                SkPoint center = SkPoint::Make(
                                    (float)gradInfo.gradient.position.x - (float)border_box.x,
                                    (float)gradInfo.gradient.position.y - (float)border_box.y);
                                float rx = (float)gradInfo.gradient.radius.x,
                                      ry = (float)gradInfo.gradient.radius.y;
                                std::vector<SkColor4f> colors;
                                std::vector<float> pos_vec;
                                for (const auto &stop : gradInfo.gradient.color_points) {
                                    colors.push_back(
                                        {stop.color.red / 255.0f, stop.color.green / 255.0f,
                                         stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
                                    pos_vec.push_back(stop.offset);
                                }
                                SkMatrix matrix;
                                matrix.setScale(1.0f, ry / rx, center.x(), center.y());
                                SkGradient sk_grad(
                                    SkGradient::Colors(SkSpan(colors), SkSpan(pos_vec),
                                                       SkTileMode::kClamp),
                                    SkGradient::Interpolation());
                                SkPaint p;
                                p.setShader(
                                    SkShaders::RadialGradient(center, rx, sk_grad, &matrix));
                                p.setAntiAlias(true);
                                bitmapCanvas.drawRect(SkRect::MakeWH((float)border_box.width,
                                                                     (float)border_box.height),
                                                      p);
                                ok = true;
                            }
                        } else if (mtag == satoru::MagicTagExtended::LinearGradient &&
                                   fullIndex > 0 && fullIndex <= (int)linears.size()) {
                            const auto &gradInfo = linears[fullIndex - 1];
                            border_box = gradInfo.layer.border_box;
                            border_radius = gradInfo.layer.border_radius;
                            opacity = gradInfo.opacity;
                            if (border_box.width > 0 && border_box.height > 0) {
                                bitmap.allocN32Pixels((int)border_box.width,
                                                      (int)border_box.height);
                                SkCanvas bitmapCanvas(bitmap);
                                bitmapCanvas.clear(SK_ColorTRANSPARENT);
                                SkPoint pts[2] = {
                                    SkPoint::Make(
                                        (float)gradInfo.gradient.start.x - (float)border_box.x,
                                        (float)gradInfo.gradient.start.y - (float)border_box.y),
                                    SkPoint::Make(
                                        (float)gradInfo.gradient.end.x - (float)border_box.x,
                                        (float)gradInfo.gradient.end.y - (float)border_box.y)};
                                std::vector<SkColor4f> colors;
                                std::vector<float> pos_vec;
                                for (const auto &stop : gradInfo.gradient.color_points) {
                                    colors.push_back(
                                        {stop.color.red / 255.0f, stop.color.green / 255.0f,
                                         stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
                                    pos_vec.push_back(stop.offset);
                                }
                                SkGradient sk_grad(
                                    SkGradient::Colors(SkSpan(colors), SkSpan(pos_vec),
                                                       SkTileMode::kClamp),
                                    SkGradient::Interpolation());
                                SkPaint p;
                                p.setShader(SkShaders::LinearGradient(pts, sk_grad));
                                p.setAntiAlias(true);
                                bitmapCanvas.drawRect(SkRect::MakeWH((float)border_box.width,
                                                                     (float)border_box.height),
                                                      p);
                                ok = true;
                            }
                        }
                        if (ok) {
                            result.append("<image x=\"" + std::to_string(border_box.x) + "\" y=\"" +
                                          std::to_string(border_box.y) + "\" width=\"" +
                                          std::to_string(border_box.width) + "\" height=\"" +
                                          std::to_string(border_box.height) +
                                          "\" preserveAspectRatio=\"none\" href=\"" +
                                          bitmapToDataUrl(bitmap) + "\"");
                            if (opacity < 1.0f)
                                result.append(" opacity=\"" + std::to_string(opacity) + "\"");
                            if (has_radius(border_radius))
                                result.append(" clip-path=\"url(#clip-gradient-" +
                                              std::to_string((int)mtag) + "-" +
                                              std::to_string(fullIndex) + ")\"");
                            result.append(" />");
                            replaced = true;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        if (!replaced) {
            bool isSvg = tag.isTag("svg");
            if (tag.isTag("image") && !tag.closing) {
                bool hasPreserve = false;
                for (const auto &a : tag.attrs) {
                    if (a.name == "preserveAspectRatio") {
                        hasPreserve = true;
                        break;
                    }
                }
                if (!hasPreserve) {
                    result.append("<image preserveAspectRatio=\"none\"");
                    for (const auto &a : tag.attrs) {
                        result.append(" ");
                        result.append(a.name);
                        result.append("=\"");
                        result.append(a.value);
                        result.append("\"");
                    }
                    if (tag.selfClosing) result.append(" /");
                    result.append(">");
                    replaced = true;
                }
            }
            if (!replaced) {
                serializeFastTag(result, tag);
            }

            if (isSvg && !tag.closing && !defsInjected) {
                SATORU_LOG_INFO("finalizeSvg: Injecting defs into <%.*s>", (int)tag.name.size(),
                                tag.name.data());
                std::string defs = generateDefs(container, context, options);
                SATORU_LOG_INFO("finalizeSvg: Defs size: %d", (int)defs.size());
                result.append("<defs><!--SATORU_DEFS-->");
                result.append(defs);
                result.append("</defs>");
                defsInjected = true;
            }
        }
    }
    return result;
}
}  // namespace

std::string renderDocumentToSvg(SatoruInstance *inst, int width, int height,
                                const RenderOptions &options) {
    if (!inst->doc || !inst->render_container) return "";

    int content_height = (height > 0) ? height : (int)inst->doc->height();
    if (content_height < 1) content_height = 1;

    SkDynamicMemoryWStream stream;
    SkSVGCanvas::Options svg_options;
    if (options.svgTextToPaths) {
        svg_options.flags = SkSVGCanvas::kConvertTextToPaths_Flag;
    } else {
        svg_options.flags = (SkSVGCanvas::Flags)0;
    }
    auto canvas = SkSVGCanvas::Make(SkRect::MakeWH((float)width, (float)content_height), &stream,
                                    svg_options);

    inst->render_container->reset();
    inst->render_container->set_canvas(canvas.get());
    inst->render_container->set_height(content_height);
    inst->render_container->set_tagging(true);
    inst->render_container->set_text_to_paths(options.svgTextToPaths);

    litehtml::position clip(0, 0, width, content_height);
    inst->doc->draw(0, 0, 0, &clip);
    inst->render_container->flush();

    canvas.reset();
    sk_sp<SkData> data = stream.detachAsData();
    std::string_view svg((const char *)data->data(), data->size());

    return finalizeSvg(svg, inst->context, *inst->render_container, options);
}

static void dump_elements_recursive(litehtml::element::ptr el) {
    if (!el) return;
    const char *tag = el->get_tagName();
    if (tag && strcmp(tag, "div") == 0) {
        auto attrs = el->css().dump_get_attrs();
        for (const auto &attr : attrs) {
            SATORU_LOG_INFO("SVG DIV [%s] CSS ATTR: %s = %s", tag, std::get<0>(attr).c_str(),
                            std::get<1>(attr).c_str());
        }
    }
    for (auto &child : el->children()) {
        dump_elements_recursive(child);
    }
}

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context,
                            const char *master_css, const RenderOptions &options) {
    int initial_height = (height > 0) ? height : 3000;
    container_skia container(width, initial_height, nullptr, context, nullptr, false);

    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    auto doc = litehtml::document::createFromString(html, &container, css.c_str());
    if (!doc) return "";
    doc->render(width);

    if (doc->root()) {
        dump_elements_recursive(doc->root());
    }

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    SkDynamicMemoryWStream stream;
    SkSVGCanvas::Options svg_options;
    if (options.svgTextToPaths) {
        svg_options.flags = SkSVGCanvas::kConvertTextToPaths_Flag;
    } else {
        svg_options.flags = (SkSVGCanvas::Flags)0;
    }
    auto canvas = SkSVGCanvas::Make(SkRect::MakeWH((float)width, (float)content_height), &stream,
                                    svg_options);

    container.set_canvas(canvas.get());
    container.set_height(content_height);
    container.set_tagging(true);
    container.set_text_to_paths(options.svgTextToPaths);
    container.reset();

    litehtml::position clip(0, 0, width, content_height);
    doc->draw(0, 0, 0, &clip);
    container.flush();

    canvas.reset();
    sk_sp<SkData> data = stream.detachAsData();
    std::string_view svg((const char *)data->data(), data->size());

    return finalizeSvg(svg, context, container, options);
}
