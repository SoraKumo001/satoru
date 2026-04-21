#include "pdf_renderer.h"

#include <litehtml/master_css.h>

#include <memory>
#include <vector>

#include "api/satoru_api.h"
#include "core/container_skia.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/docs/SkPDFDocument.h"
#include "include/encode/SkJpegEncoder.h"
#include "render_utils.h"

namespace {
std::unique_ptr<SkCodec> PdfJpegDecoder(sk_sp<const SkData> data) {
    return SkJpegDecoder::Decode(std::move(data), nullptr, nullptr);
}

bool PdfJpegEncoder(SkWStream* dst, const SkPixmap& src, int quality) {
    SkJpegEncoder::Options options;
    options.fQuality = quality;
    return SkJpegEncoder::Encode(dst, src, options);
}
}  // namespace

sk_sp<SkData> renderDocumentToPdf(SatoruInstance* inst, int width, int height,
                                  const RenderOptions& options) {
    if (!inst->doc || !inst->render_container) return nullptr;

    int content_width = width;
    int content_height = (height > 0) ? height : (int)inst->doc->height();
    if (content_height < 1) content_height = 1;

    int src_x = options.cropX;
    int src_y = options.cropY;
    int src_w = options.cropWidth > 0 ? options.cropWidth : content_width;
    int src_h = options.cropHeight > 0 ? options.cropHeight : content_height;

    int out_width = options.outputWidth > 0 ? options.outputWidth : src_w;
    int out_height = options.outputHeight > 0 ? options.outputHeight : src_h;

    SkDynamicMemoryWStream stream;
    SkPDF::Metadata metadata;
    metadata.fTitle = "Satoru PDF Export";
    metadata.fCreator = "Satoru Engine";
    metadata.jpegDecoder = PdfJpegDecoder;
    metadata.jpegEncoder = PdfJpegEncoder;

    auto pdf_doc = SkPDF::MakeDocument(&stream, metadata);
    if (!pdf_doc) return nullptr;

    SkCanvas* canvas = pdf_doc->beginPage((SkScalar)out_width, (SkScalar)out_height);
    if (canvas) {
        if (options.outputWidth > 0 || options.outputHeight > 0) {
            apply_resize_transform(canvas, src_w, src_h, out_width, out_height, options.fitType);
        }

        inst->render_container->reset();
        inst->render_container->set_canvas(canvas);
        inst->render_container->set_height(content_height);
        inst->render_container->set_tagging(false);

        litehtml::position clip(0, 0, src_w, src_h);
        inst->doc->draw(0, -src_x, -src_y, &clip);
        inst->render_container->flush();

        pdf_doc->endPage();
    }
    pdf_doc->close();
    return stream.detachAsData();
}

sk_sp<SkData> renderHtmlsToPdf(const std::vector<std::string>& htmls, int width, int height,
                               SatoruContext& context, const char* master_css, const char* user_css,
                               const RenderOptions& options) {
    if (htmls.empty()) return nullptr;

    SkDynamicMemoryWStream stream;
    SkPDF::Metadata metadata;
    metadata.fTitle = "Satoru PDF Export";
    metadata.fCreator = "Satoru Engine";
    metadata.jpegDecoder = PdfJpegDecoder;
    metadata.jpegEncoder = PdfJpegEncoder;

    auto pdf_doc = SkPDF::MakeDocument(&stream, metadata);
    if (!pdf_doc) return nullptr;

    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    for (const auto& html : htmls) {
        // Measure pass
        container_skia measure_container(width, height > 0 ? height : 3000, nullptr, context,
                                         nullptr, false);
        auto measure_doc = litehtml::document::createFromString(html.c_str(), &measure_container,
                                                                css.c_str(), user_css);
        if (!measure_doc) continue;

        measure_doc->render(width);

        int content_height = (height > 0) ? height : (int)measure_doc->height();
        if (content_height < 1) content_height = 1;

        int src_x = options.cropX;
        int src_y = options.cropY;
        int src_w = options.cropWidth > 0 ? options.cropWidth : width;
        int src_h = options.cropHeight > 0 ? options.cropHeight : content_height;

        int out_width = options.outputWidth > 0 ? options.outputWidth : src_w;
        int out_height = options.outputHeight > 0 ? options.outputHeight : src_h;

        SkCanvas* canvas = pdf_doc->beginPage((SkScalar)out_width, (SkScalar)out_height);
        if (!canvas) continue;

        if (options.outputWidth > 0 || options.outputHeight > 0) {
            apply_resize_transform(canvas, src_w, src_h, out_width, out_height, options.fitType);
        }

        // Render pass
        container_skia render_container(width, content_height, canvas, context, nullptr, false);
        auto render_doc = litehtml::document::createFromString(html.c_str(), &render_container,
                                                               css.c_str(), user_css);
        render_doc->render(width);

        litehtml::position clip(0, 0, src_w, src_h);
        render_doc->draw(0, -src_x, -src_y, &clip);
        render_container.flush();

        pdf_doc->endPage();
    }

    pdf_doc->close();

    return stream.detachAsData();
}
