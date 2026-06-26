// Tests for text_utils.h wrapper functions.
//
// These are thin wrappers that delegate to UnicodeService. The UnicodeService
// itself is tested separately in test_unicode_service.cpp. Here we verify:
//   1. The wrapper functions exist and are callable
//   2. The wrappers correctly delegate to UnicodeService
//   3. Pointer advancement behavior is correct for decode_utf8_char
//
// IMPORTANT: These tests use the UnicodeService stub, which has simplified
// behavior compared to the real implementation:
//   - decodeUtf8 returns U+FFFD for multi-byte (advances by 1 byte, not N bytes)
//   - normalize is identity (does not perform NFC composition)
//   - getBidiLevel always returns the base level (no actual BiDi analysis)
//
// The real UnicodeService has more sophisticated behavior that is verified
// by the JS/TS visual integration tests rather than C++ unit tests.
//
// Note: measure_text, text_width, ellipsize_text take a SatoruContext* and are
// NOT tested here because they require a full SatoruContext setup.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/text/unicode_service.h"
#include "core/text_utils.h"

using satoru::UnicodeService;

// ============================================================================
// decode_utf8_char
// ============================================================================

TEST(TextUtilsTest, DecodeUtf8Char_Ascii) {
    const char* input = "A";
    char32_t cp = satoru::decode_utf8_char(&input);
    EXPECT_EQ(cp, U'A');
}

TEST(TextUtilsTest, DecodeUtf8Char_MultiByteStubReturnsReplacement) {
    // The stub returns 0xFFFD for multi-byte sequences and advances by 1 byte
    // (not the full multi-byte length as the real UnicodeService would).
    // This test pins the stub's behavior so the wrapper's pointer advance
    // contract is documented.
    const char* input = "\xC3\xA9";  // 'é' in UTF-8
    const char* p = input;
    char32_t cp = satoru::decode_utf8_char(&p);
    EXPECT_EQ(cp, 0xFFFD);
    EXPECT_EQ(p, input + 1);  // stub advances by 1 byte
}

TEST(TextUtilsTest, DecodeUtf8Char_AdvancesThroughString) {
    const char* input = "Hello";
    const char* p = input;
    EXPECT_EQ(satoru::decode_utf8_char(&p), U'H');
    EXPECT_EQ(p, input + 1);
    EXPECT_EQ(satoru::decode_utf8_char(&p), U'e');
    EXPECT_EQ(p, input + 2);
    EXPECT_EQ(satoru::decode_utf8_char(&p), U'l');
    EXPECT_EQ(satoru::decode_utf8_char(&p), U'l');
    EXPECT_EQ(satoru::decode_utf8_char(&p), U'o');
}

TEST(TextUtilsTest, DecodeUtf8Char_EmptyString) {
    const char* input = "";
    char32_t cp = satoru::decode_utf8_char(&input);
    EXPECT_EQ(cp, char32_t(0));
}

TEST(TextUtilsTest, DecodeUtf8Char_NullPointer) {
    char32_t cp = satoru::decode_utf8_char(nullptr);
    EXPECT_EQ(cp, char32_t(0));
}

// ============================================================================
// normalize_utf8 — stub is identity (does not compose NFD to NFC)
// ============================================================================

TEST(TextUtilsTest, NormalizeUtf8_Ascii) {
    EXPECT_EQ(satoru::normalize_utf8("Hello World"), "Hello World");
}

TEST(TextUtilsTest, NormalizeUtf8_Empty) {
    EXPECT_TRUE(satoru::normalize_utf8("").empty());
}

TEST(TextUtilsTest, NormalizeUtf8_NullPointer) {
    EXPECT_TRUE(satoru::normalize_utf8(nullptr).empty());
}

TEST(TextUtilsTest, NormalizeUtf8_Cjk) {
    EXPECT_EQ(satoru::normalize_utf8("日本語"), "日本語");
}

TEST(TextUtilsTest, NormalizeUtf8_StubIsIdentity) {
    // The stub does NOT compose NFD sequences. The real UnicodeService would
    // call utf8proc_map to perform NFC composition. This test pins the
    // stub's identity behavior.
    const char* nfd = "\x65\xCC\x81";
    EXPECT_EQ(satoru::normalize_utf8(nfd), std::string(nfd));
}

// ============================================================================
// get_bidi_level — stub returns base level only
// ============================================================================

TEST(TextUtilsTest, GetBidiLevel_LtrBase) {
    EXPECT_EQ(satoru::get_bidi_level("Hello", 0, nullptr), 0);
}

TEST(TextUtilsTest, GetBidiLevel_RtlBase) {
    // The stub does NOT analyze the text — it always returns the base level.
    // The real UnicodeService would return 2 for LTR text in an RTL base.
    EXPECT_EQ(satoru::get_bidi_level("Hello", 1, nullptr), 1);
}

TEST(TextUtilsTest, GetBidiLevel_EmptyText) {
    EXPECT_EQ(satoru::get_bidi_level("", 0, nullptr), 0);
    EXPECT_EQ(satoru::get_bidi_level("", 1, nullptr), 1);
}

TEST(TextUtilsTest, GetBidiLevel_NullText) {
    EXPECT_EQ(satoru::get_bidi_level(nullptr, 0, nullptr), 0);
}

TEST(TextUtilsTest, GetBidiLevel_ArabicIsBaseLevel) {
    // The stub returns base level (0) for Arabic text — no actual BiDi
    // analysis. The real UnicodeService would return 1.
    EXPECT_EQ(satoru::get_bidi_level("\xD8\xB9", 0, nullptr), 0);
}

TEST(TextUtilsTest, GetBidiLevel_LastLevelNotWritten) {
    // The stub does not write to the lastLevel output parameter
    int lastLevel = 999;
    satoru::get_bidi_level("Hello", 1, &lastLevel);
    EXPECT_EQ(lastLevel, 999);
}

// ============================================================================
// measure_text / text_width / ellipsize_text
//
// These take SatoruContext* and font_info* which require a full SatoruContext
// setup. They are exercised by the visual integration tests in the JS/TS
// package (packages/satoru/) and are not unit-testable without significant
// mocking infrastructure. Documented as not unit-tested.
// ============================================================================

// Uncomment when SatoruContext mocking infrastructure is available:
//
// TEST(TextUtilsTest, MeasureText_NullContext) {
//     font_info fi{};
//     MeasureResult r = satoru::measure_text(nullptr, "Hello", &fi);
//     EXPECT_EQ(r.width, 0.0);
// }
