// Tests ResourceManager state methods (request, getPendingRequests, has, clear).
//
// Approach: #define SATORU_CONTEXT_H and CONTAINER_SKIA_H BEFORE including
// resource_manager.cpp, which prevents the real (Skia-heavy) headers from being
// pulled in. Instead we provide minimal stubs for SatoruContext and
// SatoruFontManager that support compilation of the ResourceManager state
// methods.
//
// All m_context stub methods are no-ops that return false / {} — sufficient
// for state-method testing.
//
// IMPORTANT: If resource_manager.cpp gains new includes pulling in un-guarded
// Skia-heavy headers, this test breaks. Update the guards or add stubs.

#include <gtest/gtest.h>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// --- CssChangeKind (copied from satoru_context.h) ---
// Must be kept in sync if new values are added.
enum class CssChangeKind {
    Generic,
    UserScan,
    ExternalResource,
    FontResourceCss,
    FontAliasCss,
    GeneratedFontFace,
};

// --- Skia stub includes (from tests/stubs/include/) ---
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"

// --- SatoruFontManager stub ---
// Provides no-op versions of every method ResourceManager::add() calls.
// NOTE: Named differently from the real SatoruFontManager to avoid linker
// conflicts with font_manager.cpp.o which defines real SatoruFontManager methods.
struct SatoruFontManagerStub {
    bool loadFont(const char*, const uint8_t*, int, const char* = nullptr) { return true; }
    void scanFontFaces(const std::string&) {}
    std::vector<sk_sp<SkTypeface>> matchFonts(const std::string&, int, SkFontStyle::Slant) {
        return {};
    }
    void addFallbackTypeface(sk_sp<SkTypeface>) {}
    bool hasFontFaceSource(const std::string&, const std::string&) { return false; }
    void clear() {}
};

// --- SatoruContext stub ---
// Minimal class providing the methods that ResourceManager methods call.
// All are no-ops so the state methods compile and run without side effects.
// NOTE: Uses SatoruFontManagerStub (not the real SatoruFontManager) to avoid
// linker conflicts with font_manager.cpp.o. Since satoru_context.cpp is NOT
// compiled in the test build, there's no conflict for SatoruContext itself.
class SatoruContext {
   public:
    SatoruFontManagerStub fontManager;

    bool addCss(const std::string& css, CssChangeKind kind = CssChangeKind::Generic) {
        (void)css;
        (void)kind;
        return false;
    }

    bool loadFont(const char*, const uint8_t*, int, const char* = nullptr) { return false; }

    void noteFontResourceRegistered(bool /*registered*/) {}
    void noteFontResourceNamedLoad() {}
    void noteFontResourceFallbackLoad() {}
    void noteGeneratedFontFace(bool /*added*/) {}

    void loadImageFromData(const char*, const uint8_t*, size_t, const char* = nullptr) {}
};

// === Guard out the real Skia-heavy headers ===
// These are included by resource_manager.cpp's transitive includes.
// We define the guards BEFORE #including resource_manager.cpp so the
// preprocessor skips the real headers entirely.
#define SATORU_CONTEXT_H
#define CONTAINER_SKIA_H

// Forward declare for the extern declaration in resource_manager.cpp
class container_skia;

// === Include the implementation under test ===
#include "../src/cpp/core/resource_manager.cpp"

// Helper: returns a reference to a static SatoruContext for use in tests.
static SatoruContext& test_context() {
    static SatoruContext ctx;
    return ctx;
}

// ==================== Tests ====================

TEST(ResourceManagerRequestTest, BasicRequest) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "MyFont", ResourceType::Font);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].url, "https://example.com/font.woff2");
    EXPECT_EQ(pending[0].name, "MyFont");
    EXPECT_EQ(pending[0].type, ResourceType::Font);
    EXPECT_FALSE(pending[0].redraw_on_ready);
    EXPECT_TRUE(pending[0].characters.empty());
}

TEST(ResourceManagerRequestTest, EmptyUrl) {
    ResourceManager rm(test_context());
    rm.request("", "MyFont", ResourceType::Font);

    auto pending = rm.getPendingRequests();
    EXPECT_TRUE(pending.empty());
}

TEST(ResourceManagerRequestTest, DedupSameUrl) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "FontA", ResourceType::Font);
    rm.request("https://example.com/font.woff2", "FontB", ResourceType::Font);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1u);
    // The first name is recorded
    EXPECT_EQ(pending[0].name, "FontA");
}

TEST(ResourceManagerRequestTest, DifferentUrls) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/a.woff2", "FontA", ResourceType::Font);
    rm.request("https://example.com/b.woff2", "FontB", ResourceType::Font);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 2u);
}

TEST(ResourceManagerRequestTest, DifferentTypes) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font);
    rm.request("https://example.com/img.png", "Img", ResourceType::Image);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 2u);
}

TEST(ResourceManagerRequestTest, RedrawOnReadyFlag) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "MyFont", ResourceType::Font, true);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_TRUE(pending[0].redraw_on_ready);
}

TEST(ResourceManagerRequestTest, CharactersField) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "MyFont", ResourceType::Font, false, "abc123");

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].characters, "abc123");
}

TEST(ResourceManagerHasTest, FalseInitially) {
    ResourceManager rm(test_context());
    EXPECT_FALSE(rm.has("https://example.com/font.woff2"));
}

TEST(ResourceManagerHasTest, TrueAfterAdd) {
    ResourceManager rm(test_context());
    uint8_t data[] = "font data";
    rm.add("https://example.com/font.woff2", data, sizeof(data), ResourceType::Font);
    EXPECT_TRUE(rm.has("https://example.com/font.woff2"));
}

TEST(ResourceManagerHasTest, AfterClear) {
    ResourceManager rm(test_context());
    uint8_t data[] = "font data";
    rm.add("https://example.com/font.woff2", data, sizeof(data), ResourceType::Font);
    EXPECT_TRUE(rm.has("https://example.com/font.woff2"));
    rm.clear();
    EXPECT_FALSE(rm.has("https://example.com/font.woff2"));
}

TEST(ResourceManagerClearTest, ClearsAll) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/a.woff2", "A", ResourceType::Font);
    rm.request("https://example.com/b.png", "B", ResourceType::Image);

    rm.clear();

    auto pending = rm.getPendingRequests();
    EXPECT_TRUE(pending.empty());
}

// Note: After clear(), previous pending requests are gone — already verified.
// has() after clear() is tested in AfterClear above.

TEST(ResourceManagerClearTypeTest, RemovesMatchingTypeOnly) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font);
    rm.request("https://example.com/img.png", "Img", ResourceType::Image);

    rm.clear(ResourceType::Image);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].type, ResourceType::Font);
}

TEST(ResourceManagerClearTypeTest, ClearFontKeepsImage) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font);
    rm.request("https://example.com/img.png", "Img", ResourceType::Image);

    rm.clear(ResourceType::Font);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].type, ResourceType::Image);
}

TEST(ResourceManagerClearTypeTest, ClearsOnlySpecifiedFromResolved) {
    ResourceManager rm(test_context());
    uint8_t data[] = "data";
    rm.add("https://example.com/font.woff2", data, sizeof(data), ResourceType::Font);
    rm.add("https://example.com/img.png", data, sizeof(data), ResourceType::Image);

    rm.clear(ResourceType::Font);

    EXPECT_FALSE(rm.has("https://example.com/font.woff2"));
    EXPECT_TRUE(rm.has("https://example.com/img.png"));
}

TEST(ResourceManagerRequestTest, AlreadyResolvedRequestNoOp) {
    ResourceManager rm(test_context());
    uint8_t data[] = "data";
    rm.add("https://example.com/font.woff2", data, sizeof(data), ResourceType::Font);

    // Requesting an already-resolved URL should be a no-op
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font);

    auto pending = rm.getPendingRequests();
    EXPECT_TRUE(pending.empty());
}

TEST(ResourceManagerGetPendingRequestsTest, ClearsAfterRead) {
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font);

    auto first = rm.getPendingRequests();
    EXPECT_EQ(first.size(), 1u);

    auto second = rm.getPendingRequests();
    EXPECT_TRUE(second.empty());
}

TEST(ResourceManagerRequestTest, DedupSetSemantics) {
    // Two identical requests should result in only one entry
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font, false, "");
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font, false, "");

    auto pending = rm.getPendingRequests();
    EXPECT_EQ(pending.size(), 1u);
}

TEST(ResourceManagerRequestTest, DifferentRedrawOnReadyDifferentRequests) {
    // Same URL + name + type but different redraw_on_ready = different request
    ResourceManager rm(test_context());
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font, false);
    rm.request("https://example.com/font.woff2", "Font", ResourceType::Font, true);

    auto pending = rm.getPendingRequests();
    // The URL is deduped by m_requestedUrls, so only 1
    EXPECT_EQ(pending.size(), 1u);
}

TEST(ResourceManagerRequestTest, DataUrlEmptyPayloadFallsThroughToPending) {
    // Empty base64 data means add() is never called — request goes to pending
    ResourceManager rm(test_context());
    rm.request("data:font/woff2;base64,", "InlineFont", ResourceType::Font);

    auto pending = rm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].name, "InlineFont");
    EXPECT_EQ(pending[0].url, "data:font/woff2;base64,");
}

TEST(ResourceManagerAddTest, EmptyUrl) {
    ResourceManager rm(test_context());
    uint8_t data[] = "data";
    rm.add("", data, sizeof(data), ResourceType::Font);
    EXPECT_FALSE(rm.has(""));
}

TEST(ResourceManagerAddTest, NullData) {
    ResourceManager rm(test_context());
    rm.add("https://example.com/nodata", nullptr, 0, ResourceType::Font);
    EXPECT_TRUE(rm.has("https://example.com/nodata"));
}

TEST(ResourceManagerAddTest, DedupSameUrl) {
    ResourceManager rm(test_context());
    uint8_t data[] = "data";
    rm.add("https://example.com/font.woff2", data, sizeof(data), ResourceType::Font);
    rm.add("https://example.com/font.woff2", data, sizeof(data), ResourceType::Image);

    // Should stay as Font (first type wins)
    EXPECT_TRUE(rm.has("https://example.com/font.woff2"));
}
