#include "svg_renderer.h"

#include <litehtml/master_css.h>

#include <cstdio>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
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

struct TagInfo {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attrs;
    bool selfClosing = false;
    bool closing = false;

    std::string getAttr(const std::string &attrName) const {
        for (const auto &a : attrs) {
            if (a.first == attrName) return a.second;
        }
        return "";
    }

    void setAttr(const std::string &attrName, const std::string &val) {
        for (auto &a : attrs) {
            if (a.first == attrName) {
                a.second = val;
                return;
            }
        }
        attrs.push_back({attrName, val});
    }

    void removeAttr(const std::string &attrName) {
        attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
                                   [&](const std::pair<std::string, std::string> &a) {
                                       return a.first == attrName;
                                   }),
                    attrs.end());
    }

    struct MagicResult {
        satoru::DecodedMagicTag tag;
        bool isAttr = true;
    };

    MagicResult getMagicTag() const {
        std::string colorVal = getAttr("fill");
        bool isAttr = true;

        if (colorVal.empty()) {
            std::string style = getAttr("style");
            size_t f = style.find("fill:");
            if (f != std::string::npos) {
                size_t vStart = f + 5;
                size_t vEnd = style.find_first_of(";\"", vStart);
                colorVal = style.substr(
                    vStart, (vEnd == std::string::npos ? std::string::npos : vEnd - vStart));
                isAttr = false;
            }
        }

        if (colorVal.empty()) return {};

        // マジックカラー候補の高速チェック (# または rgb)
        if (colorVal.empty() || (colorVal[0] != '#' && colorVal.find("rgb") == std::string::npos)) {
            return {};
        }

        int r = -1, g = -1, b = -1;
        if (colorVal.size() >= 7 && colorVal[0] == '#') {
            try {
                r = std::stoi(colorVal.substr(1, 2), nullptr, 16);
                g = std::stoi(colorVal.substr(3, 2), nullptr, 16);
                b = std::stoi(colorVal.substr(5, 2), nullptr, 16);
            } catch (...) {
                return {};
            }
        } else if (colorVal.find("rgb(") == 0) {
            if (sscanf(colorVal.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) != 3) {
                sscanf(colorVal.c_str(), "rgb(%d, %d, %d)", &r, &g, &b);
            }
        }

        if (r >= 0 && g >= 0 && b >= 0) {
            return {satoru::decode_magic_color((uint8_t)r, (uint8_t)g, (uint8_t)b), isAttr};
        }
        return {};
    }
};

static TagInfo parseTag(const std::string &content) {
    TagInfo info;
    if (content.empty()) return info;
    size_t start = 0;
    while (start < content.size() && isspace(content[start])) start++;
    if (start < content.size() && content[start] == '/') {
        info.closing = true;
        start++;
    }
    size_t nameEnd = content.find_first_of(" \t\n\r/", start);
    if (nameEnd == std::string::npos) {
        info.name = content.substr(start);
    } else {
        info.name = content.substr(start, nameEnd - start);
        size_t p = nameEnd;
        while (p < content.size()) {
            while (p < content.size() && isspace(content[p])) p++;
            if (p >= content.size()) break;
            if (content[p] == '/') {
                info.selfClosing = true;
                break;
            }
            size_t eq = content.find('=', p);
            if (eq == std::string::npos) break;
            std::string attrName = content.substr(p, eq - p);
            p = eq + 1;
            while (p < content.size() && isspace(content[p])) p++;
            if (p >= content.size()) break;
            char quote = content[p];
            if (quote == '"' || quote == '\'') {
                p++;
                size_t valEnd = content.find(quote, p);
                if (valEnd == std::string::npos) break;
                info.attrs.push_back({attrName, content.substr(p, valEnd - p)});
                p = valEnd + 1;
            } else {
                size_t valEnd = content.find_first_of(" \t\n\r/", p);
                if (valEnd == std::string::npos) {
                    info.attrs.push_back({attrName, content.substr(p)});
                    break;
                }
                info.attrs.push_back({attrName, content.substr(p, valEnd - p)});
                p = valEnd;
            }
        }
    }
    return info;
}

static std::string serializeTag(const TagInfo &info) {
    if (info.name.empty()) return "";
    std::string s = "<";
    if (info.closing) s += "/";
    s += info.name;
    for (const auto &attr : info.attrs) {
        s += " " + attr.first + "=\"" + attr.second + "\"";
    }
    if (info.selfClosing) s += " /";
    s += ">";
    return s;
}

static void processTextDraw(TagInfo &info, const text_draw_info &drawInfo) {
    info.removeAttr("font-weight");
    info.removeAttr("font-style");
    info.removeAttr("fill-opacity");

    info.setAttr("font-weight", std::to_string(drawInfo.weight));
    info.setAttr("font-style", drawInfo.italic ? "italic" : "normal");

    std::string textColor = "rgb(" + std::to_string((int)drawInfo.color.red) + "," +
                            std::to_string((int)drawInfo.color.green) + "," +
                            std::to_string((int)drawInfo.color.blue) + ")";
    float opacity = ((float)drawInfo.color.alpha / 255.0f) * drawInfo.opacity;

    info.setAttr("fill", textColor);
    info.setAttr("fill-opacity", std::to_string(opacity));

    // Also fix potential stroke if it was used for boldness or decoration
    for (auto &attr : info.attrs) {
        if (attr.first == "stroke") {
            attr.second = textColor;
        }
    }
}

void processTags(std::string &svg, SatoruContext &context, const container_skia &container) {
    std::string result;
    result.reserve(svg.size() + 4096);

    const auto &shadows = container.get_used_shadows();
    const auto &textShadows = container.get_used_text_shadows();
    const auto &images = container.get_used_image_draws();
    const auto &conics = container.get_used_conic_gradients();
    const auto &radials = container.get_used_radial_gradients();
    const auto &linears = container.get_used_linear_gradients();
    const auto &textDraws = container.get_used_text_draws();
    const auto &filters = container.get_used_filters();

    size_t lastPos = 0;
    while (lastPos < svg.size()) {
        size_t tagStart = svg.find('<', lastPos);
        if (tagStart == std::string::npos) {
            result.append(svg.substr(lastPos));
            break;
        }

        result.append(svg.substr(lastPos, tagStart - lastPos));

        size_t tagEnd = svg.find('>', tagStart);
        if (tagEnd == std::string::npos) {
            result.append(svg.substr(tagStart));
            break;
        }

        std::string tagContent = svg.substr(tagStart + 1, tagEnd - tagStart - 1);
        if (tagContent.empty() || tagContent[0] == '!' || tagContent[0] == '?') {
            result.append(svg.substr(tagStart, tagEnd - tagStart + 1));
            lastPos = tagEnd + 1;
            continue;
        }

        // 高速フィルタ: fill または style 属性がないタグはマジックタグではないためスキップ
        if (tagContent.find("fill") == std::string::npos &&
            tagContent.find("style") == std::string::npos) {
            result.append(svg.substr(tagStart, tagEnd - tagStart + 1));
            lastPos = tagEnd + 1;
            continue;
        }

        TagInfo info = parseTag(tagContent);
        auto magic = info.getMagicTag();
        bool replaced = false;

        if (magic.tag.is_magic) {
            int fullIndex = magic.tag.index;
            bool isAttr = magic.isAttr;

            if (!magic.tag.is_extended) {
                auto tag = (satoru::MagicTag)magic.tag.tag_value;
                switch (tag) {
                    case satoru::MagicTag::Shadow:
                        if (fullIndex > 0 && fullIndex <= (int)shadows.size()) {
                            std::string filterId = "shadow-" + std::to_string(fullIndex);
                            if (isAttr) {
                                info.setAttr("filter", "url(#" + filterId + ")");
                                info.setAttr("fill", "black");
                            } else {
                                std::string style = info.getAttr("style");
                                size_t f = style.find("fill:");
                                if (f != std::string::npos) {
                                    size_t vEnd = style.find_first_of(";\"", f + 5);
                                    style.replace(
                                        f, (vEnd == std::string::npos ? std::string::npos : vEnd - f),
                                        "fill:black");
                                }
                                info.setAttr("style", style + ";filter:url(#" + filterId + ")");
                            }
                            result.append(serializeTag(info));
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::TextShadow:
                        if (fullIndex > 0 && fullIndex <= (int)textShadows.size()) {
                            const auto &tsh = textShadows[fullIndex - 1];
                            std::string filterId = "text-shadow-" + std::to_string(fullIndex);
                            std::string textColor = "rgb(" + std::to_string((int)tsh.text_color.red) +
                                                    "," + std::to_string((int)tsh.text_color.green) +
                                                    "," + std::to_string((int)tsh.text_color.blue) +
                                                    ")";
                            float opacity = ((float)tsh.text_color.alpha / 255.0f) * tsh.opacity;
                            if (isAttr) {
                                info.setAttr("filter", "url(#" + filterId + ")");
                                info.setAttr("fill", textColor);
                                info.setAttr("fill-opacity", std::to_string(opacity));
                            } else {
                                info.setAttr("style", "filter:url(#" + filterId + ");fill:" +
                                                          textColor +
                                                          ";fill-opacity:" + std::to_string(opacity));
                            }
                            result.append(serializeTag(info));
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
                            std::stringstream ss;
                            ss << "<g filter=\"url(#filter-" << fullIndex << ")\"";
                            if (finfo.opacity < 1.0f) ss << " opacity=\"" << finfo.opacity << "\"";
                            ss << ">";
                            result.append(ss.str());
                            replaced = true;
                        }
                        break;
                    case satoru::MagicTag::FilterPop:
                        result.append("</g>");
                        replaced = true;
                        break;
                    case satoru::MagicTag::TextDraw:
                        if (fullIndex > 0 && fullIndex <= (int)textDraws.size()) {
                            processTextDraw(info, textDraws[fullIndex - 1]);
                            result.append(serializeTag(info));
                            replaced = true;
                        }
                        break;
                    default:
                        break;
                }
            } else {
                auto tag = (satoru::MagicTagExtended)magic.tag.tag_value;
                switch (tag) {
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
                                std::stringstream ss;
                                if (draw.layer.repeat == litehtml::background_repeat_no_repeat) {
                                    ss << "<image x=\"" << draw.layer.origin_box.x << "\" y=\""
                                       << draw.layer.origin_box.y << "\" width=\""
                                       << draw.layer.origin_box.width << "\" height=\""
                                       << draw.layer.origin_box.height
                                       << "\" preserveAspectRatio=\"none\" href=\"" << dataUrl << "\"";
                                    if (draw.opacity < 1.0f)
                                        ss << " opacity=\"" << draw.opacity << "\"";
                                    if (draw.has_clip) {
                                        ss << " clip-path=\"url(#clip-img-" << fullIndex << ")\"";
                                    }
                                    ss << " />";
                                } else {
                                    ss << "<rect x=\"" << draw.layer.clip_box.x << "\" y=\""
                                       << draw.layer.clip_box.y << "\" width=\""
                                       << draw.layer.clip_box.width << "\" height=\""
                                       << draw.layer.clip_box.height << "\" fill=\"url(#pattern-img-"
                                       << fullIndex << ")\"";
                                    if (draw.opacity < 1.0f)
                                        ss << " opacity=\"" << draw.opacity << "\"";
                                    if (draw.has_clip)
                                        ss << " clip-path=\"url(#clip-img-" << fullIndex << ")\"";
                                    ss << " />";
                                }
                                result.append(ss.str());
                                replaced = true;
                            }
                        }
                        break;
                    case satoru::MagicTagExtended::InlineSvg:
                        if (fullIndex > 0 && fullIndex <= (int)container.get_used_inline_svgs().size()) {
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

                        if (tag == satoru::MagicTagExtended::ConicGradient && fullIndex > 0 &&
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
                        } else if (tag == satoru::MagicTagExtended::RadialGradient &&
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
                        } else if (tag == satoru::MagicTagExtended::LinearGradient &&
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
                            std::stringstream ss;
                            ss << "<image x=\"" << border_box.x << "\" y=\"" << border_box.y
                               << "\" width=\"" << border_box.width << "\" height=\""
                               << border_box.height << "\" preserveAspectRatio=\"none\" href=\""
                               << bitmapToDataUrl(bitmap) << "\"";
                            if (opacity < 1.0f) ss << " opacity=\"" << opacity << "\"";
                            if (has_radius(border_radius))
                                ss << " clip-path=\"url(#clip-gradient-" << (int)tag << "-"
                                   << fullIndex << ")\"";
                            ss << " />";
                            result.append(ss.str());
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
            result.append(svg.substr(tagStart, tagEnd - tagStart + 1));
        }
        lastPos = tagEnd + 1;
    }

    svg = std::move(result);
}


static void appendDefs(std::string &svg, const container_skia &render_container,
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

    std::string defsStr = defs.str();
    if (!defsStr.empty()) {
        size_t svgStart = svg.find("<svg");
        if (svgStart != std::string::npos) {
            size_t tagEnd = svg.find(">", svgStart);
            if (tagEnd != std::string::npos) svg.insert(tagEnd + 1, "<defs>" + defsStr + "</defs>");
        }
    }
}
}  // namespace

std::string renderDocumentToSvg(SatoruInstance *inst, int width, int height,
                                const RenderOptions &options) {
    if (!inst->doc || !inst->render_container) return "";

    // We rely on external layout (inst->doc->render(width) or deserialized layout).
    // Height might be 0 if layout was skipped or not provided.
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

    // Reuse container but update canvas and height
    inst->render_container->reset();
    inst->render_container->set_canvas(canvas.get());
    inst->render_container->set_height(content_height);
    inst->render_container->set_tagging(true);
    inst->render_container->set_text_to_paths(options.svgTextToPaths);

    litehtml::position clip(0, 0, width, content_height);
    inst->doc->draw(0, 0, 0, &clip);
    inst->render_container->flush();

    canvas.reset();  // Flush
    sk_sp<SkData> data = stream.detachAsData();
    std::string svg((const char *)data->data(), data->size());

    processTags(svg, inst->context, *inst->render_container);

    // Ensure all <image> elements have preserveAspectRatio="none"
    size_t imgPos = 0;
    while ((imgPos = svg.find("<image", imgPos)) != std::string::npos) {
        size_t endPos = svg.find(">", imgPos);
        if (endPos != std::string::npos) {
            std::string tag = svg.substr(imgPos, endPos - imgPos);
            if (tag.find("preserveAspectRatio") == std::string::npos) {
                size_t insertPos = endPos;
                if (endPos > 0 && svg[endPos - 1] == '/') {
                    insertPos--;
                }
                svg.insert(insertPos, " preserveAspectRatio=\"none\" ");
            }
        }
        imgPos = endPos;
    }

    appendDefs(svg, *inst->render_container, inst->context, options);

    return svg;
}

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context,
                            const char *master_css, const RenderOptions &options) {
    int initial_height = (height > 0) ? height : 32767;
    container_skia container(width, initial_height, nullptr, context, nullptr, false);

    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    auto doc = litehtml::document::createFromString(html, &container, css.c_str());
    if (!doc) return "";
    doc->render(width);

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
    container.reset();  // Clear any state from measurement if necessary

    litehtml::position clip(0, 0, width, content_height);
    doc->draw(0, 0, 0, &clip);
    container.flush();

    canvas.reset();
    sk_sp<SkData> data = stream.detachAsData();
    std::string svg((const char *)data->data(), data->size());

    processTags(svg, context, container);

    // Ensure all <image> elements have preserveAspectRatio="none"
    size_t imgPos = 0;
    while ((imgPos = svg.find("<image", imgPos)) != std::string::npos) {
        size_t endPos = svg.find(">", imgPos);
        if (endPos != std::string::npos) {
            std::string tag = svg.substr(imgPos, endPos - imgPos);
            if (tag.find("preserveAspectRatio") == std::string::npos) {
                size_t insertPos = endPos;
                if (endPos > 0 && svg[endPos - 1] == '/') {
                    insertPos--;
                }
                svg.insert(insertPos, " preserveAspectRatio=\"none\" ");
            }
        }
        imgPos = endPos;
    }

    appendDefs(svg, container, context, options);

    return svg;
}
