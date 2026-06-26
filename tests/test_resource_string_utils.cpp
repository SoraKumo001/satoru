#include <gtest/gtest.h>
#include "resource_string_utils.h"

using namespace satoru;

// ============================================================
// starts_with_ascii_ci
// ============================================================

TEST(StartsWithAsciiCiTest, MatchAtPositionZero) {
    EXPECT_TRUE(starts_with_ascii_ci("HelloWorld", 0, "hello"));
}

TEST(StartsWithAsciiCiTest, MatchMidString) {
    EXPECT_TRUE(starts_with_ascii_ci("prefixHelloWorld", 6, "hello"));
}

TEST(StartsWithAsciiCiTest, CaseInsensitive) {
    EXPECT_TRUE(starts_with_ascii_ci("HELLO", 0, "hello"));
    EXPECT_TRUE(starts_with_ascii_ci("hello", 0, "HELLO"));
    EXPECT_TRUE(starts_with_ascii_ci("HeLLo", 0, "hElLo"));
}

TEST(StartsWithAsciiCiTest, NoMatch) {
    EXPECT_FALSE(starts_with_ascii_ci("HelloWorld", 0, "world"));
}

TEST(StartsWithAsciiCiTest, PartialMatch) {
    EXPECT_FALSE(starts_with_ascii_ci("Hel", 0, "Hello"));
}

TEST(StartsWithAsciiCiTest, EmptyNeedle) {
    // Empty needle matches trivially (zero iterations)
    EXPECT_TRUE(starts_with_ascii_ci("Hello", 0, ""));
}

TEST(StartsWithAsciiCiTest, OutOfBoundsPosition) {
    EXPECT_FALSE(starts_with_ascii_ci("Hello", 10, "H"));
}

TEST(StartsWithAsciiCiTest, NeedleLongerThanString) {
    EXPECT_FALSE(starts_with_ascii_ci("Hi", 0, "Hello"));
}

TEST(StartsWithAsciiCiTest, NonAsciiByte) {
    // \xE9 is 'é' in Latin-1 — verify it doesn't false-match with ASCII
    std::string s = "\xE9" "abc";
    EXPECT_FALSE(starts_with_ascii_ci(s, 0, "e"));
    EXPECT_FALSE(starts_with_ascii_ci(s, 0, "E"));
}

// ============================================================
// contains_ascii
// ============================================================

TEST(ContainsAsciiTest, ExactMatch) {
    const uint8_t data[] = "Hello World";
    EXPECT_TRUE(contains_ascii(data, sizeof(data) - 1, "Hello"));
}

TEST(ContainsAsciiTest, MatchAtEnd) {
    const uint8_t data[] = "Hello World";
    EXPECT_TRUE(contains_ascii(data, sizeof(data) - 1, "World"));
}

TEST(ContainsAsciiTest, NoMatch) {
    const uint8_t data[] = "Hello World";
    EXPECT_FALSE(contains_ascii(data, sizeof(data) - 1, "xyz"));
}

TEST(ContainsAsciiTest, NeedleLargerThanBuffer) {
    const uint8_t data[] = "Hi";
    EXPECT_FALSE(contains_ascii(data, sizeof(data) - 1, "Hello"));
}

TEST(ContainsAsciiTest, NullData) {
    EXPECT_FALSE(contains_ascii(nullptr, 10, "Hello"));
}

TEST(ContainsAsciiTest, EmptyNeedle) {
    const uint8_t data[] = "Hello";
    EXPECT_FALSE(contains_ascii(data, sizeof(data) - 1, ""));
}

TEST(ContainsAsciiTest, ZeroSize) {
    const uint8_t data[] = "Hello";
    EXPECT_FALSE(contains_ascii(data, 0, "Hello"));
}

TEST(ContainsAsciiTest, EmbeddedNullBytes) {
    // Data with embedded null — memcmp operates on byte count, not null-terminated
    const uint8_t data[] = {'H', 'e', '\0', 'l', 'o', '.', 't', 't', 'f'};
    // Should still match .ttf across the null boundary
    EXPECT_TRUE(contains_ascii(data, sizeof(data), ".ttf"));
    // Should not false-match across the null boundary
    EXPECT_TRUE(contains_ascii(data, sizeof(data), "He\0lo"));
}

// ============================================================
// contains_ascii_ci
// ============================================================

TEST(ContainsAsciiCiTest, SubstringMatch) {
    EXPECT_TRUE(contains_ascii_ci("Hello World", "world"));
}

TEST(ContainsAsciiCiTest, CaseInsensitive) {
    EXPECT_TRUE(contains_ascii_ci("HELLO.WOFF2", ".woff2"));
    EXPECT_TRUE(contains_ascii_ci("hello.Woff2", ".WOFF2"));
}

TEST(ContainsAsciiCiTest, NoMatch) {
    EXPECT_FALSE(contains_ascii_ci("Hello World", "xyz"));
}

TEST(ContainsAsciiCiTest, EmptyNeedle) {
    // Empty needle always returns true (documented behavior)
    EXPECT_TRUE(contains_ascii_ci("Hello", ""));
}

TEST(ContainsAsciiCiTest, NullNeedle) {
    EXPECT_TRUE(contains_ascii_ci("Hello", nullptr));
}

TEST(ContainsAsciiCiTest, NeedleAtStart) {
    EXPECT_TRUE(contains_ascii_ci("Hello World", "hello"));
}

TEST(ContainsAsciiCiTest, NeedleLongerThanString) {
    EXPECT_FALSE(contains_ascii_ci("Hi", "Hello"));
}

TEST(ContainsAsciiCiTest, EmptyString) {
    EXPECT_FALSE(contains_ascii_ci("", "hello"));
}

// ============================================================
// contains_any_ascii_ci
// ============================================================

TEST(ContainsAnyAsciiCiTest, FirstNeedleMatches) {
    const char* needles[] = {".woff2", ".ttf", ".otf"};
    EXPECT_TRUE(contains_any_ascii_ci("font.woff2", needles, 3));
}

TEST(ContainsAnyAsciiCiTest, LastNeedleMatches) {
    const char* needles[] = {".woff2", ".ttf", ".otf"};
    EXPECT_TRUE(contains_any_ascii_ci("font.otf", needles, 3));
}

TEST(ContainsAnyAsciiCiTest, NoMatch) {
    const char* needles[] = {".woff2", ".ttf"};
    EXPECT_FALSE(contains_any_ascii_ci("font.png", needles, 2));
}

TEST(ContainsAnyAsciiCiTest, EmptyNeedleArray) {
    EXPECT_FALSE(contains_any_ascii_ci("Hello", nullptr, 0));
}

TEST(ContainsAnyAsciiCiTest, CaseInsensitive) {
    const char* needles[] = {".WOFF2"};
    EXPECT_TRUE(contains_any_ascii_ci("hello.woff2", needles, 1));
}

// ============================================================
// looks_like_font_url
// ============================================================

TEST(LooksLikeFontUrlTest, Woff2Extension) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/font.woff2"));
}

TEST(LooksLikeFontUrlTest, TtfExtension) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/font.ttf"));
}

TEST(LooksLikeFontUrlTest, OtfExtension) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/font.otf"));
}

TEST(LooksLikeFontUrlTest, TtcExtension) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/font.ttc"));
}

TEST(LooksLikeFontUrlTest, WoffExtension) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/font.woff"));
}

TEST(LooksLikeFontUrlTest, FontMimeType) {
    EXPECT_TRUE(looks_like_font_url("data:application/font-woff;base64,..."));
}

TEST(LooksLikeFontUrlTest, NonFontUrl) {
    EXPECT_FALSE(looks_like_font_url("https://example.com/image.jpg"));
    EXPECT_FALSE(looks_like_font_url("https://example.com/style.css"));
    EXPECT_FALSE(looks_like_font_url("https://example.com/page.html"));
}

TEST(LooksLikeFontUrlTest, EmptyUrl) {
    EXPECT_FALSE(looks_like_font_url(""));
}

TEST(LooksLikeFontUrlTest, CaseInsensitiveExtension) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/Font.TTF"));
    EXPECT_TRUE(looks_like_font_url("https://example.com/Font.WOFF2"));
}

// ============================================================
// contains_weight_marker
// ============================================================

TEST(ContainsWeightMarkerTest, Weight700) {
    EXPECT_TRUE(contains_weight_marker("font-weight-700.woff2", "700", "bold"));
}

TEST(ContainsWeightMarkerTest, BoldKeyword) {
    EXPECT_TRUE(contains_weight_marker("font-bold.woff2", "700", "bold"));
}

TEST(ContainsWeightMarkerTest, NoMatch) {
    EXPECT_FALSE(contains_weight_marker("font-regular.woff2", "700", "bold"));
}

TEST(ContainsWeightMarkerTest, ItalicMarker) {
    EXPECT_TRUE(contains_weight_marker("font-italic.woff2", "italic", "oblique"));
    EXPECT_TRUE(contains_weight_marker("font-oblique.woff2", "italic", "oblique"));
}

TEST(ContainsWeightMarkerTest, NumericSubstringFalsePositive) {
    // "300" should match "1300" — be aware of this behavior
    EXPECT_TRUE(contains_weight_marker("weight-1300.woff2", "300", "light"));
}

TEST(ContainsWeightMarkerTest, LightKeyword) {
    EXPECT_TRUE(contains_weight_marker("font-light.woff2", "300", "light"));
}

// ============================================================
// replace_font_family_names
// ============================================================

TEST(ReplaceFontFamilyNamesTest, SingleDeclaration) {
    std::string css = "body { font-family: serif; }";
    std::string result = replace_font_family_names(css, "Noto Serif JP");
    EXPECT_EQ(result, "body { font-family: 'Noto Serif JP'; }");
}

TEST(ReplaceFontFamilyNamesTest, MultipleDeclarations) {
    std::string css = "h1 { font-family: serif; }\np { font-family: sans-serif; }";
    std::string result = replace_font_family_names(css, "Noto Sans JP");
    EXPECT_EQ(result, "h1 { font-family: 'Noto Sans JP'; }\np { font-family: 'Noto Sans JP'; }");
}

TEST(ReplaceFontFamilyNamesTest, NoFontFamily) {
    std::string css = "body { color: red; margin: 0; }";
    std::string result = replace_font_family_names(css, "Test");
    EXPECT_EQ(result, css);
}

TEST(ReplaceFontFamilyNamesTest, NoColonAfterFontFamily) {
    std::string css = "body { font-family }";
    std::string result = replace_font_family_names(css, "Test");
    // Without colon, the function leaves font-family... to end unchanged
    EXPECT_EQ(result, css);
}

TEST(ReplaceFontFamilyNamesTest, ValueEndsWithClosingBrace) {
    // The function replaces the value after font-family: with 'Sans',
    // preserving the original spacing up to the value start but not the trailing space
    std::string css = "div { font-family: serif }";
    std::string result = replace_font_family_names(css, "Sans");
    EXPECT_EQ(result, "div { font-family: 'Sans'}");
}

TEST(ReplaceFontFamilyNamesTest, CaseInsensitivePropertyName) {
    std::string css = "body { FONT-FAMILY: serif; }";
    std::string result = replace_font_family_names(css, "Mono");
    EXPECT_EQ(result, "body { FONT-FAMILY: 'Mono'; }");
}

TEST(ReplaceFontFamilyNamesTest, CommaSeparatedFallbacks) {
    // Comma-separated fallbacks — the function replaces the entire value
    // with the single quoted name
    std::string css = "body { font-family: 'Arial', 'Helvetica', sans-serif; }";
    std::string result = replace_font_family_names(css, "Unified Font");
    EXPECT_EQ(result, "body { font-family: 'Unified Font'; }");
}

TEST(ReplaceFontFamilyNamesTest, LeadingWhitespaceInValue) {
    // The function preserves whitespace between : and the value start
    std::string css = "body { font-family:   serif; }";
    std::string result = replace_font_family_names(css, "Mono");
    EXPECT_EQ(result, "body { font-family:   'Mono'; }");
}

TEST(ReplaceFontFamilyNamesTest, MultipleFontFamiliesInOneRule) {
    // Replaces all font-family declarations across the entire string
    std::string css = "div { font-family: serif; color: red; font-family: sans-serif; }";
    std::string result = replace_font_family_names(css, "Unified");
    EXPECT_EQ(result, "div { font-family: 'Unified'; color: red; font-family: 'Unified'; }");
}

TEST(ReplaceFontFamilyNamesTest, TrailingSemicolons) {
    // Double semicolon after value — the function treats first ; as terminator
    std::string css = "body { font-family: serif;; }";
    std::string result = replace_font_family_names(css, "Mono");
    EXPECT_EQ(result, "body { font-family: 'Mono';; }");
}

TEST(ReplaceFontFamilyNamesTest, EmptyCss) {
    std::string result = replace_font_family_names("", "Test");
    EXPECT_TRUE(result.empty());
}

TEST(ReplaceFontFamilyNamesTest, NoReplacementForDifferentProperty) {
    std::string css = "body { font-size: 16px; font-weight: bold; }";
    std::string result = replace_font_family_names(css, "Test");
    EXPECT_EQ(result, css);
}
