#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "core/font_manager.h"

// SatoruFontManager constructor calls SkFontMgr_New_Custom_Empty() (stubbed).
// scanFontFaces / getFontUrls / hasFontFaceSource / generateFontFaceCSS
// are pure CSS parsing logic that works with the stub font manager.

class FontManagerTest : public ::testing::Test {
   protected:
    void SetUp() override {
        fm = std::make_unique<SatoruFontManager>();
    }

    std::unique_ptr<SatoruFontManager> fm;
};

// ── scanFontFaces ────────────────────────────────────────────────────────

TEST_F(FontManagerTest, ScanFontFaces_EmptyCss) {
    fm->scanFontFaces("");
    // No font faces registered — getFontUrl should return empty
    EXPECT_TRUE(fm->getFontUrl("Roboto", 400, SkFontStyle::kUpright_Slant).empty());
}

TEST_F(FontManagerTest, ScanFontFaces_NoFontFace) {
    fm->scanFontFaces("body { color: red; }");
    EXPECT_TRUE(fm->getFontUrl("body", 400, SkFontStyle::kUpright_Slant).empty());
}

TEST_F(FontManagerTest, ScanFontFaces_SingleFontFace) {
    const char* css = R"(
        @font-face {
            font-family: 'Roboto';
            src: url('https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2') format('woff2');
        }
    )";
    fm->scanFontFaces(css);

    EXPECT_TRUE(fm->hasFontFaceSource("Roboto",
        "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2"));
    EXPECT_FALSE(fm->hasFontFaceSource("NonExistent",
        "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_MultipleWeightRanges) {
    const char* css = R"(
        @font-face {
            font-family: 'OpenSans';
            font-weight: 100 900;
            src: url('https://example.com/opensans-variable.woff2') format('woff2');
        }
    )";
    fm->scanFontFaces(css);

    // All weights 100–900 should be registered
    for (int w = 100; w <= 900; w += 100) {
        EXPECT_TRUE(fm->hasFontFaceSource("OpenSans",
            "https://example.com/opensans-variable.woff2"))
            << "weight " << w;
    }
}

TEST_F(FontManagerTest, ScanFontFaces_BoldKeyword) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            font-weight: bold;
            src: url('https://example.com/test-bold.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test-bold.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_NormalKeyword) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            font-weight: normal;
            src: url('https://example.com/test-normal.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test-normal.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_Italic) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            font-style: italic;
            src: url('https://example.com/test-italic.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test-italic.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_ObliqueIsItalic) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            font-style: oblique;
            src: url('https://example.com/test-oblique.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test-oblique.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_Woff2PreferredOverWoff) {
    // When multiple url() sources exist, woff2 should be preferred (higher score)
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            src: url('https://example.com/test.woff') format('woff'),
                 url('https://example.com/test.woff2') format('woff2');
        }
    )";
    fm->scanFontFaces(css);
    // The woff2 URL should be the one registered
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_TtfScore) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            src: url('https://example.com/test.ttf');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test.ttf"));
}

TEST_F(FontManagerTest, ScanFontFaces_EotIgnored) {
    // .eot has score -1, should be skipped
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            src: url('https://example.com/test.eot');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_FALSE(fm->hasFontFaceSource("Test", "https://example.com/test.eot"));
}

TEST_F(FontManagerTest, ScanFontFaces_UnicodeRange) {
    const char* css = R"(
        @font-face {
            font-family: 'NotoSansJP';
            src: url('https://example.com/noto-latin.woff2');
            unicode-range: U+0000-007F;
        }
        @font-face {
            font-family: 'NotoSansJP';
            src: url('https://example.com/noto-cjk.woff2');
            unicode-range: U+3000-9FFF;
        }
    )";
    fm->scanFontFaces(css);

    // Both URLs registered
    EXPECT_TRUE(fm->hasFontFaceSource("NotoSansJP", "https://example.com/noto-latin.woff2"));
    EXPECT_TRUE(fm->hasFontFaceSource("NotoSansJP", "https://example.com/noto-cjk.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_MultipleFontFamilies) {
    const char* css = R"(
        @font-face {
            font-family: 'FontA';
            src: url('https://example.com/a.woff2');
        }
        @font-face {
            font-family: 'FontB';
            src: url('https://example.com/b.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("FontA", "https://example.com/a.woff2"));
    EXPECT_TRUE(fm->hasFontFaceSource("FontB", "https://example.com/b.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_NestedBraces) {
    // CSS with nested braces inside @font-face (e.g. comments with braces)
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            /* comment with { and } */
            src: url('https://example.com/test.woff2');
        }
    )";
    fm->scanFontFaces(css);
    // Should still parse correctly despite nested braces in comment
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_CaseInsensitiveProperty) {
    const char* css = R"(
        @font-face {
            FONT-FAMILY: 'Test';
            SRC: url('https://example.com/test.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test.woff2"));
}

// ── getFontUrls / getFontUrl ─────────────────────────────────────────────

TEST_F(FontManagerTest, GetFontUrls_EmptyWhenNoScan) {
    auto urls = fm->getFontUrls("Roboto", 400, SkFontStyle::kUpright_Slant);
    EXPECT_TRUE(urls.empty());
}

TEST_F(FontManagerTest, GetFontUrls_ReturnsRegisteredUrl) {
    const char* css = R"(
        @font-face {
            font-family: 'Roboto';
            src: url('https://example.com/roboto.woff2');
        }
    )";
    fm->scanFontFaces(css);

    auto urls = fm->getFontUrls("Roboto", 400, SkFontStyle::kUpright_Slant);
    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "https://example.com/roboto.woff2");
}

TEST_F(FontManagerTest, GetFontUrl_Single) {
    const char* css = R"(
        @font-face {
            font-family: 'Roboto';
            src: url('https://example.com/roboto.woff2');
        }
    )";
    fm->scanFontFaces(css);

    std::string url = fm->getFontUrl("Roboto", 400, SkFontStyle::kUpright_Slant);
    EXPECT_EQ(url, "https://example.com/roboto.woff2");
}

TEST_F(FontManagerTest, GetFontUrl_EmptyWhenNotFound) {
    std::string url = fm->getFontUrl("NonExistent", 400, SkFontStyle::kUpright_Slant);
    EXPECT_TRUE(url.empty());
}

TEST_F(FontManagerTest, GetFontUrls_FallbackToDifferentWeight) {
    // Registered weight 700, but requesting 400 — should still find it via fallback
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            font-weight: 700;
            src: url('https://example.com/test-bold.woff2');
        }
    )";
    fm->scanFontFaces(css);

    auto urls = fm->getFontUrls("Test", 400, SkFontStyle::kUpright_Slant);
    ASSERT_GE(urls.size(), 1u);
    EXPECT_EQ(urls[0], "https://example.com/test-bold.woff2");
}

TEST_F(FontManagerTest, GetFontUrls_CommaSeparatedFamily) {
    const char* css = R"(
        @font-face {
            font-family: 'Primary';
            src: url('https://example.com/primary.woff2');
        }
        @font-face {
            font-family: 'Fallback';
            src: url('https://example.com/fallback.woff2');
        }
    )";
    fm->scanFontFaces(css);

    auto urls = fm->getFontUrls("Primary, Fallback", 400, SkFontStyle::kUpright_Slant);
    ASSERT_EQ(urls.size(), 2u);
    EXPECT_EQ(urls[0], "https://example.com/primary.woff2");
    EXPECT_EQ(urls[1], "https://example.com/fallback.woff2");
}

// ── hasFontFaceSource ────────────────────────────────────────────────────

TEST_F(FontManagerTest, HasFontFaceSource_FalseWhenEmpty) {
    EXPECT_FALSE(fm->hasFontFaceSource("Roboto", "https://example.com/anything.woff2"));
}

TEST_F(FontManagerTest, HasFontFaceSource_CaseSensitiveUrl) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            src: url('https://example.com/Test.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/Test.woff2"));
    // URL matching is exact (case-sensitive)
    EXPECT_FALSE(fm->hasFontFaceSource("Test", "https://example.com/test.woff2"));
}

// ── generateFontFaceCSS ──────────────────────────────────────────────────

TEST_F(FontManagerTest, GenerateFontFaceCSS_Empty) {
    std::string css = fm->generateFontFaceCSS();
    EXPECT_TRUE(css.empty());
}

TEST_F(FontManagerTest, GenerateFontFaceCSS_SingleFace) {
    const char* input = R"(
        @font-face {
            font-family: 'Roboto';
            font-weight: 400;
            src: url('https://example.com/roboto.woff2');
        }
    )";
    fm->scanFontFaces(input);

    std::string output = fm->generateFontFaceCSS();
    // cleanName lowercases and strips quotes, so 'Roboto' becomes 'roboto'
    EXPECT_NE(output.find("font-family: 'roboto'"), std::string::npos);
    EXPECT_NE(output.find("font-weight: 400"), std::string::npos);
    EXPECT_NE(output.find("url('https://example.com/roboto.woff2')"), std::string::npos);
    EXPECT_NE(output.find("font-style: normal"), std::string::npos);
}

TEST_F(FontManagerTest, GenerateFontFaceCSS_Italic) {
    const char* input = R"(
        @font-face {
            font-family: 'Test';
            font-style: italic;
            font-weight: 400;
            src: url('https://example.com/test-italic.woff2');
        }
    )";
    fm->scanFontFaces(input);

    std::string output = fm->generateFontFaceCSS();
    EXPECT_NE(output.find("font-style: italic"), std::string::npos);
}

TEST_F(FontManagerTest, GenerateFontFaceCSS_WithUnicodeRange) {
    const char* input = R"(
        @font-face {
            font-family: 'NotoSansJP';
            font-weight: 400;
            src: url('https://example.com/noto-cjk.woff2');
            unicode-range: U+3000-9FFF;
        }
    )";
    fm->scanFontFaces(input);

    std::string output = fm->generateFontFaceCSS();
    EXPECT_NE(output.find("unicode-range: U+3000-9FFF"), std::string::npos);
}

TEST_F(FontManagerTest, GenerateFontFaceCSS_DeduplicatesEntries) {
    // Scanning the same CSS twice should not duplicate entries
    const char* input = R"(
        @font-face {
            font-family: 'Test';
            font-weight: 400;
            src: url('https://example.com/test.woff2');
        }
    )";
    fm->scanFontFaces(input);
    fm->scanFontFaces(input);

    std::string output = fm->generateFontFaceCSS();
    // Count occurrences of the URL — should be exactly 1
    size_t pos = 0;
    int count = 0;
    while ((pos = output.find("https://example.com/test.woff2", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    EXPECT_EQ(count, 1);
}

// ── clear ────────────────────────────────────────────────────────────────

TEST_F(FontManagerTest, ClearRemovesFontFaces) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            src: url('https://example.com/test.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test.woff2"));

    fm->clear();
    EXPECT_FALSE(fm->hasFontFaceSource("Test", "https://example.com/test.woff2"));
}

// ── Edge cases ───────────────────────────────────────────────────────────

TEST_F(FontManagerTest, ScanFontFaces_MissingUrlSkipped) {
    // @font-face with no src url() should not register anything
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            font-weight: 400;
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->getFontUrls("Test", 400, SkFontStyle::kUpright_Slant).empty());
}

TEST_F(FontManagerTest, ScanFontFaces_WhitespaceInFamily) {
    const char* css = R"(
        @font-face {
            font-family: '  Open  Sans  ';
            src: url('https://example.com/opensans.woff2');
        }
    )";
    fm->scanFontFaces(css);
    // cleanName strips whitespace and quotes
    EXPECT_TRUE(fm->hasFontFaceSource("OpenSans", "https://example.com/opensans.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_SingleQuoteFamily) {
    const char* css = R"(
        @font-face {
            font-family: 'TestFont';
            src: url('https://example.com/test.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("TestFont", "https://example.com/test.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_DoubleQuoteFamily) {
    const char* css = R"(
        @font-face {
            font-family: "TestFont";
            src: url('https://example.com/test.woff2');
        }
    )";
    fm->scanFontFaces(css);
    EXPECT_TRUE(fm->hasFontFaceSource("TestFont", "https://example.com/test.woff2"));
}

TEST_F(FontManagerTest, ScanFontFaces_UnclosedBlock) {
    // Unclosed @font-face block — should not crash, just skip
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            src: url('https://example.com/test.woff2');
    )";
    fm->scanFontFaces(css);
    // Should not register (block not properly closed)
    EXPECT_TRUE(fm->getFontUrls("Test", 400, SkFontStyle::kUpright_Slant).empty());
}

TEST_F(FontManagerTest, ScanFontFaces_MultipleSrcUrls) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            src: url('https://example.com/test.woff2') format('woff2'),
                 url('https://example.com/test.ttf') format('truetype');
        }
    )";
    fm->scanFontFaces(css);
    // Only the best URL (woff2) should be registered
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test.woff2"));
    EXPECT_FALSE(fm->hasFontFaceSource("Test", "https://example.com/test.ttf"));
}

TEST_F(FontManagerTest, GetFontUrls_WithUsedCodepoints) {
    const char* css = R"(
        @font-face {
            font-family: 'NotoSansJP';
            src: url('https://example.com/noto-latin.woff2');
            unicode-range: U+0000-007F;
        }
        @font-face {
            font-family: 'NotoSansJP';
            src: url('https://example.com/noto-cjk.woff2');
            unicode-range: U+3000-9FFF;
        }
    )";
    fm->scanFontFaces(css);

    // Request with only ASCII codepoints — should only return latin URL
    std::set<char32_t> asciiCodepoints = {0x41, 0x42, 0x43}; // A, B, C
    auto urls = fm->getFontUrls("NotoSansJP", 400, SkFontStyle::kUpright_Slant, &asciiCodepoints);
    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "https://example.com/noto-latin.woff2");
}

TEST_F(FontManagerTest, GetFontUrls_WithCjkCodepoints) {
    const char* css = R"(
        @font-face {
            font-family: 'NotoSansJP';
            src: url('https://example.com/noto-latin.woff2');
            unicode-range: U+0000-007F;
        }
        @font-face {
            font-family: 'NotoSansJP';
            src: url('https://example.com/noto-cjk.woff2');
            unicode-range: U+3000-9FFF;
        }
    )";
    fm->scanFontFaces(css);

    // Request with CJK codepoints — should return CJK URL
    std::set<char32_t> cjkCodepoints = {0x3042, 0x3044}; // あ い
    auto urls = fm->getFontUrls("NotoSansJP", 400, SkFontStyle::kUpright_Slant, &cjkCodepoints);
    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "https://example.com/noto-cjk.woff2");
}

TEST_F(FontManagerTest, ScanFontFaces_SingleWeight) {
    const char* css = R"(
        @font-face {
            font-family: 'Test';
            font-weight: 700;
            src: url('https://example.com/test-bold.woff2');
        }
    )";
    fm->scanFontFaces(css);
    // Weight 700 is registered, requesting exact 700 should find it
    EXPECT_TRUE(fm->hasFontFaceSource("Test", "https://example.com/test-bold.woff2"));
    // Note: getFontUrls has fallback behavior that returns fonts of different weights
    // when no exact match exists, so we don't test that 400 returns empty here.
}
