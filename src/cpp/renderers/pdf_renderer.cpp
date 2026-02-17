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
#include "utils/skia_utils.h"

namespace {
std::unique_ptr<SkCodec> PdfJpegDecoder(sk_sp<const SkData> data) {
    return SkJpegDecoder::Decode(std::move(data), nullptr, nullptr);
}

bool PdfJpegEncoder(SkWStream *dst, const SkPixmap &src, int quality) {
    SkJpegEncoder::Options options;
    options.fQuality = quality;
    return SkJpegEncoder::Encode(dst, src, options);
}
}  // namespace

sk_sp<SkData> renderDocumentToPdf(SatoruInstance *inst, int width, int height,
                                  const RenderOptions &options) {
    if (!inst->doc || !inst->render_container) return nullptr;

    SkDynamicMemoryWStream stream;
    SkPDF::Metadata metadata;
    metadata.fTitle = "Satoru PDF Export";
    metadata.fCreator = "Satoru Engine";
    metadata.jpegDecoder = PdfJpegDecoder;
    metadata.jpegEncoder = PdfJpegEncoder;

    auto pdf_doc = SkPDF::MakeDocument(&stream, metadata);
    if (!pdf_doc) return nullptr;

    int content_height = (height > 0) ? height : (int)inst->doc->height();
    if (content_height < 1) content_height = 1;

    SkCanvas *canvas = pdf_doc->beginPage((SkScalar)width, (SkScalar)content_height);
    if (canvas) {
        inst->render_container->reset();
        inst->render_container->set_canvas(canvas);
        inst->render_container->set_height(content_height);
        inst->render_container->set_tagging(false);

        litehtml::position clip(0, 0, width, content_height);
        inst->doc->draw(0, 0, 0, &clip);

        pdf_doc->endPage();
    }
    pdf_doc->close();
    return stream.detachAsData();
}

sk_sp<SkData> renderHtmlsToPdf(const std::vector<std::string> &htmls, int width, int height,
                               SatoruContext &context, const char *master_css) {
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

    for (const auto &html : htmls) {
        // Measure pass
        container_skia measure_container(width, height > 0 ? height : 32767, nullptr, context,
                                         nullptr, false);
        auto measure_doc =
            litehtml::document::createFromString(html.c_str(), &measure_container, css.c_str());
        if (!measure_doc) continue;

        measure_doc->render(width);

        int content_height = (height > 0) ? height : (int)measure_doc->height();
        if (content_height < 1) content_height = 1;

        SkCanvas *canvas = pdf_doc->beginPage((SkScalar)width, (SkScalar)content_height);
        if (!canvas) continue;

        // Render pass
        container_skia render_container(width, content_height, canvas, context, nullptr, false);
        auto render_doc =
            litehtml::document::createFromString(html.c_str(), &render_container, css.c_str());
        render_doc->render(width);

        litehtml::position clip(0, 0, width, content_height);
        render_doc->draw(0, 0, 0, &clip);

        pdf_doc->endPage();
    }

    pdf_doc->close();

    return stream.detachAsData();
}
