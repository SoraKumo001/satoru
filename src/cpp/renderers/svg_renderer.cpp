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

#include "container_skia.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkStream.h"
#include "include/effects/SkGradient.h"
#include "include/encode/SkPngEncoder.h"
#include "include/svg/SkSVGCanvas.h"
#include "../utils/utils.h"

namespace {
static std::string bitmapToDataUrl(const SkBitmap &bitmap) {
    SkDynamicMemoryWStream stream;
    if (!SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) return "";
    sk_sp<SkData> data = stream.detachAsData();
    return "data:image/png;base64," + base64_encode((const uint8_t *)data->data(), data->size());
}
}  // namespace

std::string renderHtmlToSvg(const char *html, int width, int height, SatoruContext &context, const char* master_css) {
    // 1. First pass: Layout to determine height
    container_skia measure_container(width, height > 0 ? height : 1000, nullptr, context, nullptr, false);
    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";
    
    auto doc = litehtml::document::createFromString(html, &measure_container, css.c_str());
    if (!doc) return "";
    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    // 2. Second pass: Actual rendering to SVG
    SkDynamicMemoryWStream stream;
    SkSVGCanvas::Options svg_options;
    svg_options.flags = SkSVGCanvas::kConvertTextToPaths_Flag;
    auto canvas = SkSVGCanvas::Make(SkRect::MakeWH((float)width, (float)content_height), &stream, svg_options);

    container_skia render_container(width, content_height, canvas.get(), context, nullptr, true);
    auto render_doc = litehtml::document::createFromString(html, &render_container, css.c_str());
    render_doc->render(width);

    litehtml::position clip(0, 0, width, content_height);
    render_doc->draw(0, 0, 0, &clip);

    // Canvas must be destroyed to flush SVG to stream
    canvas.reset();

    sk_sp<SkData> data = stream.detachAsData();
    std::string svg((const char *)data->data(), data->size());

    // 3. Post-processing
    const auto &imageDraws = render_container.get_used_image_draws();
    for (size_t i = 0; i < imageDraws.size(); ++i) {
        const auto &draw = imageDraws[i];
        int index = (int)(i + 1);
        char tag[64]; snprintf(tag, sizeof(tag), "#0000%02x", index);
        auto it = context.imageCache.find(draw.url);
        if (it != context.imageCache.end() && it->second.skImage) {
            SkBitmap bitmap;
            bitmap.allocN32Pixels(it->second.skImage->width(), it->second.skImage->height());
            SkCanvas bitmapCanvas(bitmap);
            bitmapCanvas.drawImage(it->second.skImage, 0, 0);
            std::string dataUrl = bitmapToDataUrl(bitmap);
            std::stringstream ss;
            ss << "<image x=\"" << draw.layer.origin_box.x << "\" y=\"" << draw.layer.origin_box.y
               << "\" width=\"" << draw.layer.origin_box.width << "\" height=\""
               << draw.layer.origin_box.height << "\" href=\"" << dataUrl << "\" />";
            size_t pos = svg.find(tag);
            if (pos != std::string::npos) {
                size_t rectStart = svg.rfind("<rect", pos);
                size_t rectEnd = svg.find("/>", pos);
                if (rectStart != std::string::npos && rectEnd != std::string::npos) svg.replace(rectStart, rectEnd + 2 - rectStart, ss.str());
            }
        }
    }

    const auto &conics = render_container.get_used_conic_gradients();
    for (size_t i = 0; i < conics.size(); ++i) {
        const auto &info = conics[i];
        int index = (int)(i + 1);
        char tag[128]; snprintf(tag, sizeof(tag), "fill:rgb(0,0,%d);fill-opacity:0.992157", index);
        std::stringstream ds;
        ds << "<defs><conicGradient id=\"conic-" << index << "\" cx=\"" << info.gradient.position.x
           << "\" cy=\"" << info.gradient.position.y << "\" angle=\"" << info.gradient.angle << "\">";
        for (const auto &stop : info.gradient.color_points) {
            ds << "<stop offset=\"" << stop.offset << "\" stop-color=\"rgb(" << (int)stop.color.red
               << "," << (int)stop.color.green << "," << (int)stop.color.blue
               << ")\" stop-opacity=\"" << (float)stop.color.alpha / 255.0f << "\"/>";
        }
        ds << "</conicGradient></defs>";
        size_t pos = svg.find(tag);
        if (pos != std::string::npos) {
            std::string fillUrl = "fill:url(#conic-" + std::to_string(index) + ")";
            svg.replace(pos, strlen(tag), fillUrl);
            size_t svgEnd = svg.find(">", svg.find("<svg"));
            if (svgEnd != std::string::npos) svg.insert(svgEnd + 1, ds.str());
        }
    }

    const auto &shadows = render_container.get_used_shadows();
    for (size_t i = 0; i < shadows.size(); ++i) {
        const auto &s = shadows[i];
        int index = (int)(i + 1);
        char tag[128]; snprintf(tag, sizeof(tag), "fill:rgb(0,0,%d);fill-opacity:0.996078", index);
        std::stringstream ds;
        ds << "<defs><filter id=\"shadow-" << index << "\" x=\"-50%\" y=\"-50%\" width=\"200%\" height=\"200%\">"
           << "<feGaussianBlur in=\"SourceAlpha\" stdDeviation=\"" << s.blur * 0.5f << "\"/>"
           << "<feOffset dx=\"" << s.x << "\" dy=\"" << s.y << "\" result=\"offsetblur\"/>"
           << "<feFlood stop-color=\"rgb(" << (int)s.color.red << "," << (int)s.color.green << "," << (int)s.color.blue << ")\" stop-opacity=\"" << (float)s.color.alpha / 255.0f << "\"/>"
           << "<feComposite in2=\"offsetblur\" operator=\"in\"/>"
           << "<feMerge><feMergeNode/><feMergeNode in=\"SourceGraphic\"/></feMerge></filter></defs>";
        size_t pos = svg.find(tag);
        if (pos != std::string::npos) {
            std::string filterUrl = "filter:url(#shadow-" + std::to_string(index) + ")";
            svg.replace(pos, strlen(tag), filterUrl);
            size_t svgEnd = svg.find(">", svg.find("<svg"));
            if (svgEnd != std::string::npos) svg.insert(svgEnd + 1, ds.str());
        }
    }

    return svg;
}