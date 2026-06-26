#include <gtest/gtest.h>
#include "litehtml/web_color.h"

using namespace litehtml;

// ---- web_color constructors ----

TEST(WebColorTest, DefaultConstructor) {
    web_color c;
    EXPECT_EQ(c.red, 0);
    EXPECT_EQ(c.green, 0);
    EXPECT_EQ(c.blue, 0);
    EXPECT_EQ(c.alpha, 255);
    EXPECT_FALSE(c.is_current_color);
}

TEST(WebColorTest, ParameterizedConstructor) {
    web_color c(255, 128, 0);
    EXPECT_EQ(c.red, 255);
    EXPECT_EQ(c.green, 128);
    EXPECT_EQ(c.blue, 0);
    EXPECT_EQ(c.alpha, 255);
}

TEST(WebColorTest, WithAlpha) {
    web_color c(255, 128, 0, 128);
    EXPECT_EQ(c.alpha, 128);
}

TEST(WebColorTest, CurrentColorConstructor) {
    web_color c(true);
    EXPECT_TRUE(c.is_current_color);
}

// ---- web_color comparison ----

TEST(WebColorTest, Equality) {
    web_color a(100, 200, 50);
    web_color b(100, 200, 50);
    web_color c(100, 200, 51);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ---- web_color::darken ----

TEST(WebColorTest, Darken) {
    web_color c(200, 200, 200);
    web_color dark = c.darken(0.5);
    EXPECT_LT(dark.red, c.red);
    EXPECT_LT(dark.green, c.green);
    EXPECT_LT(dark.blue, c.blue);
}

TEST(WebColorTest, DarkenZero) {
    web_color c(200, 200, 200);
    web_color dark = c.darken(0.0);
    EXPECT_EQ(dark.red, c.red);
    EXPECT_EQ(dark.green, c.green);
    EXPECT_EQ(dark.blue, c.blue);
}

// ---- static constants ----

TEST(WebColorTest, Constants) {
    EXPECT_EQ(web_color::transparent.alpha, 0);
    EXPECT_EQ(web_color::black.red, 0);
    EXPECT_EQ(web_color::black.green, 0);
    EXPECT_EQ(web_color::black.blue, 0);
    EXPECT_EQ(web_color::white.red, 255);
    EXPECT_EQ(web_color::white.green, 255);
    EXPECT_EQ(web_color::white.blue, 255);
}
