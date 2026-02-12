#include "svg_renderer.h"

#include <litehtml/master_css.h>

#include <cstdio>
#include <iomanip>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/container_skia.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkStream.h"
#include "include/effects/SkGradient.h"
#include "include/encode/SkPngEncoder.h"
#include "include/svg/SkSVGCanvas.h"
#include "utils/skia_utils.h"

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
    std::stringstream ss;
    float x = (float)pos.x, y = (float)pos.y, w = (float)pos.width, h = (float)pos.height;
    ss << "M" << x + r.top_left_x << "," << y << " L" << x + w - r.top_right_x << "," << y;
    if (r.top_right_x > 0 || r.top_right_y > 0)
        ss << " A " << r.top_right_x << " " << r.top_right_y << " 0 0 1 " << x + w << " "
           << y + r.top_right_y;
    ss << " L" << x + w << "," << y + h - r.bottom_right_y;
    if (r.bottom_right_x > 0 || r.bottom_right_y > 0)
        ss << " A " << r.bottom_right_x << " " << r.bottom_right_y << " 0 0 1 "
           << x + w - r.bottom_right_x << " " << y + h;
    ss << " L" << x + r.bottom_left_x << "," << y + h;
    if (r.bottom_left_x > 0 || r.bottom_left_y > 0)
        ss << " A " << r.bottom_left_x << " " << r.bottom_left_y << " 0 0 1 " << x << " "
           << y + h - r.bottom_left_y;
    ss << " L" << x << "," << y + r.top_left_y;
    if (r.top_left_x > 0 || r.top_left_y > 0)
        ss << " A " << r.top_left_x << " " << r.top_left_y << " 0 0 1 " << x + r.top_left_x << " "
           << y;
    ss << " Z";
    return ss.str();
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

    size_t lastPos = 0;
    while (lastPos < svg.size()) {
        size_t posFillAttr = svg.find("fill=\"", lastPos);
        size_t posFillStyle = svg.find("fill:", lastPos);
        size_t pos = std::min(posFillAttr, posFillStyle);

        if (pos == std::string::npos) break;

        result.append(svg, lastPos, pos - lastPos);

        bool isAttr = (pos == posFillAttr);
        size_t valStart = pos + (isAttr ? 6 : 5);
        size_t valEnd = std::string::npos;

        if (isAttr) {
            valEnd = svg.find('"', valStart);
        } else {
            valEnd = svg.find_first_of(";\"", valStart);
        }

        if (valEnd == std::string::npos) {
            result.append(svg, pos, (isAttr ? 6 : 5));
            lastPos = valStart;
            continue;
        }

        std::string colorVal = svg.substr(valStart, valEnd - valStart);
        bool replaced = false;

        int r = -1, g = -1, b = -1;
        if (colorVal.size() >= 7 && colorVal[0] == '#') {
            try {
                r = std::stoi(colorVal.substr(1, 2), nullptr, 16);
                g = std::stoi(colorVal.substr(3, 2), nullptr, 16);
                b = std::stoi(colorVal.substr(5, 2), nullptr, 16);
            } catch (...) {
            }
        } else if (colorVal.find("rgb(") == 0) {
            sscanf(colorVal.c_str(), "rgb(%d,%d,%d)", &r, &g, &b);
        }

        if (r != -1) {
            if (r == 0 && g == 1 && b > 0 && b <= (int)shadows.size()) {
                std::string filterId = "shadow-" + std::to_string(b);
                if (isAttr) {
                    result.append("filter=\"url(#" + filterId + ")\" fill=\"black\"");
                } else {
                    result.append("filter:url(#" + filterId + ");fill:black");
                }
                lastPos = valEnd + (isAttr ? 1 : 0);
                replaced = true;
            } else if (r == 0 && g == 2 && b > 0 && b <= (int)textShadows.size()) {
                const auto &info = textShadows[b - 1];
                std::string filterId = "text-shadow-" + std::to_string(b);
                std::string textColor = "rgb(" + std::to_string((int)info.text_color.red) + "," +
                                        std::to_string((int)info.text_color.green) + "," +
                                        std::to_string((int)info.text_color.blue) + ")";
                float opacity = (float)info.text_color.alpha / 255.0f;

                if (isAttr) {
                    result.append("filter=\"url(#" + filterId + ")\" fill=\"" + textColor +
                                  "\" fill-opacity=\"" + std::to_string(opacity) + "\"");
                } else {
                    result.append("filter:url(#" + filterId + ");fill:" + textColor +
                                  ";fill-opacity:" + std::to_string(opacity));
                }
                lastPos = valEnd + (isAttr ? 1 : 0);
                replaced = true;
            } else if (r == 1 && g == 0 && b > 0 && b <= (int)images.size()) {
                size_t elementStart = svg.rfind('<', pos);
                size_t elementEnd = svg.find("/>", valEnd);
                if (elementStart != std::string::npos && elementEnd != std::string::npos) {
                    const auto &draw = images[b - 1];
                    auto it = context.imageCache.find(draw.url);
                    if (it != context.imageCache.end() && it->second.skImage) {
                        SkBitmap bitmap;
                        bitmap.allocN32Pixels(it->second.skImage->width(),
                                              it->second.skImage->height());
                        SkCanvas bitmapCanvas(bitmap);
                        bitmapCanvas.drawImage(it->second.skImage, 0, 0);
                        std::string dataUrl = bitmapToDataUrl(bitmap);

                        std::stringstream ss;
                        if (draw.layer.repeat == litehtml::background_repeat_no_repeat) {
                            ss << "<image x=\"" << draw.layer.origin_box.x << "\" y=\""
                               << draw.layer.origin_box.y << "\" width=\""
                               << draw.layer.origin_box.width << "\" height=\""
                               << draw.layer.origin_box.height << "\" href=\"" << dataUrl << "\"";
                            if (draw.opacity < 1.0f) {
                                ss << " opacity=\"" << draw.opacity << "\"";
                            }
                            if (has_radius(draw.layer.border_radius)) {
                                ss << " clip-path=\"url(#clip-img-" << b << ")\"";
                            }
                            ss << " />";
                        } else {
                            ss << "<rect x=\"" << draw.layer.clip_box.x << "\" y=\""
                               << draw.layer.clip_box.y << "\" width=\""
                               << draw.layer.clip_box.width << "\" height=\""
                               << draw.layer.clip_box.height << "\" fill=\"url(#pattern-img-" << b
                               << ")\"";
                            if (draw.opacity < 1.0f) {
                                ss << " opacity=\"" << draw.opacity << "\"";
                            }
                            if (has_radius(draw.layer.border_radius)) {
                                ss << " clip-path=\"url(#clip-img-" << b << ")\"";
                            }
                            ss << " />";
                        }

                        result.erase(result.size() - (pos - elementStart));
                        result.append(ss.str());
                        lastPos = elementEnd + 2;
                        replaced = true;
                    }
                }
            } else if (r == 1 && (g == 1 || g == 2 || g == 3)) {
                size_t elementStart = svg.rfind('<', pos);
                size_t elementEnd = svg.find("/>", valEnd);
                if (elementStart != std::string::npos && elementEnd != std::string::npos) {
                    SkBitmap bitmap;
                    litehtml::position border_box;
                    litehtml::border_radiuses border_radius;
                    bool ok = false;

                    if (g == 1 && b > 0 && b <= (int)conics.size()) {
                        const auto &info = conics[b - 1];
                        border_box = info.layer.border_box;
                        border_radius = info.layer.border_radius;
                        if (border_box.width > 0 && border_box.height > 0) {
                            bitmap.allocN32Pixels((int)border_box.width, (int)border_box.height);
                            SkCanvas bitmapCanvas(bitmap);
                            bitmapCanvas.clear(SK_ColorTRANSPARENT);
                            SkPoint center = SkPoint::Make(
                                (float)info.gradient.position.x - (float)border_box.x,
                                (float)info.gradient.position.y - (float)border_box.y);
                            std::vector<SkColor4f> colors;
                            std::vector<float> pos_vec;
                            for (size_t i = 0; i < info.gradient.color_points.size(); ++i) {
                                const auto &stop = info.gradient.color_points[i];
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
                            SkGradient sk_grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos_vec),
                                                                  SkTileMode::kClamp),
                                               SkGradient::Interpolation());
                            SkMatrix matrix;
                            matrix.setRotate(info.gradient.angle - 90.0f, center.x(), center.y());
                            SkPaint p;
                            p.setShader(SkShaders::SweepGradient(center, sk_grad, &matrix));
                            p.setAntiAlias(true);
                            bitmapCanvas.drawRect(
                                SkRect::MakeWH((float)border_box.width, (float)border_box.height),
                                p);
                            ok = true;
                        }
                    } else if (g == 2 && b > 0 && b <= (int)radials.size()) {
                        const auto &info = radials[b - 1];
                        border_box = info.layer.border_box;
                        border_radius = info.layer.border_radius;
                        if (border_box.width > 0 && border_box.height > 0) {
                            bitmap.allocN32Pixels((int)border_box.width, (int)border_box.height);
                            SkCanvas bitmapCanvas(bitmap);
                            bitmapCanvas.clear(SK_ColorTRANSPARENT);
                            SkPoint center = SkPoint::Make(
                                (float)info.gradient.position.x - (float)border_box.x,
                                (float)info.gradient.position.y - (float)border_box.y);
                            float rx = (float)info.gradient.radius.x,
                                  ry = (float)info.gradient.radius.y;
                            std::vector<SkColor4f> colors;
                            std::vector<float> pos_vec;
                            for (const auto &stop : info.gradient.color_points) {
                                colors.push_back(
                                    {stop.color.red / 255.0f, stop.color.green / 255.0f,
                                     stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
                                pos_vec.push_back(stop.offset);
                            }
                            SkMatrix matrix;
                            matrix.setScale(1.0f, ry / rx, center.x(), center.y());
                            SkGradient sk_grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos_vec),
                                                                  SkTileMode::kClamp),
                                               SkGradient::Interpolation());
                            SkPaint p;
                            p.setShader(SkShaders::RadialGradient(center, rx, sk_grad, &matrix));
                            p.setAntiAlias(true);
                            bitmapCanvas.drawRect(
                                SkRect::MakeWH((float)border_box.width, (float)border_box.height),
                                p);
                            ok = true;
                        }
                    } else if (g == 3 && b > 0 && b <= (int)linears.size()) {
                        const auto &info = linears[b - 1];
                        border_box = info.layer.border_box;
                        border_radius = info.layer.border_radius;
                        if (border_box.width > 0 && border_box.height > 0) {
                            bitmap.allocN32Pixels((int)border_box.width, (int)border_box.height);
                            SkCanvas bitmapCanvas(bitmap);
                            bitmapCanvas.clear(SK_ColorTRANSPARENT);
                            SkPoint pts[2] = {
                                SkPoint::Make((float)info.gradient.start.x - (float)border_box.x,
                                              (float)info.gradient.start.y - (float)border_box.y),
                                SkPoint::Make((float)info.gradient.end.x - (float)border_box.x,
                                              (float)info.gradient.end.y - (float)border_box.y)};
                            std::vector<SkColor4f> colors;
                            std::vector<float> pos_vec;
                            for (const auto &stop : info.gradient.color_points) {
                                colors.push_back(
                                    {stop.color.red / 255.0f, stop.color.green / 255.0f,
                                     stop.color.blue / 255.0f, stop.color.alpha / 255.0f});
                                pos_vec.push_back(stop.offset);
                            }
                            SkGradient sk_grad(SkGradient::Colors(SkSpan(colors), SkSpan(pos_vec),
                                                                  SkTileMode::kClamp),
                                               SkGradient::Interpolation());
                            SkPaint p;
                            p.setShader(SkShaders::LinearGradient(pts, sk_grad));
                            p.setAntiAlias(true);
                            bitmapCanvas.drawRect(
                                SkRect::MakeWH((float)border_box.width, (float)border_box.height),
                                p);
                            ok = true;
                        }
                    }

                    if (ok) {
                        std::stringstream ss;
                        ss << "<image x=\"" << border_box.x << "\" y=\"" << border_box.y
                           << "\" width=\"" << border_box.width << "\" height=\""
                           << border_box.height << "\" href=\"" << bitmapToDataUrl(bitmap) << "\"";
                        
                        float opacity = 1.0f;
                        if (g == 1) opacity = conics[b - 1].opacity;
                        else if (g == 2) opacity = radials[b - 1].opacity;
                        else if (g == 3) opacity = linears[b - 1].opacity;

                        if (opacity < 1.0f) {
                            ss << " opacity=\"" << opacity << "\"";
                        }

                        if (has_radius(border_radius)) {
                            ss << " clip-path=\"url(#clip-gradient-" << g << "-" << b << ")\"";
                        }
                        ss << " />";
                        result.erase(result.size() - (pos - elementStart));
                        result.append(ss.str());
                        lastPos = elementEnd + 2;
                        replaced = true;
                    }
                }
            }
        }

        if (!replaced) {
            result.append(svg, pos, valEnd + (isAttr ? 1 : 0) - pos);
            lastPos = valEnd + (isAttr ? 1 : 0);
        }
    }

    result.append(svg, lastPos, std::string::npos);
    svg = std::move(result);
}
}  // namespace

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context,
                            const char *master_css) {
    container_skia measure_container(width, height > 0 ? height : 1000, nullptr, context, nullptr,
                                     false);
    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    auto doc = litehtml::document::createFromString(html, &measure_container, css.c_str());
    if (!doc) return "";
    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    SkDynamicMemoryWStream stream;
    SkSVGCanvas::Options svg_options;
    svg_options.flags = SkSVGCanvas::kConvertTextToPaths_Flag;
    auto canvas = SkSVGCanvas::Make(SkRect::MakeWH((float)width, (float)content_height), &stream,
                                    svg_options);

    container_skia render_container(width, content_height, canvas.get(), context, nullptr, true);
    auto render_doc = litehtml::document::createFromString(html, &render_container, css.c_str());
    render_doc->render(width);

    litehtml::position clip(0, 0, width, content_height);
    render_doc->draw(0, 0, 0, &clip);

    canvas.reset();
    sk_sp<SkData> data = stream.detachAsData();
    std::string svg((const char *)data->data(), data->size());

    processTags(svg, context, render_container);

    std::stringstream defs;
    const auto &shadows = render_container.get_used_shadows();
    for (size_t i = 0; i < shadows.size(); ++i) {
        const auto &s = shadows[i];
        int index = (int)(i + 1);

        defs << "<filter id=\"shadow-" << index
             << "\" x=\"-100%\" y=\"-100%\" width=\"300%\" height=\"300%\">";

        std::string floodColor = "rgb(" + std::to_string((int)s.color.red) + "," +
                                 std::to_string((int)s.color.green) + "," +
                                 std::to_string((int)s.color.blue) + ")";
        float floodOpacity = (float)s.color.alpha / 255.0f;

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
                 << "<feOffset dx=\"" << s.x << "\" dy=\"" << s.y << "\" result=\"offsetblur\"/>"
                 << "<feFlood flood-color=\"" << floodColor << "\" flood-opacity=\"" << floodOpacity
                 << "\"/>"
                 << "<feComposite in2=\"offsetblur\" operator=\"in\"/>"
                 << "<feMerge><feMergeNode/></feMerge>";
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
            float floodOpacity = (float)s.color.alpha / 255.0f;

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
        if (has_radius(draw.layer.border_radius)) {
            defs << "<clipPath id=\"clip-img-" << index << "\">";
            defs << "<path d=\"" << path_from_rrect(draw.layer.border_box, draw.layer.border_radius)
                 << "\" />";
            defs << "</clipPath>";
        }
        if (draw.layer.repeat != litehtml::background_repeat_no_repeat) {
            auto it = context.imageCache.find(draw.url);
            if (it != context.imageCache.end() && it->second.skImage) {
                SkBitmap bitmap;
                bitmap.allocN32Pixels(it->second.skImage->width(), it->second.skImage->height());
                SkCanvas bitmapCanvas(bitmap);
                bitmapCanvas.drawImage(it->second.skImage, 0, 0);
                std::string dataUrl = bitmapToDataUrl(bitmap);

                float pW = (float)draw.layer.origin_box.width;
                float pH = (float)draw.layer.origin_box.height;

                // SVG patterns repeat by default based on width/height.
                // To prevent repeating in one direction, we set the pattern size to be
                // larger than the clip area in that direction.
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
                     << "\" height=\"" << draw.layer.origin_box.height << "\" href=\"" << dataUrl
                     << "\" />";
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

    std::string defsStr = defs.str();
    if (!defsStr.empty()) {
        size_t svgStart = svg.find("<svg");
        if (svgStart != std::string::npos) {
            size_t tagEnd = svg.find(">", svgStart);
            if (tagEnd != std::string::npos) svg.insert(tagEnd + 1, "<defs>" + defsStr + "</defs>");
        }
    }

    return svg;
}
