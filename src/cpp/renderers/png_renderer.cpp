#include "png_renderer.h"

#include <litehtml/master_css.h>

#include "api/satoru_api.h"
#include "core/container_skia.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"
#include "render_utils.h"

sk_sp<SkData> renderDocumentToPng(SatoruInstance* inst, int width, int height,
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

    SkImageInfo info = SkImageInfo::MakeN32Premul(out_width, out_height, SkColorSpace::MakeSRGB());
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));  // Transparent background

    SkCanvas canvas(bitmap);

    if (options.outputWidth > 0 || options.outputHeight > 0) {
        apply_resize_transform(&canvas, src_w, src_h, out_width, out_height, options.fitType);
    }

    inst->render_container->reset();
    inst->render_container->set_canvas(&canvas);
    inst->render_container->set_height(content_height);
    inst->render_container->set_tagging(false);

    litehtml::position clip(0, 0, src_w, src_h);
    inst->doc->draw(0, -src_x, -src_y, &clip);
    inst->render_container->flush();

    SkDynamicMemoryWStream stream;
    if (SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) {
        return stream.detachAsData();
    }
    return nullptr;
}

sk_sp<SkData> renderHtmlToPng(const char* html, int width, int height, SatoruContext& context,
                              const char* master_css, const char* user_css,
                              const RenderOptions& options) {
    int initial_height = (height > 0) ? height : 3000;
    container_skia container(width, initial_height, nullptr, context, nullptr, false);

    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    litehtml::document::ptr doc =
        litehtml::document::createFromString(html, &container, css.c_str(), user_css);
    if (!doc) return nullptr;

    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    int src_x = options.cropX;
    int src_y = options.cropY;
    int src_w = options.cropWidth > 0 ? options.cropWidth : width;
    int src_h = options.cropHeight > 0 ? options.cropHeight : content_height;

    int out_width = options.outputWidth > 0 ? options.outputWidth : src_w;
    int out_height = options.outputHeight > 0 ? options.outputHeight : src_h;

    SkImageInfo info = SkImageInfo::MakeN32Premul(out_width, out_height, SkColorSpace::MakeSRGB());
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));  // Transparent background

    SkCanvas canvas(bitmap);

    if (options.outputWidth > 0 || options.outputHeight > 0) {
        apply_resize_transform(&canvas, src_w, src_h, out_width, out_height, options.fitType);
    }

    container.set_canvas(&canvas);
    container.set_height(content_height);

    litehtml::position clip(0, 0, src_w, src_h);
    doc->draw(0, -src_x, -src_y, &clip);
    container.flush();

    SkDynamicMemoryWStream stream;
    if (SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) {
        return stream.detachAsData();
    }

    return nullptr;
}
