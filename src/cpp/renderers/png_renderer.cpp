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
#include "litehtml.h"
#include "utils/skia_utils.h"

sk_sp<SkData> renderDocumentToPng(SatoruInstance *inst, int width, int height,
                                  const RenderOptions &options) {
    if (!inst->doc || !inst->render_container) return nullptr;

    int content_height = (height > 0) ? height : (int)inst->doc->height();
    if (content_height < 1) content_height = 1;

    SkImageInfo info = SkImageInfo::MakeN32Premul(width, content_height, SkColorSpace::MakeSRGB());
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));  // Transparent background

    SkCanvas canvas(bitmap);

    inst->render_container->reset();
    inst->render_container->set_canvas(&canvas);
    inst->render_container->set_height(content_height);
    inst->render_container->set_tagging(false);

    litehtml::position clip(0, 0, width, content_height);
    inst->doc->draw(0, 0, 0, &clip);
    inst->render_container->flush();

    SkDynamicMemoryWStream stream;
    if (SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) {
        return stream.detachAsData();
    }
    return nullptr;
}

sk_sp<SkData> renderHtmlToPng(const char *html, int width, int height, SatoruContext &context,
                              const char *master_css) {
    int initial_height = (height > 0) ? height : 32767;
    container_skia container(width, initial_height, nullptr, context, nullptr, false);

    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";

    litehtml::document::ptr doc =
        litehtml::document::createFromString(html, &container, css.c_str());
    if (!doc) return nullptr;

    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    container.set_height(content_height);

    SkImageInfo info = SkImageInfo::MakeN32Premul(width, content_height, SkColorSpace::MakeSRGB());
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));  // Transparent background

    SkCanvas canvas(bitmap);
    container.set_canvas(&canvas);

    litehtml::position clip(0, 0, width, content_height);
    doc->draw(0, 0, 0, &clip);
    container.flush();

    SkDynamicMemoryWStream stream;
    if (SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) {
        return stream.detachAsData();
    }

    return nullptr;
}
