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
#include "utils/logging.h"

namespace {
std::unique_ptr<SkCodec> PdfJpegDecoder(sk_sp<const SkData> data) {
    return SkJpegDecoder::Decode(std::move(data), nullptr, nullptr);
}

bool PdfJpegEncoder(SkWStream* dst, const SkPixmap& src, int quality) {
    SkJpegEncoder::Options options;
    options.fQuality = quality;
    return SkJpegEncoder::Encode(dst, src, options);
}

std::string replace_template_vars(std::string html, int pageNum, int totalPages) {
    size_t pos;
    std::string pageNumStr = std::to_string(pageNum);
    std::string totalPagesStr = std::to_string(totalPages);

    while ((pos = html.find("{{pageNumber}}")) != std::string::npos) {
        html.replace(pos, 14, pageNumStr);
    }
    while ((pos = html.find("{{totalPages}}")) != std::string::npos) {
        html.replace(pos, 14, totalPagesStr);
    }
    return html;
}

void render_template(const std::string& html, int width, int height, SkCanvas* canvas,
                     SatoruContext& context, const char* master_css, const char* user_css,
                     litehtml::media_type media_type) {
    if (html.empty()) return;
    container_skia container(width, height, canvas, context, nullptr, false, media_type);
    auto doc = litehtml::document::createFromString(html.c_str(), &container, master_css, user_css);
    if (doc) {
        doc->render(width);
        doc->draw(0, 0, 0, nullptr);
        container.flush();
    }
}
}  // namespace

sk_sp<SkData> renderDocumentToPdf(SatoruInstance* inst, int width, int height,
                                  const RenderOptions& options) {
    if (!inst->doc || !inst->render_container) {
        SATORU_LOG_ERROR("[Satoru] renderDocumentToPdf FAILED: null doc/container");
        return nullptr;
    }

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
    metadata.fTitle = options.pdfTitle.empty() ? "Satoru PDF Export" : options.pdfTitle.c_str();
    metadata.fAuthor = options.pdfAuthor.c_str();
    metadata.fSubject = options.pdfSubject.c_str();
    metadata.fKeywords = options.pdfKeywords.c_str();
    metadata.fCreator = options.pdfCreator.empty() ? "Satoru Engine" : options.pdfCreator.c_str();
    metadata.fProducer = options.pdfProducer.c_str();
    metadata.jpegDecoder = PdfJpegDecoder;
    metadata.jpegEncoder = PdfJpegEncoder;

    auto pdf_doc = SkPDF::MakeDocument(&stream, metadata);
    if (!pdf_doc) return nullptr;

    SkCanvas* canvas = pdf_doc->beginPage((SkScalar)out_width, (SkScalar)out_height);
    if (canvas) {
        if (options.outputWidth > 0 || options.outputHeight > 0) {
            apply_resize_transform(canvas, src_w, src_h, options);
        }

        inst->render_container->reset();
        inst->render_container->set_canvas(canvas);
        inst->render_container->set_height(content_height);
        inst->render_container->set_tagging(false);

        litehtml::media_type media_type =
            (options.mediaType == 1) ? litehtml::media_type_print : litehtml::media_type_screen;
        if (inst->render_container->get_media_type() != media_type) {
            inst->render_container->set_media_type(media_type);
            inst->doc->media_changed();
            inst->doc->render(width);
        }

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
    metadata.fTitle = options.pdfTitle.empty() ? "Satoru PDF Export" : options.pdfTitle.c_str();
    metadata.fAuthor = options.pdfAuthor.c_str();
    metadata.fSubject = options.pdfSubject.c_str();
    metadata.fKeywords = options.pdfKeywords.c_str();
    metadata.fCreator = options.pdfCreator.empty() ? "Satoru Engine" : options.pdfCreator.c_str();
    metadata.fProducer = options.pdfProducer.c_str();
    metadata.jpegDecoder = PdfJpegDecoder;
    metadata.jpegEncoder = PdfJpegEncoder;

    auto pdf_doc = SkPDF::MakeDocument(&stream, metadata);
    if (!pdf_doc) return nullptr;

    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    int pageNum = 1;
    int totalPages = (int)htmls.size();

    for (const auto& html : htmls) {
        // Measure pass
        litehtml::media_type media_type =
            (options.mediaType == 1) ? litehtml::media_type_print : litehtml::media_type_screen;

        int margin_top = options.pdfMarginTop;
        int margin_bottom = options.pdfMarginBottom;
        int margin_left = options.pdfMarginLeft;
        int margin_right = options.pdfMarginRight;

        int content_width = width - margin_left - margin_right;
        if (content_width < 1) content_width = 1;

        container_skia measure_container(content_width, height > 0 ? height : 3000, nullptr,
                                         context, nullptr, false, media_type);
        auto measure_doc = litehtml::document::createFromString(html.c_str(), &measure_container,
                                                                css.c_str(), user_css);
        if (!measure_doc) continue;

        measure_doc->render(content_width);

        int measured_height = (height > 0) ? height : (int)measure_doc->height();
        int full_page_height = measured_height + margin_top + margin_bottom;
        if (full_page_height < 1) full_page_height = 1;

        SkCanvas* canvas = pdf_doc->beginPage((SkScalar)width, (SkScalar)full_page_height);
        if (!canvas) continue;

        if (options.backgroundColor != 0) {
            SkPaint paint;
            paint.setColor(options.backgroundColor);
            canvas->drawRect(SkRect::MakeWH(width, full_page_height), paint);
        }

        // Render Header
        if (!options.pdfHeader.empty()) {
            std::string headerHtml = replace_template_vars(options.pdfHeader, pageNum, totalPages);
            canvas->save();
            canvas->translate((SkScalar)margin_left, 0);
            render_template(headerHtml, content_width, margin_top, canvas, context, css.c_str(),
                            user_css, media_type);
            canvas->restore();
        }

        // Render Footer
        if (!options.pdfFooter.empty()) {
            std::string footerHtml = replace_template_vars(options.pdfFooter, pageNum, totalPages);
            canvas->save();
            canvas->translate((SkScalar)margin_left, (SkScalar)(full_page_height - margin_bottom));
            render_template(footerHtml, content_width, margin_bottom, canvas, context, css.c_str(),
                            user_css, media_type);
            canvas->restore();
        }

        // Render Content
        canvas->save();
        canvas->translate((SkScalar)margin_left, (SkScalar)margin_top);

        container_skia render_container(content_width, measured_height, canvas, context, nullptr,
                                        false, media_type);
        auto render_doc = litehtml::document::createFromString(html.c_str(), &render_container,
                                                               css.c_str(), user_css);
        if (!render_doc) continue;
        render_doc->render(content_width);
        render_doc->draw(0, 0, 0, nullptr);
        render_container.flush();

        canvas->restore();

        pdf_doc->endPage();
        pageNum++;
    }

    pdf_doc->close();

    return stream.detachAsData();
}
