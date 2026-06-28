// Stubs for render_item methods used by container_skia_filters.cpp.
#include "render_item.h"

void litehtml::render_item::place(pixel_t /*x*/, pixel_t /*y*/,
                                  const containing_block_context& /*containing_block_size*/,
                                  formatting_context* /*fmt_ctx*/) {
    // Stub
}

litehtml::position litehtml::render_item::get_placement() const {
    return litehtml::position();
}
