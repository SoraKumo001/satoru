// Stub for text_utils wrappers used in the native test build.
//
// The real text_utils.cpp depends on SatoruContext, which is not part of
// the test build. We provide stub implementations of the wrappers that
// just delegate to the (stubbed) UnicodeService — exactly the same behavior
// the real code has for these particular functions.
//
// Functions NOT provided here (require SatoruContext):
//   - measure_text
//   - text_width
//   - ellipsize_text
//
// These are exercised by the JS/TS visual integration tests in
// packages/satoru/, not by the C++ unit tests.
#include "core/text_utils.h"

#include "core/text/unicode_service.h"

namespace satoru {

char32_t decode_utf8_char(const char** ptr) {
    UnicodeService svc;
    return svc.decodeUtf8(ptr);
}

std::string normalize_utf8(const char* text) {
    UnicodeService svc;
    return svc.normalize(text);
}

int get_bidi_level(const char* text, int base_level, int* last_level) {
    UnicodeService svc;
    return svc.getBidiLevel(text, base_level, last_level);
}

}  // namespace satoru
