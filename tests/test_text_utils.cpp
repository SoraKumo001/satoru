#include <gtest/gtest.h>

#include "core/text/unicode_service.h"

// ============================================================================
// UnicodeService stub tests
//
// These test the UnicodeService stub implementation directly, which is the
// underlying engine behind text_utils pure functions (decode_utf8_char,
// normalize_utf8, get_bidi_level). The text_utils wrappers are one-liner
// delegates that construct a UnicodeService and forward the call.
// ============================================================================

// ---------------------------------------------------------------------------
// decodeUtf8
// ---------------------------------------------------------------------------

TEST(unicode_service_test, decode_utf8_ascii) {
    satoru::UnicodeService svc;
    const char* input = "A";
    char32_t cp = svc.decodeUtf8(&input);
    EXPECT_EQ(cp, U'A');
}

TEST(unicode_service_test, decode_utf8_ascii_advances_ptr) {
    satoru::UnicodeService svc;
    const char* input = "Hello";
    const char* p = input;
    char32_t cp = svc.decodeUtf8(&p);
    EXPECT_EQ(cp, U'H');
    EXPECT_EQ(p, input + 1);  // advanced by 1 for ASCII
}

TEST(unicode_service_test, decode_utf8_ascii_multiple) {
    satoru::UnicodeService svc;
    const char* input = "AB";
    const char* p = input;
    EXPECT_EQ(svc.decodeUtf8(&p), U'A');
    EXPECT_EQ(svc.decodeUtf8(&p), U'B');
    EXPECT_EQ(*p, '\0');  // reached end
}

TEST(unicode_service_test, decode_utf8_multi_byte_replacement) {
    satoru::UnicodeService svc;
    // 2-byte UTF-8 sequence (Latin character with accent, e.g. é = 0xC3 0xA9)
    const char* input = "\xC3\xA9";
    const char* p = input;
    // Stub returns replacement character for multi-byte sequences
    char32_t cp = svc.decodeUtf8(&p);
    EXPECT_EQ(cp, 0xFFFD);
}

TEST(unicode_service_test, decode_utf8_empty_string) {
    satoru::UnicodeService svc;
    const char* input = "";
    char32_t cp = svc.decodeUtf8(&input);
    EXPECT_EQ(cp, char32_t(0));
}

// ---------------------------------------------------------------------------
// normalize (stub is identity)
// ---------------------------------------------------------------------------

TEST(unicode_service_test, normalize_identity) {
    satoru::UnicodeService svc;
    std::string result = svc.normalize("Hello World");
    EXPECT_EQ(result, "Hello World");
}

TEST(unicode_service_test, normalize_empty) {
    satoru::UnicodeService svc;
    std::string result = svc.normalize("");
    EXPECT_TRUE(result.empty());
}

TEST(unicode_service_test, normalize_cjk_identity) {
    satoru::UnicodeService svc;
    std::string input = "日本語";
    std::string result = svc.normalize(input.c_str());
    EXPECT_EQ(result, input);
}

// ---------------------------------------------------------------------------
// getBidiLevel (stub returns baseLevel)
// ---------------------------------------------------------------------------

TEST(unicode_service_test, get_bidi_level_ltr_base) {
    satoru::UnicodeService svc;
    int level = svc.getBidiLevel("Hello", 0, nullptr);
    EXPECT_EQ(level, 0);
}

TEST(unicode_service_test, get_bidi_level_rtl_base) {
    satoru::UnicodeService svc;
    int level = svc.getBidiLevel("Hello", 1, nullptr);
    EXPECT_EQ(level, 1);
}

TEST(unicode_service_test, get_bidi_level_returns_base_level_for_any_text) {
    satoru::UnicodeService svc;
    EXPECT_EQ(svc.getBidiLevel("abc", 2, nullptr), 2);
    EXPECT_EQ(svc.getBidiLevel("日本語", 0, nullptr), 0);
    EXPECT_EQ(svc.getBidiLevel("", 1, nullptr), 1);
}

TEST(unicode_service_test, get_bidi_level_with_last_level_output) {
    satoru::UnicodeService svc;
    int last_level = 999;
    int level = svc.getBidiLevel("Hello", 1, &last_level);
    EXPECT_EQ(level, 1);
    // Stub does not write to last_level output parameter
    EXPECT_EQ(last_level, 999);
}

// ---------------------------------------------------------------------------
// Other UnicodeService stub methods
// ---------------------------------------------------------------------------

TEST(unicode_service_test, encode_utf8) {
    satoru::UnicodeService svc;
    std::string out;
    svc.encodeUtf8(U'A', out);
    EXPECT_EQ(out, "A");
}

TEST(unicode_service_test, is_mark) {
    satoru::UnicodeService svc;
    // Stub returns true for combining marks (0x0300-0x036F)
    EXPECT_TRUE(svc.isMark(0x0300));  // combining grave accent
    // ...and false for non-marks
    EXPECT_FALSE(svc.isMark(U'A'));
    EXPECT_FALSE(svc.isMark(0x1F600));  // emoji, not a mark
}

TEST(unicode_service_test, is_space) {
    satoru::UnicodeService svc;
    EXPECT_TRUE(svc.isSpace(U' '));
    EXPECT_FALSE(svc.isSpace(U'A'));
}

TEST(unicode_service_test, is_emoji) {
    satoru::UnicodeService svc;
    // Stub returns true for emoji ranges (0x1F600-0x1F64F)
    EXPECT_TRUE(svc.isEmoji(0x1F600));  // grinning face
    // ...and false for non-emoji
    EXPECT_FALSE(svc.isEmoji(U'A'));
    EXPECT_FALSE(svc.isEmoji(0x0300));  // combining mark, not emoji
}
