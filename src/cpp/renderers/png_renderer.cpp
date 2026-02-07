#include "png_renderer.h"

#include <litehtml/master_css.h>

#include "container_skia.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"
#include "litehtml.h"
#include "../utils/utils.h"

std::string renderHtmlToPng(const char *html, int width, int height, SatoruContext &context, const char* master_css) {
    auto data = renderHtmlToPngBinary(html, width, height, context, master_css);
    if (!data) return "";
    return "data:image/png;base64," + base64_encode((const uint8_t *)data->data(), data->size());
}

sk_sp<SkData> renderHtmlToPngBinary(const char *html, int width, int height,
                                    SatoruContext &context, const char* master_css) {
    int initial_height = (height > 0) ? height : 1000;
    container_skia container(width, initial_height, nullptr, context, nullptr, false);

    std::string css = master_css ? master_css : litehtml::master_css;
    css += "\nbr { display: -litehtml-br !important; }\n";
    css += "button { text-align: center; }\n";

    litehtml::document::ptr doc =
        litehtml::document::createFromString(html, &container, css.c_str());
    if (!doc) return nullptr;

    doc->render(width);

    int content_height = (height > 0) ? height : (int)doc->height();
    if (content_height < 1) content_height = 1;

    container.set_height(content_height);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, content_height);
    bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));  // Transparent background

    SkCanvas canvas(bitmap);
    container.set_canvas(&canvas);

    litehtml::position clip(0, 0, width, content_height);
    doc->draw(0, 0, 0, &clip);

    SkDynamicMemoryWStream stream;
    if (SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) {
        return stream.detachAsData();
    }

    return nullptr;
}
