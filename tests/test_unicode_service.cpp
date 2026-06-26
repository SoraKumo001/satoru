// Tests for UnicodeService stub implementation.
//
// The real UnicodeService in src/cpp/core/text/unicode_service.cpp uses
// utf8proc + libunibreak, which are heavy C dependencies only available
// for the wasm32 target. For the native test build, we use the stub in
// tests/stubs/unicode_service_stub.cpp.
//
// These tests verify:
//   1. The stub's API matches the real UnicodeService (so callers work
//      correctly when linked against either implementation).
//   2. The stub's documented simplified behavior (identity normalization,
//      simplified Unicode property detection) is correct.
//
// The real UnicodeService has more sophisticated behavior (proper NFC
// composition, full Unicode property tables, libunibreak-based line breaking)
// that is verified by the JS/TS visual integration tests rather than C++
// unit tests.

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "core/text/unicode_service.h"

using satoru::UnicodeService;

// ============================================================================
// decodeUtf8 — stub returns U+FFFD for multi-byte, advances by 1 each call
// ============================================================================

TEST(UnicodeServiceStubTest, DecodeUtf8_Ascii) {
    UnicodeService svc;
    const char* input = "A";
    char32_t cp = svc.decodeUtf8(&input);
    EXPECT_EQ(cp, U'A');
    EXPECT_EQ(*input, '\0');
}

TEST(UnicodeServiceStubTest, DecodeUtf8_AsciiAdvancesOne) {
    UnicodeService svc;
    const char* input = "AB";
    const char* p = input;
    EXPECT_EQ(svc.decodeUtf8(&p), U'A');
    EXPECT_EQ(p, input + 1);
    EXPECT_EQ(svc.decodeUtf8(&p), U'B');
    EXPECT_EQ(p, input + 2);
}

TEST(UnicodeServiceStubTest, DecodeUtf8_MultiByteReturnsReplacementChar) {
    // The stub returns U+FFFD for any multi-byte sequence
    UnicodeService svc;
    const char* input = "\xC3\xA9";  // 'é' in UTF-8
    const char* p = input;
    char32_t cp = svc.decodeUtf8(&p);
    EXPECT_EQ(cp, 0xFFFD);
    // The stub advances by 1 byte even for invalid sequences
    EXPECT_EQ(p, input + 1);
}

TEST(UnicodeServiceStubTest, DecodeUtf8_EmptyString) {
    UnicodeService svc;
    const char* input = "";
    char32_t cp = svc.decodeUtf8(&input);
    EXPECT_EQ(cp, char32_t(0));
}

TEST(UnicodeServiceStubTest, DecodeUtf8_NullPointer) {
    UnicodeService svc;
    char32_t cp = svc.decodeUtf8(nullptr);
    EXPECT_EQ(cp, char32_t(0));
}

// ============================================================================
// encodeUtf8 — stub writes single byte for ASCII, replacement char otherwise
// ============================================================================

TEST(UnicodeServiceStubTest, EncodeUtf8_Ascii) {
    UnicodeService svc;
    std::string out;
    svc.encodeUtf8(U'A', out);
    EXPECT_EQ(out, "A");
}

TEST(UnicodeServiceStubTest, EncodeUtf8_NonAsciiReturnsReplacement) {
    // The stub encodes non-ASCII as a single byte (simplified behavior).
    // The real implementation would write the proper 3-byte UTF-8 encoding
    // (EF BF BD). This test pins the stub's single-byte output behavior.
    UnicodeService svc;
    std::string out;
    svc.encodeUtf8(U'é', out);
    EXPECT_EQ(out.size(), 1u);  // single byte, not 3
    EXPECT_EQ(static_cast<unsigned char>(out[0]),
              static_cast<unsigned char>(0xFD));  // truncated from 0xFFFD
}

TEST(UnicodeServiceStubTest, EncodeUtf8_AppendsToExisting) {
    UnicodeService svc;
    std::string out = "x";
    svc.encodeUtf8(U'A', out);
    EXPECT_EQ(out, "xA");
}

// ============================================================================
// normalize — stub is identity (no transformation)
// ============================================================================

TEST(UnicodeServiceStubTest, Normalize_AsciiUnchanged) {
    UnicodeService svc;
    EXPECT_EQ(svc.normalize("Hello World"), "Hello World");
}

TEST(UnicodeServiceStubTest, Normalize_Empty) {
    UnicodeService svc;
    EXPECT_TRUE(svc.normalize("").empty());
}

TEST(UnicodeServiceStubTest, Normalize_NullPointer) {
    UnicodeService svc;
    EXPECT_TRUE(svc.normalize(nullptr).empty());
}

TEST(UnicodeServiceStubTest, Normalize_IsIdentity) {
    // The stub does NOT compose NFD sequences — unlike the real implementation
    // which would call utf8proc_map. This test pins that behavior so a refactor
    // of the stub doesn't accidentally start doing NFC composition.
    UnicodeService svc;
    // NFD for é: e + combining acute
    const char* nfd = "\x65\xCC\x81";
    std::string result = svc.normalize(nfd);
    // The stub returns the input unchanged
    EXPECT_EQ(result, std::string(nfd));
}

TEST(UnicodeServiceStubTest, Normalize_CjkIdentity) {
    UnicodeService svc;
    EXPECT_EQ(svc.normalize("日本語"), "日本語");
}

// ============================================================================
// getBidiLevel — stub returns base level
// ============================================================================

TEST(UnicodeServiceStubTest, GetBidiLevel_LtrBase) {
    UnicodeService svc;
    EXPECT_EQ(svc.getBidiLevel("Hello", 0, nullptr), 0);
}

TEST(UnicodeServiceStubTest, GetBidiLevel_RtlBase) {
    UnicodeService svc;
    EXPECT_EQ(svc.getBidiLevel("Hello", 1, nullptr), 1);
}

TEST(UnicodeServiceStubTest, GetBidiLevel_EmptyText) {
    UnicodeService svc;
    EXPECT_EQ(svc.getBidiLevel("", 0, nullptr), 0);
    EXPECT_EQ(svc.getBidiLevel("", 1, nullptr), 1);
}

TEST(UnicodeServiceStubTest, GetBidiLevel_NullText) {
    UnicodeService svc;
    EXPECT_EQ(svc.getBidiLevel(nullptr, 0, nullptr), 0);
}

TEST(UnicodeServiceStubTest, GetBidiLevel_ArabicIsBaseLevel) {
    // The stub does NOT actually parse Arabic — it just returns base level.
    // This test pins that limitation. The real UnicodeService would return
    // level 1 for Arabic text in a LTR base.
    UnicodeService svc;
    int level = svc.getBidiLevel("\xD8\xB9", 0, nullptr);
    EXPECT_EQ(level, 0);  // Stub returns base level, not actual BiDi level
}

TEST(UnicodeServiceStubTest, GetBidiLevel_LastLevelNotWritten) {
    // The stub does not write to lastLevel output parameter
    UnicodeService svc;
    int lastLevel = 999;
    int level = svc.getBidiLevel("Hello", 1, &lastLevel);
    EXPECT_EQ(level, 1);
    EXPECT_EQ(lastLevel, 999);
}

// ============================================================================
// isMark — stub returns true for combining mark ranges
// ============================================================================

TEST(UnicodeServiceStubTest, IsMark_CombiningGraveAccent) {
    UnicodeService svc;
    EXPECT_TRUE(svc.isMark(0x0300));
}

TEST(UnicodeServiceStubTest, IsMark_OtherCombiningRanges) {
    UnicodeService svc;
    EXPECT_TRUE(svc.isMark(0x1DC0));
    EXPECT_TRUE(svc.isMark(0x20D0));
    EXPECT_TRUE(svc.isMark(0xFE20));
}

TEST(UnicodeServiceStubTest, IsMark_AsciiNotMark) {
    UnicodeService svc;
    EXPECT_FALSE(svc.isMark(U'A'));
    EXPECT_FALSE(svc.isMark(U'a'));
    EXPECT_FALSE(svc.isMark(U'0'));
}

TEST(UnicodeServiceStubTest, IsMark_CjkNotMark) {
    UnicodeService svc;
    EXPECT_FALSE(svc.isMark(U'日'));
}

// ============================================================================
// isSpace — stub returns true for known whitespace characters
// ============================================================================

TEST(UnicodeServiceStubTest, IsSpace_AsciiWhitespace) {
    UnicodeService svc;
    EXPECT_TRUE(svc.isSpace(U' '));
    EXPECT_TRUE(svc.isSpace(U'\t'));
    EXPECT_TRUE(svc.isSpace(U'\n'));
    EXPECT_TRUE(svc.isSpace(U'\r'));
}

TEST(UnicodeServiceStubTest, IsSpace_IdeographicSpace) {
    UnicodeService svc;
    EXPECT_TRUE(svc.isSpace(0x3000));
}

TEST(UnicodeServiceStubTest, IsSpace_AsciiNonWhitespace) {
    UnicodeService svc;
    EXPECT_FALSE(svc.isSpace(U'A'));
    EXPECT_FALSE(svc.isSpace(U'0'));
}

TEST(UnicodeServiceStubTest, IsSpace_CjkNotSpace) {
    UnicodeService svc;
    EXPECT_FALSE(svc.isSpace(U'日'));
}

// ============================================================================
// isEmoji — stub returns true for known emoji ranges
// ============================================================================

TEST(UnicodeServiceStubTest, IsEmoji_GrinningFace) {
    UnicodeService svc;
    EXPECT_TRUE(svc.isEmoji(0x1F600));
}

TEST(UnicodeServiceStubTest, IsEmoji_OtherRanges) {
    UnicodeService svc;
    EXPECT_TRUE(svc.isEmoji(0x1F300));
    EXPECT_TRUE(svc.isEmoji(0x1F680));
    EXPECT_TRUE(svc.isEmoji(0x2600));  // ☀
    EXPECT_TRUE(svc.isEmoji(0x2700));
}

TEST(UnicodeServiceStubTest, IsEmoji_AsciiNotEmoji) {
    UnicodeService svc;
    EXPECT_FALSE(svc.isEmoji(U'A'));
    EXPECT_FALSE(svc.isEmoji(U'0'));
}

TEST(UnicodeServiceStubTest, IsEmoji_CjkNotEmoji) {
    UnicodeService svc;
    EXPECT_FALSE(svc.isEmoji(U'日'));
}

// ============================================================================
// getLineBreaks — stub is no-op
// ============================================================================

TEST(UnicodeServiceStubTest, GetLineBreaks_NullText) {
    UnicodeService svc;
    std::vector<char> breaks;
    svc.getLineBreaks(nullptr, 0, "en", breaks);
    EXPECT_TRUE(breaks.empty());
}

TEST(UnicodeServiceStubTest, GetLineBreaks_EmptyText) {
    UnicodeService svc;
    std::vector<char> breaks;
    svc.getLineBreaks("", 0, "en", breaks);
    EXPECT_TRUE(breaks.empty());
}

TEST(UnicodeServiceStubTest, GetLineBreaks_NonEmptyText) {
    // The stub doesn't actually compute line breaks
    UnicodeService svc;
    std::vector<char> breaks;
    svc.getLineBreaks("Hello world", 11, "en", breaks);
    EXPECT_TRUE(breaks.empty());
}

// ============================================================================
// clearCache — stub no-op
// ============================================================================

TEST(UnicodeServiceStubTest, ClearCache_DoesNotCrash) {
    UnicodeService svc;
    svc.clearCache();
    // No assertion needed — just verify it doesn't crash
}
