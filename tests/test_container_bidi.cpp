// Tests for container_skia::get_bidi_level fast-path character classification.
//
// container_skia::get_bidi_level is a virtual override of
// litehtml::document_container::get_bidi_level. It contains an inline fast-path
// that classifies the first character of text into BiDi categories:
//
//   ASCII strong (letters/digits)          → level = (base_level==1) ? 2 : 0
//   CJK / Hiragana / Katakana / Hangul     → level = (base_level==1) ? 2 : 0
//   ASCII neutral (punctuation/controls)   → fall-through to UnicodeService
//   Other non-ASCII                        → fall-through to UnicodeService
//
// The fall-through delegates to UnicodeService::getBidiLevel. In the test stub
// (unicode_service_stub.cpp), this always returns the base_level unchanged.
//
// Since instantiating container_skia requires SkCanvas, SatoruContext, and
// ResourceManager, we extract the pure character-classification logic into a
// standalone helper (isomorphic to container_skia.cpp L1652-1679) — the same
// pattern used by test_container_resolve.cpp.
//
// NOTE on caching: The original method also updates m_last_bidi_level for
// callers that read it later (e.g. subsequent split_text calls). The return
// value is not affected by the cache, so we omit it from the standalone helper.

#include <gtest/gtest.h>

#include <string>

// ============================================================================
// Standalone helper: isomorphic to container_skia::get_bidi_level (minus cache)
// ============================================================================
namespace {

/// Classifies the first character of \p text and returns the BiDi embedding
/// level, or falls through to \p base_level (matching the stub's behaviour).
static int classify_bidi_level(const char* text, int base_level) {
    if (!text || !*text) return base_level;
    const unsigned char c0 = static_cast<unsigned char>(text[0]);
    if (c0 < 0x80) {
        // ASCII neutral: controls(0x00-0x1F), space, !"#$%&'()*+,-./ (0x20-0x2F),
        // :;<=>?@ (0x3A-0x40), [\]^_` (0x5B-0x60), {|}~DEL (0x7B-0x7F)
        bool is_ascii_neutral = c0 <= 0x2F || (c0 >= 0x3A && c0 <= 0x40) ||
                                (c0 >= 0x5B && c0 <= 0x60) || (c0 >= 0x7B && c0 <= 0x7F);
        if (!is_ascii_neutral) {
            return (base_level == 1) ? 2 : 0;
        }
    } else {
        const unsigned char c1 = static_cast<unsigned char>(text[1]);
        // CJK Unified Ideographs        U+4E00-U+9FFF   UTF-8: 0xE4-0xE9
        // Hiragana                      U+3040-U+309F   UTF-8: 0xE3 0x81-0x82
        // Katakana                      U+30A0-U+30FF   UTF-8: 0xE3 0x82-0x83
        // Hangul Syllables              U+AC00-U+D7A3   UTF-8: 0xEA-0xED
        if ((c0 >= 0xE4 && c0 <= 0xE9) ||                // CJK Unified Ideographs (U+4E00-U+9FFF)
            (c0 == 0xE3 && c1 >= 0x81 && c1 <= 0x83) ||  // Hiragana/Katakana
            (c0 == 0xE3 && c1 >= 0x90) ||                // CJK Extension A (U+3400-U+3FFF)
            (c0 >= 0xEA && c0 <= 0xED)) {                // Hangul syllables
            return (base_level == 1) ? 2 : 0;
        }
    }
    // Fall-through — stub UnicodeService returns base_level unchanged.
    return base_level;
}

}  // anonymous namespace

// ============================================================================
// Null / empty
// ============================================================================

TEST(ContainerBidiTest, NullText) {
    EXPECT_EQ(classify_bidi_level(nullptr, 0), 0);
    EXPECT_EQ(classify_bidi_level(nullptr, 1), 1);
}

TEST(ContainerBidiTest, EmptyText) {
    EXPECT_EQ(classify_bidi_level("", 0), 0);
    EXPECT_EQ(classify_bidi_level("", 1), 1);
}

// ============================================================================
// ASCII strong characters (letters) — return 0 for LTR base, 2 for RTL base
// ============================================================================

TEST(ContainerBidiTest, AsciiLowercase_LtrBase) {
    EXPECT_EQ(classify_bidi_level("a", 0), 0);
    EXPECT_EQ(classify_bidi_level("z", 0), 0);
}

TEST(ContainerBidiTest, AsciiLowercase_RtlBase) {
    EXPECT_EQ(classify_bidi_level("a", 1), 2);
    EXPECT_EQ(classify_bidi_level("z", 1), 2);
}

TEST(ContainerBidiTest, AsciiUppercase_LtrBase) {
    EXPECT_EQ(classify_bidi_level("A", 0), 0);
    EXPECT_EQ(classify_bidi_level("Z", 0), 0);
}

TEST(ContainerBidiTest, AsciiUppercase_RtlBase) {
    EXPECT_EQ(classify_bidi_level("A", 1), 2);
    EXPECT_EQ(classify_bidi_level("Z", 1), 2);
}

TEST(ContainerBidiTest, AsciiMixedCaseText) {
    // Only the first character matters
    EXPECT_EQ(classify_bidi_level("Hello", 0), 0);
    EXPECT_EQ(classify_bidi_level("World", 1), 2);
}

// ============================================================================
// ASCII digits — treated as strong (EN class in Unicode BiDi)
// ============================================================================

TEST(ContainerBidiTest, AsciiDigits_LtrBase) {
    EXPECT_EQ(classify_bidi_level("0", 0), 0);
    EXPECT_EQ(classify_bidi_level("9", 0), 0);
    EXPECT_EQ(classify_bidi_level("123", 0), 0);
}

TEST(ContainerBidiTest, AsciiDigits_RtlBase) {
    EXPECT_EQ(classify_bidi_level("0", 1), 2);
    EXPECT_EQ(classify_bidi_level("9", 1), 2);
}

// ============================================================================
// ASCII strong — boundary bytes (0x30-0x39, 0x41-0x5A, 0x61-0x7A)
// ============================================================================

TEST(ContainerBidiTest, AsciiStrongBoundaries) {
    // Boundaries of the strong ranges: just inside each range
    EXPECT_EQ(classify_bidi_level("\x30", 0), 0);  // '0'
    EXPECT_EQ(classify_bidi_level("\x39", 1), 2);  // '9'
    EXPECT_EQ(classify_bidi_level("\x41", 0), 0);  // 'A'
    EXPECT_EQ(classify_bidi_level("\x5A", 1), 2);  // 'Z'
    EXPECT_EQ(classify_bidi_level("\x61", 0), 0);  // 'a'
    EXPECT_EQ(classify_bidi_level("\x7A", 1), 2);  // 'z'
}

// ============================================================================
// ASCII neutral characters — fall through to base_level
// ============================================================================

TEST(ContainerBidiTest, AsciiNeutralPunctuation) {
    // 0x21-0x2F: !"#$%&'()*+,-./
    EXPECT_EQ(classify_bidi_level("!", 0), 0);
    EXPECT_EQ(classify_bidi_level("!", 1), 1);
    EXPECT_EQ(classify_bidi_level("/", 0), 0);
    EXPECT_EQ(classify_bidi_level("/", 1), 1);
}

TEST(ContainerBidiTest, AsciiNeutralColonAt) {
    // 0x3A-0x40: :;<=>?@
    EXPECT_EQ(classify_bidi_level(":", 0), 0);
    EXPECT_EQ(classify_bidi_level(":", 1), 1);
    EXPECT_EQ(classify_bidi_level("@", 0), 0);
    EXPECT_EQ(classify_bidi_level("@", 1), 1);
}

TEST(ContainerBidiTest, AsciiNeutralBrackets) {
    // 0x5B-0x60: [\]^_`
    EXPECT_EQ(classify_bidi_level("[", 0), 0);
    EXPECT_EQ(classify_bidi_level("[", 1), 1);
    EXPECT_EQ(classify_bidi_level("`", 0), 0);
    EXPECT_EQ(classify_bidi_level("`", 1), 1);
}

TEST(ContainerBidiTest, AsciiNeutralBraces) {
    // 0x7B-0x7F: {|}~DEL
    EXPECT_EQ(classify_bidi_level("{", 0), 0);
    EXPECT_EQ(classify_bidi_level("{", 1), 1);
    EXPECT_EQ(classify_bidi_level("\x7F", 0), 0);  // DEL
    EXPECT_EQ(classify_bidi_level("\x7F", 1), 1);
}

TEST(ContainerBidiTest, AsciiSpace) {
    EXPECT_EQ(classify_bidi_level(" ", 0), 0);
    EXPECT_EQ(classify_bidi_level(" ", 1), 1);
    EXPECT_EQ(classify_bidi_level("\t", 0), 0);
    EXPECT_EQ(classify_bidi_level("\t", 1), 1);
}

TEST(ContainerBidiTest, AsciiControlChars) {
    EXPECT_EQ(classify_bidi_level("\x01", 0), 0);  // SOH
    EXPECT_EQ(classify_bidi_level("\x01", 1), 1);
    EXPECT_EQ(classify_bidi_level("\x1F", 0), 0);  // US
    EXPECT_EQ(classify_bidi_level("\x1F", 1), 1);
}

// ============================================================================
// ASCII neutral — boundary bytes adjacent to strong ranges
// ============================================================================

TEST(ContainerBidiTest, AsciiNeutralBoundaries) {
    // Just outside the strong ranges
    EXPECT_EQ(classify_bidi_level("\x2F", 1), 1);  // '/' before '0'
    EXPECT_EQ(classify_bidi_level("\x3A", 1), 1);  // ':' after '9'
    EXPECT_EQ(classify_bidi_level("\x40", 1), 1);  // '@' before 'A'
    EXPECT_EQ(classify_bidi_level("\x5B", 1), 1);  // '[' after 'Z'
    EXPECT_EQ(classify_bidi_level("\x60", 1), 1);  // '`' before 'a'
    EXPECT_EQ(classify_bidi_level("\x7B", 1), 1);  // '{' after 'z'
}

// ============================================================================
// CJK Unified Ideographs (U+4E00–U+9FFF)
// ============================================================================

TEST(ContainerBidiTest, CjkIdeograph_LtrBase) {
    // U+4E2D '中'
    EXPECT_EQ(classify_bidi_level("\xE4\xB8\xAD", 0), 0);
    // U+6587 '文'
    EXPECT_EQ(classify_bidi_level("\xE6\x96\x87", 0), 0);
}

TEST(ContainerBidiTest, CjkIdeograph_RtlBase) {
    EXPECT_EQ(classify_bidi_level("\xE4\xB8\xAD", 1), 2);  // '中'
    EXPECT_EQ(classify_bidi_level("\xE6\x96\x87", 1), 2);  // '文'
}

TEST(ContainerBidiTest, CjkBoundaryStart) {
    // U+4E00 (一) = 0xE4 0xB8 0x80 — first CJK unified ideograph
    EXPECT_EQ(classify_bidi_level("\xE4\xB8\x80", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE4\xB8\x80", 1), 2);
}

TEST(ContainerBidiTest, CjkBoundaryEnd) {
    // U+9FFF (last CJK unified ideograph) = 0xE9 0xBF 0xBF
    EXPECT_EQ(classify_bidi_level("\xE9\xBF\xBF", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE9\xBF\xBF", 1), 2);
}

// ============================================================================
// Hiragana (U+3040–U+309F)
// ============================================================================

TEST(ContainerBidiTest, Hiragana_LtrBase) {
    // U+3042 'あ' = 0xE3 0x81 0x82
    EXPECT_EQ(classify_bidi_level("\xE3\x81\x82", 0), 0);
    // U+3093 'ん' = 0xE3 0x82 0x93
    EXPECT_EQ(classify_bidi_level("\xE3\x82\x93", 0), 0);
}

TEST(ContainerBidiTest, Hiragana_RtlBase) {
    EXPECT_EQ(classify_bidi_level("\xE3\x81\x82", 1), 2);
    EXPECT_EQ(classify_bidi_level("\xE3\x82\x93", 1), 2);
}

TEST(ContainerBidiTest, HiraganaRepeatMark) {
    // U+309D 'ゝ' = 0xE3 0x82 0x9D
    EXPECT_EQ(classify_bidi_level("\xE3\x82\x9D", 0), 0);
    // U+309E 'ゞ' = 0xE3 0x82 0x9E
    EXPECT_EQ(classify_bidi_level("\xE3\x82\x9E", 0), 0);
}

// ============================================================================
// Katakana (U+30A0–U+30FF)
// ============================================================================

TEST(ContainerBidiTest, Katakana_LtrBase) {
    // U+30A2 'ア' = 0xE3 0x82 0xA2
    EXPECT_EQ(classify_bidi_level("\xE3\x82\xA2", 0), 0);
    // U+30F3 'ン' = 0xE3 0x83 0xB3
    EXPECT_EQ(classify_bidi_level("\xE3\x83\xB3", 0), 0);
}

TEST(ContainerBidiTest, Katakana_RtlBase) {
    EXPECT_EQ(classify_bidi_level("\xE3\x82\xA2", 1), 2);
    EXPECT_EQ(classify_bidi_level("\xE3\x83\xB3", 1), 2);
}

TEST(ContainerBidiTest, KatakanaStart) {
    // U+30A0 '゠' = 0xE3 0x82 0x80 — first katakana
    EXPECT_EQ(classify_bidi_level("\xE3\x82\x80", 1), 2);
}

TEST(ContainerBidiTest, KatakanaEnd) {
    // U+30FF 'ヿ' = 0xE3 0x83 0xBF — last katakana
    EXPECT_EQ(classify_bidi_level("\xE3\x83\xBF", 1), 2);
}

// ============================================================================
// Hangul Syllables (U+AC00–U+D7A3)
// ============================================================================

TEST(ContainerBidiTest, Hangul_LtrBase) {
    // U+D55C '한' = 0xED 0x95 0x9C
    EXPECT_EQ(classify_bidi_level("\xED\x95\x9C", 0), 0);
    // U+AE00 '글' = 0xEA 0xB8 0x80
    EXPECT_EQ(classify_bidi_level("\xEA\xB8\x80", 0), 0);
}

TEST(ContainerBidiTest, Hangul_RtlBase) {
    EXPECT_EQ(classify_bidi_level("\xED\x95\x9C", 1), 2);
    EXPECT_EQ(classify_bidi_level("\xEA\xB8\x80", 1), 2);
}

TEST(ContainerBidiTest, HangulBoundaryStart) {
    // U+AC00 '가' = 0xEA 0xB0 0x80 — first hangul syllable
    EXPECT_EQ(classify_bidi_level("\xEA\xB0\x80", 1), 2);
}

TEST(ContainerBidiTest, HangulBoundaryEnd) {
    // U+D7A3 '힣' = 0xED 0x9E 0xA3 — last hangul syllable
    EXPECT_EQ(classify_bidi_level("\xED\x9E\xA3", 1), 2);
}

// ============================================================================
// Non-CJK non-ASCII — fall through to base_level
// (These characters do NOT match any fast-path range)
// ============================================================================

TEST(ContainerBidiTest, ArabicFallthrough) {
    // U+0639 'ع' = 0xD8 0xB9
    EXPECT_EQ(classify_bidi_level("\xD8\xB9", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xD8\xB9", 1), 1);
}

TEST(ContainerBidiTest, CyrillicFallthrough) {
    // U+041F 'П' = 0xD0 0x9F
    EXPECT_EQ(classify_bidi_level("\xD0\x9F", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xD0\x9F", 1), 1);
}

TEST(ContainerBidiTest, GreekFallthrough) {
    // U+03B1 'α' = 0xCE 0xB1
    EXPECT_EQ(classify_bidi_level("\xCE\xB1", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xCE\xB1", 1), 1);
}

TEST(ContainerBidiTest, HebrewFallthrough) {
    // U+05D0 'א' = 0xD7 0x90
    EXPECT_EQ(classify_bidi_level("\xD7\x90", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xD7\x90", 1), 1);
}

TEST(ContainerBidiTest, Latin1SupplementFallthrough) {
    // U+00FF 'ÿ' = 0xC3 0xBF — 2-byte UTF-8, NOT in any fast-path
    EXPECT_EQ(classify_bidi_level("\xC3\xBF", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xC3\xBF", 1), 1);
}

TEST(ContainerBidiTest, HalfwidthKatakanaFallthrough) {
    // U+FF66 'ｦ' (Halfwidth Katakana) = 0xEF 0xBD 0xA6
    // NOT matched by the hiragana/katakana check (first byte is 0xEF, not 0xE3)
    EXPECT_EQ(classify_bidi_level("\xEF\xBD\xA6", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xEF\xBD\xA6", 1), 1);
}

TEST(ContainerBidiTest, CjkCompatibilityIdeographsFallthrough) {
    // U+F900 '豈' (CJK Compatibility Ideograph) = 0xEF 0xA4 0x80
    // NOT matched by CJK check (first byte is 0xEF, not in 0xE4-0xE9)
    EXPECT_EQ(classify_bidi_level("\xEF\xA4\x80", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xEF\xA4\x80", 1), 1);
}

// ============================================================================
// Non-CJK 0xE3 sub-ranges — E3 0x80 and E3 >= 0x84 are NOT matched
// ============================================================================

TEST(ContainerBidiTest, CjkSymbolsFallthrough) {
    // U+3000 (ideographic space) = 0xE3 0x80 0x80
    // c0=0xE3, c1=0x80 → NOT in range (c1 >= 0x81 fails)
    EXPECT_EQ(classify_bidi_level("\xE3\x80\x80", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE3\x80\x80", 1), 1);
}

TEST(ContainerBidiTest, HangulCompatibilityJamoFallthrough) {
    // U+3130 '㌰' (Hangul Compatibility Jamo) = 0xE3 0x84 0xB0
    // c0=0xE3, c1=0x84 → NOT in range (c1 <= 0x83 fails)
    EXPECT_EQ(classify_bidi_level("\xE3\x84\xB0", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE3\x84\xB0", 1), 1);
}

TEST(ContainerBidiTest, CjkExtensionA) {
    // U+3400 '㐀' (CJK Extension A start) = 0xE3 0x90 0x80
    // c0=0xE3, c1=0x90 → matches CJK Extension A fast-path condition
    EXPECT_EQ(classify_bidi_level("\xE3\x90\x80", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE3\x90\x80", 1), 2);
}

TEST(ContainerBidiTest, CjkExtensionA_End) {
    // U+4DBF (last CJK Extension A) = 0xE4 0xB6 0xBF
    // c0=0xE4 → matches CJK Unified Ideographs range (0xE4-0xE9)
    EXPECT_EQ(classify_bidi_level("\xE4\xB6\xBF", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE4\xB6\xBF", 1), 2);
}

// ============================================================================
// Text with multiple characters — only first byte matters
// ============================================================================

TEST(ContainerBidiTest, MixedText_FirstCharAsciiStrong) {
    // LTR letter + CJK text → ASCII strong wins
    EXPECT_EQ(classify_bidi_level("A\xE4\xB8\xAD", 0), 0);  // "A中"
    EXPECT_EQ(classify_bidi_level("A\xE4\xB8\xAD", 1), 2);
}

TEST(ContainerBidiTest, MixedText_FirstCharCjk) {
    // CJK + ASCII → CJK wins
    EXPECT_EQ(classify_bidi_level("\xE4\xB8\xAD""A", 0), 0);  // "中A"
    EXPECT_EQ(classify_bidi_level("\xE4\xB8\xAD""A", 1), 2);
}

TEST(ContainerBidiTest, MixedText_FirstCharSpace) {
    // Space is neutral → falls through to base_level
    EXPECT_EQ(classify_bidi_level(" Hello", 0), 0);
    EXPECT_EQ(classify_bidi_level(" Hello", 1), 1);
}

TEST(ContainerBidiTest, JapaneseSentence) {
    // "これは日本語です" — first char 'こ' (U+3053) is hiragana
    // こ = 0xE3 0x81 0x93
    EXPECT_EQ(classify_bidi_level("\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF"
                                  "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
                                  "\xE3\x81\xA7\xE3\x81\x99", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF"
                                  "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
                                  "\xE3\x81\xA7\xE3\x81\x99", 1), 2);
}

TEST(ContainerBidiTest, KoreanSentence) {
    // "한글" — first char '한' (U+D55C) is hangul
    EXPECT_EQ(classify_bidi_level("\xED\x95\x9C\xEA\xB8\x80", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xED\x95\x9C\xEA\xB8\x80", 1), 2);
}

// ============================================================================
// Non-first character tests (confirm first-char-only semantics)
// ============================================================================

TEST(ContainerBidiTest, SecondCharDoesNotMatter) {
    // Text starting with neutral, followed by strong — first char (neutral) wins
    EXPECT_EQ(classify_bidi_level(" A", 0), 0);  // space + 'A'
    EXPECT_EQ(classify_bidi_level("\xE3\x80\x80\xE4\xB8\xAD", 0), 0);  // U+3000 + U+4E2D
}

// ============================================================================
// Malformed / edge-case UTF-8 sequences (safety)
// ============================================================================

TEST(ContainerBidiTest, TruncatedCjkUtf8) {
    // Only the first byte of a CJK character, no continuation bytes
    // c0=0xE4 → matches CJK range → returns 0/2
    EXPECT_EQ(classify_bidi_level("\xE4", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xE4", 1), 2);
}

TEST(ContainerBidiTest, TruncatedHangulUtf8) {
    // Only first byte of a Hangul character
    EXPECT_EQ(classify_bidi_level("\xEA", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xEA", 1), 2);
}

TEST(ContainerBidiTest, SoloContinuationByte) {
    // 0x80 alone is >= 0x80 and non-ASCII; c1 would be null terminator (0x00)
    // No fast-path matches, so falls through to base_level
    EXPECT_EQ(classify_bidi_level("\x80", 0), 0);
    EXPECT_EQ(classify_bidi_level("\x80", 1), 1);
}

TEST(ContainerBidiTest, FourByteUtf8) {
    // U+20000 (CJK Extension B) = 0xF0 0xA0 0x80 0x80
    // First byte 0xF0 is non-ASCII, no fast-path range matches → falls through
    EXPECT_EQ(classify_bidi_level("\xF0\xA0\x80\x80", 0), 0);
    EXPECT_EQ(classify_bidi_level("\xF0\xA0\x80\x80", 1), 1);
}
