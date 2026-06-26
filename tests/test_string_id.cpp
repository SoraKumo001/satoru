#include <gtest/gtest.h>
#include "litehtml/string_id.h"

using namespace litehtml;

// ---- _id() and _s() basics ----

TEST(StringIdTest, EmptyId) {
    // empty_id should resolve to ""
    EXPECT_EQ(_s(empty_id), "");
}

TEST(StringIdTest, StarId) {
    // star_id should resolve to "*"
    EXPECT_EQ(_s(star_id), "*");
}

TEST(StringIdTest, KnownHtmlTags) {
    // Check that standard HTML tag names are registered
    EXPECT_EQ(_s(_div_), "div");
    EXPECT_EQ(_s(_span_), "span");
    EXPECT_EQ(_s(_p_), "p");
    EXPECT_EQ(_s(_a_), "a");
    EXPECT_EQ(_s(_img_), "img");
    EXPECT_EQ(_s(_h1_), "h1");
    EXPECT_EQ(_s(_html_), "html");
    EXPECT_EQ(_s(_body_), "body");
}

TEST(StringIdTest, KnownCssProperties) {
    // CSS property names use hyphens (underscore → hyphen)
    EXPECT_EQ(_s(_background_color_), "background-color");
    EXPECT_EQ(_s(_font_size_), "font-size");
    EXPECT_EQ(_s(_text_align_), "text-align");
    EXPECT_EQ(_s(_border_top_width_), "border-top-width");
    EXPECT_EQ(_s(_margin_left_), "margin-left");
}

TEST(StringIdTest, KnownCssValues) {
    EXPECT_EQ(_s(_auto_), "auto");
    EXPECT_EQ(_s(_none_), "none");
    EXPECT_EQ(_s(_initial_), "initial");
    EXPECT_EQ(_s(_initial_), "initial");
}

TEST(StringIdTest, PseudoElements) {
    EXPECT_EQ(_s(_before_), "before");
    EXPECT_EQ(_s(_after_), "after");
}

TEST(StringIdTest, PseudoClasses) {
    EXPECT_EQ(_s(_root_), "root");
    EXPECT_EQ(_s(_first_child_), "first-child");
    EXPECT_EQ(_s(_nth_child_), "nth-child");
    EXPECT_EQ(_s(_hover_), "hover");
    EXPECT_EQ(_s(_active_), "active");
}

// ---- _id() dynamic registration ----

TEST(StringIdTest, DynamicRegistration) {
    // Register a custom string not in the static table
    string_id custom = _id("my-custom-value");
    EXPECT_EQ(_s(custom), "my-custom-value");
}

TEST(StringIdTest, IdempotentLookup) {
    // Calling _id() twice with the same string returns the same id
    string_id first = _id("some-string");
    string_id second = _id("some-string");
    EXPECT_EQ(first, second);
}

TEST(StringIdTest, DifferentStringsDifferentIds) {
    string_id a = _id("aaa");
    string_id b = _id("bbb");
    EXPECT_NE(a, b);
}

TEST(StringIdTest, EmptyStringDynamic) {
    string_id custom_empty = _id("");
    // Should resolve to ""
    EXPECT_EQ(_s(custom_empty), "");
}

// ---- enum values are sequential ----

TEST(StringIdTest, EnumSequence) {
    // _a_ and _abbr_ should be adjacent
    EXPECT_EQ(_a_ + 1, _abbr_);
    // _abbr_ and _acronym_ should be adjacent
    EXPECT_EQ(_abbr_ + 1, _acronym_);
}
