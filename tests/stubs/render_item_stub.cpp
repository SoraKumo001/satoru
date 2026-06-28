// Stub for render_item::place() used by litehtml_extensions.cpp.
// The full render_item.cpp has too many litehtml DOM dependencies
// (document::to_pixels, element, etc.) for the unit test build.
// This stub is never called by any test — it only satisfies the linker
// so that litehtml_extensions.cpp can be compiled and validated.

#include "render_item.h"

void litehtml::render_item::place(pixel_t /*x*/, pixel_t /*y*/,
                                  const containing_block_context& /*containing_block_size*/,
                                  formatting_context* /*fmt_ctx*/) {
    // Stub — tests do not exercise render_item placement.
}
