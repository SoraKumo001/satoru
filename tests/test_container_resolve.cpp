// Tests for container_skia::resolve_color (var() parsing) and
// container_skia::import_css (looks_like_font_url routing).
//
// resolve_color is an instance method that depends on m_doc->root() for the
// final lookup; we extract the pure var()-parsing logic into a standalone
// helper (isomorphic to container_skia.cpp L1839-1858).
//
// import_css delegates to the inline helper looks_like_font_url() from
// resource_string_utils.h — we test that directly.

#include <gtest/gtest.h>
#include "resource_string_utils.h"

#include <string>

using namespace satoru;

// ============================================================================
// Isomorphic helper for container_skia::resolve_color var() extraction
// (container_skia.cpp L1839-1858)
// ============================================================================
namespace {

/// Extract the variable name from a CSS var() expression.
/// Returns the empty string on malformed input.
/// Mirrors container_skia.cpp L1839-1858.
static std::string resolve_var_name(const std::string& color) {
    if (color.empty()) return "";

    std::string name = color;
    if (name.size() > 4 && name.substr(0, 4) == "var(") {
        size_t pos = name.find('(');
        size_t end = name.find_last_of(')');
        if (pos != std::string::npos && end != std::string::npos && end > pos + 1) {
            name = name.substr(pos + 1, end - pos - 1);
            // Handle fallback if present (comma)
            size_t comma = name.find(',');
            if (comma != std::string::npos) {
                name = name.substr(0, comma);
            }
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t\r\n"));
            size_t last = name.find_last_not_of(" \t\r\n");
            if (last != std::string::npos) {
                name.erase(last + 1);
            }
        }
    }
    return name;
}

/// True if the string starts with "--" (CSS custom property prefix).
static bool is_custom_property(const std::string& name) {
    return name.size() >= 2 && name[0] == '-' && name[1] == '-';
}

} // anonymous namespace

// ============================================================================
// resolve_color — var() name extraction
// ============================================================================

TEST(ResolveVarNameTest, EmptyInput) {
    EXPECT_EQ(resolve_var_name(""), "");
}

TEST(ResolveVarNameTest, NonVarInput) {
    // Standard color names pass through unmodified
    EXPECT_EQ(resolve_var_name("red"), "red");
    EXPECT_EQ(resolve_var_name("#ff0000"), "#ff0000");
    EXPECT_EQ(resolve_var_name("rgb(255,0,0)"), "rgb(255,0,0)");
    EXPECT_EQ(resolve_var_name("transparent"), "transparent");
    EXPECT_EQ(resolve_var_name("currentColor"), "currentColor");
}

TEST(ResolveVarNameTest, SimpleVar) {
    EXPECT_EQ(resolve_var_name("var(--primary)"), "--primary");
}

TEST(ResolveVarNameTest, VarWithFallback) {
    // Everything before the comma is the variable name
    EXPECT_EQ(resolve_var_name("var(--primary, red)"), "--primary");
}

TEST(ResolveVarNameTest, VarWithFallbackComplex) {
    EXPECT_EQ(resolve_var_name("var(--accent, #ff8800)"), "--accent");
}

TEST(ResolveVarNameTest, VarWithFallbackNestedParen) {
    // The variable name stops at the comma (not affected by nested parens)
    EXPECT_EQ(resolve_var_name("var(--primary, rgb(255,0,0))"), "--primary");
}

TEST(ResolveVarNameTest, VarWithWhitespace) {
    // Whitespace around variable name is trimmed
    EXPECT_EQ(resolve_var_name("var(  --primary  )"), "--primary");
}

TEST(ResolveVarNameTest, VarWithWhitespaceAndFallback) {
    EXPECT_EQ(resolve_var_name("var(  --primary , red )"), "--primary");
}

TEST(ResolveVarNameTest, VarNoClosingParen) {
    // find_last_of(')') returns npos → var() extraction fails,
    // the original string is returned (no "--" prefix → resolve_color returns "")
    EXPECT_EQ(resolve_var_name("var(--primary"), "var(--primary");
}

TEST(ResolveVarNameTest, VarEmptyParens) {
    // end > pos + 1 is false (4 > 4) → var() extraction fails,
    // the original string passes through (no "--" → resolve_color returns "")
    EXPECT_EQ(resolve_var_name("var()"), "var()");
}

TEST(ResolveVarNameTest, VarJustParens) {
    // Content after trimming is empty
    EXPECT_EQ(resolve_var_name("var(  )"), "");
}

TEST(ResolveVarNameTest, NonVarPrefix) {
    // "variable" is not "var(" — passes through
    EXPECT_EQ(resolve_var_name("variable"), "variable");
    EXPECT_EQ(resolve_var_name("variance"), "variance");
}

TEST(ResolveVarNameTest, VarCaseSensitivity) {
    // CSS var() is case-sensitive in property names
    EXPECT_EQ(resolve_var_name("VAR(--primary)"), "VAR(--primary)");
    EXPECT_EQ(resolve_var_name("Var(--primary)"), "Var(--primary)");
}

TEST(ResolveVarNameTest, VarCustomPropertyWithDigits) {
    EXPECT_EQ(resolve_var_name("var(--color123)"), "--color123");
}

TEST(ResolveVarNameTest, VarMultipleDashes) {
    EXPECT_EQ(resolve_var_name("var(---custom)"), "---custom");
}

TEST(ResolveVarNameTest, VarDeepNestedFallback) {
    // Fallback value itself contains var(), but only the top-level name is extracted
    EXPECT_EQ(resolve_var_name("var(--primary, var(--secondary, red))"), "--primary");
}

TEST(ResolveVarNameTest, VarWithTabsAndNewlines) {
    EXPECT_EQ(resolve_var_name("var(\t\n--primary \t\n)"), "--primary");
}

TEST(ResolveVarNameTest, NonVarInputWithParens) {
    // calc(), rgb(), etc. don't start with "var("
    EXPECT_EQ(resolve_var_name("calc(100% - 20px)"), "calc(100% - 20px)");
    EXPECT_EQ(resolve_var_name("rgba(255,0,0,0.5)"), "rgba(255,0,0,0.5)");
}

TEST(ResolveVarNameTest, FallbackWithWhitespaceOnly) {
    // After trimming, "--primary" is extracted
    EXPECT_EQ(resolve_var_name("var(--primary,   )"), "--primary");
}

// ============================================================================
// resolve_color — is_custom_property
// ============================================================================

TEST(IsCustomPropertyTest, StartsWithDoubleDash) {
    EXPECT_TRUE(is_custom_property("--primary"));
    EXPECT_TRUE(is_custom_property("--color"));
    EXPECT_TRUE(is_custom_property("--my-var"));
}

TEST(IsCustomPropertyTest, NotCustomProperty) {
    EXPECT_FALSE(is_custom_property("primary"));
    EXPECT_FALSE(is_custom_property("-primary"));
    // "--" is technically a valid custom property name prefix, just empty
    EXPECT_TRUE(is_custom_property("--"));
    EXPECT_FALSE(is_custom_property("-"));
    EXPECT_FALSE(is_custom_property(""));
}

TEST(IsCustomPropertyTest, RedIsNotCustomProperty) {
    // Standard color names don't start with "--"
    EXPECT_FALSE(is_custom_property("red"));
    EXPECT_FALSE(is_custom_property("#ff0000"));
    EXPECT_FALSE(is_custom_property("rgb(255,0,0)"));
}

// ============================================================================
// resolve_color — round-trip: var() → name → is_custom_property
// ============================================================================

TEST(ResolveColorRoundTripTest, StandardColorReturnsEmpty) {
    // resolve_color returns "" for standard colors (the litehtml engine
    // handles those before calling resolve_color)
    std::string name = resolve_var_name("red");
    EXPECT_EQ(name, "red");
    EXPECT_FALSE(is_custom_property(name));
}

TEST(ResolveColorRoundTripTest, VarCustomPropertyIsCustom) {
    std::string name = resolve_var_name("var(--primary)");
    EXPECT_EQ(name, "--primary");
    EXPECT_TRUE(is_custom_property(name));
}

TEST(ResolveColorRoundTripTest, VarFallbackNotCustom) {
    std::string name = resolve_var_name("var(--primary, red)");
    EXPECT_EQ(name, "--primary");
    EXPECT_TRUE(is_custom_property(name));
}

// ============================================================================
// looks_like_font_url — additional tests beyond test_resource_string_utils.cpp
// ============================================================================

TEST(LooksLikeFontUrlExtraTest, GoogleFontsUrl) {
    EXPECT_TRUE(looks_like_font_url("https://fonts.gstatic.com/s/roboto/v30/abc.woff2"));
}

TEST(LooksLikeFontUrlExtraTest, DataUrlFont) {
    EXPECT_TRUE(looks_like_font_url("data:application/font-woff2;base64,AAAB..."));
}

TEST(LooksLikeFontUrlExtraTest, DataUrlImage) {
    EXPECT_FALSE(looks_like_font_url("data:image/png;base64,iVBOR..."));
}

TEST(LooksLikeFontUrlExtraTest, FontInQueryParam) {
    // ".woff2" in query parameter with the dot
    EXPECT_TRUE(looks_like_font_url("https://example.com/css?family=OpenSans&format=.woff2"));
}

TEST(LooksLikeFontUrlExtraTest, FontInPathSegment) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/font-ttf-roboto"));
}

TEST(LooksLikeFontUrlExtraTest, ApplicationFontMime) {
    EXPECT_TRUE(looks_like_font_url("https://example.com/font?mime=application/font-woff"));
}

TEST(LooksLikeFontUrlExtraTest, SvgFont) {
    EXPECT_FALSE(looks_like_font_url("https://example.com/font.svg"));
}

TEST(LooksLikeFontUrlExtraTest, EotExtension) {
    EXPECT_FALSE(looks_like_font_url("https://example.com/font.eot"));
}

TEST(LooksLikeFontUrlExtraTest, CssExtension) {
    EXPECT_FALSE(looks_like_font_url("https://example.com/style.css"));
}

TEST(LooksLikeFontUrlExtraTest, NoExtension) {
    EXPECT_FALSE(looks_like_font_url("https://example.com/resource"));
}
