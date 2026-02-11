#include "pdf_renderer.h"

#include <litehtml/master_css.h>

#include <memory>
#include <vector>

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

std::vector<uint8_t> renderHtmlToPdf(const char *html, int width, int height,
                                     SatoruContext &context, const char *master_css) {
    container_skia measure_container(width, height > 0 ? height : 1000, nullptr, context, nullptr,
                                     false);
    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    auto doc = litehtml::document::createFromString(html, &measure_container, css.c_str());
    if (!doc) return {};
    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    SkDynamicMemoryWStream stream;
    SkPDF::Metadata metadata;
    metadata.fTitle = "Satoru PDF Export";
    metadata.fCreator = "Satoru Engine";
    metadata.jpegDecoder = PdfJpegDecoder;
    metadata.jpegEncoder = PdfJpegEncoder;

    auto pdf_doc = SkPDF::MakeDocument(&stream, metadata);
    if (!pdf_doc) return {};

    SkCanvas *canvas = pdf_doc->beginPage((SkScalar)width, (SkScalar)content_height);
    if (!canvas) return {};

    container_skia render_container(width, content_height, canvas, context, nullptr, false);
    auto render_doc = litehtml::document::createFromString(html, &render_container, css.c_str());
    render_doc->render(width);

    litehtml::position clip(0, 0, width, content_height);
    render_doc->draw(0, 0, 0, &clip);

    pdf_doc->endPage();
    pdf_doc->close();

    sk_sp<SkData> data = stream.detachAsData();
    if (!data) return {};

    std::vector<uint8_t> result((const uint8_t *)data->data(),
                                (const uint8_t *)data->data() + data->size());
    return result;
}
