#include <gtest/gtest.h>
#include "litehtml/codepoint.h"

using namespace litehtml;

// ---- is_ascii_codepoint ----

TEST(CodepointTest, IsAscii) {
    EXPECT_TRUE(is_ascii_codepoint('a'));
    EXPECT_TRUE(is_ascii_codepoint('z'));
    EXPECT_TRUE(is_ascii_codepoint('A'));
    EXPECT_TRUE(is_ascii_codepoint('Z'));
    EXPECT_TRUE(is_ascii_codepoint('0'));
    EXPECT_TRUE(is_ascii_codepoint('9'));
    EXPECT_TRUE(is_ascii_codepoint(' '));
    EXPECT_TRUE(is_ascii_codepoint('@'));
    EXPECT_FALSE(is_ascii_codepoint(static_cast<char>(128)));
    EXPECT_FALSE(is_ascii_codepoint(static_cast<char>(255)));
}

// ---- is_url_reserved_codepoint ----

TEST(CodepointTest, IsUrlReserved) {
    // Reserved: : / ? # [ ] @ ! $ & ' ( ) * + , ; =
    EXPECT_TRUE(is_url_reserved_codepoint(':'));
    EXPECT_TRUE(is_url_reserved_codepoint('/'));
    EXPECT_TRUE(is_url_reserved_codepoint('?'));
    EXPECT_TRUE(is_url_reserved_codepoint('#'));
    EXPECT_TRUE(is_url_reserved_codepoint('['));
    EXPECT_TRUE(is_url_reserved_codepoint(']'));
    EXPECT_TRUE(is_url_reserved_codepoint('@'));
    EXPECT_TRUE(is_url_reserved_codepoint('!'));
    EXPECT_TRUE(is_url_reserved_codepoint('$'));
    EXPECT_TRUE(is_url_reserved_codepoint('&'));
    EXPECT_TRUE(is_url_reserved_codepoint('\''));
    EXPECT_TRUE(is_url_reserved_codepoint('('));
    EXPECT_TRUE(is_url_reserved_codepoint(')'));
    EXPECT_TRUE(is_url_reserved_codepoint('*'));
    EXPECT_TRUE(is_url_reserved_codepoint('+'));
    EXPECT_TRUE(is_url_reserved_codepoint(','));
    EXPECT_TRUE(is_url_reserved_codepoint(';'));
    EXPECT_TRUE(is_url_reserved_codepoint('='));
    // Not reserved
    EXPECT_FALSE(is_url_reserved_codepoint('a'));
    EXPECT_FALSE(is_url_reserved_codepoint('0'));
    EXPECT_FALSE(is_url_reserved_codepoint('-'));
    EXPECT_FALSE(is_url_reserved_codepoint('.'));
    EXPECT_FALSE(is_url_reserved_codepoint('_'));
    EXPECT_FALSE(is_url_reserved_codepoint('~'));
}

// ---- is_url_scheme_codepoint ----

TEST(CodepointTest, IsUrlScheme) {
    EXPECT_TRUE(is_url_scheme_codepoint('a'));
    EXPECT_TRUE(is_url_scheme_codepoint('z'));
    EXPECT_TRUE(is_url_scheme_codepoint('A'));
    EXPECT_TRUE(is_url_scheme_codepoint('Z'));
    EXPECT_TRUE(is_url_scheme_codepoint('0'));
    EXPECT_TRUE(is_url_scheme_codepoint('9'));
    EXPECT_TRUE(is_url_scheme_codepoint('+'));
    EXPECT_TRUE(is_url_scheme_codepoint('-'));
    EXPECT_TRUE(is_url_scheme_codepoint('.'));
    // Not scheme
    EXPECT_FALSE(is_url_scheme_codepoint(' '));
    EXPECT_FALSE(is_url_scheme_codepoint('/'));
    EXPECT_FALSE(is_url_scheme_codepoint(':'));
    EXPECT_FALSE(is_url_scheme_codepoint('@'));
}
