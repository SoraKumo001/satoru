#include <gtest/gtest.h>
#include "litehtml/html_microsyntaxes.h"

using namespace litehtml;

// ---- html_parse_integer ----

TEST(HtmlMicrosyntaxesTest, ParseIntegerBasic) {
    int val = 0;
    EXPECT_TRUE(html_parse_integer("42", val));
    EXPECT_EQ(val, 42);
}

TEST(HtmlMicrosyntaxesTest, ParseIntegerNegative) {
    int val = 0;
    EXPECT_TRUE(html_parse_integer("-7", val));
    EXPECT_EQ(val, -7);
}

TEST(HtmlMicrosyntaxesTest, ParseIntegerZero) {
    int val = 0;
    EXPECT_TRUE(html_parse_integer("0", val));
    EXPECT_EQ(val, 0);
}

TEST(HtmlMicrosyntaxesTest, ParseIntegerLeadingWhitespace) {
    int val = 0;
    // strtol skips leading whitespace
    EXPECT_TRUE(html_parse_integer("  10", val));
    EXPECT_EQ(val, 10);
}

TEST(HtmlMicrosyntaxesTest, ParseIntegerTrailingChars) {
    int val = 0;
    // strtol parses until non-digit
    EXPECT_TRUE(html_parse_integer("123px", val));
    EXPECT_EQ(val, 123);
}

TEST(HtmlMicrosyntaxesTest, ParseIntegerEmpty) {
    int val = 0;
    EXPECT_FALSE(html_parse_integer("", val));
}

TEST(HtmlMicrosyntaxesTest, ParseIntegerNonDigit) {
    int val = 0;
    EXPECT_FALSE(html_parse_integer("abc", val));
}

// ---- html_parse_non_negative_integer ----

TEST(HtmlMicrosyntaxesTest, ParseNonNegativeInteger) {
    int val = 0;
    EXPECT_TRUE(html_parse_non_negative_integer("100", val));
    EXPECT_EQ(val, 100);
}

TEST(HtmlMicrosyntaxesTest, ParseNonNegativeIntegerZero) {
    int val = 0;
    EXPECT_TRUE(html_parse_non_negative_integer("0", val));
    EXPECT_EQ(val, 0);
}

TEST(HtmlMicrosyntaxesTest, ParseNonNegativeIntegerNegative) {
    int val = 0;
    EXPECT_FALSE(html_parse_non_negative_integer("-5", val));
}

// ---- html_parse_dimension_value ----

TEST(HtmlMicrosyntaxesTest, ParseDimensionLength) {
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_dimension_value("100", val, type));
    EXPECT_FLOAT_EQ(val, 100.0f);
    EXPECT_EQ(type, html_length);
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionPercentage) {
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_dimension_value("50%", val, type));
    EXPECT_FLOAT_EQ(val, 50.0f);
    EXPECT_EQ(type, html_percentage);
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionDecimal) {
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_dimension_value("3.14", val, type));
    EXPECT_FLOAT_EQ(val, 3.14f);
    EXPECT_EQ(type, html_length);
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionDecimalPercentage) {
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_dimension_value("66.6%", val, type));
    EXPECT_FLOAT_EQ(val, 66.6f);
    EXPECT_EQ(type, html_percentage);
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionEmpty) {
    float val = 0;
    html_dimension_type type;
    EXPECT_FALSE(html_parse_dimension_value("", val, type));
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionNonDigit) {
    float val = 0;
    html_dimension_type type;
    EXPECT_FALSE(html_parse_dimension_value("abc", val, type));
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionWithLeadingWhitespace) {
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_dimension_value("  10px", val, type));
    EXPECT_FLOAT_EQ(val, 10.0f);
    EXPECT_EQ(type, html_length);
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionDecimalDotTrailing) {
    // "10." → value=10, type=length (no digit after dot)
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_dimension_value("10.", val, type));
    EXPECT_FLOAT_EQ(val, 10.0f);
    EXPECT_EQ(type, html_length);
}

TEST(HtmlMicrosyntaxesTest, ParseDimensionDecimalDotPercent) {
    // "10.%" → value=10, type=percentage
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_dimension_value("10.%", val, type));
    EXPECT_FLOAT_EQ(val, 10.0f);
    EXPECT_EQ(type, html_percentage);
}

// ---- html_parse_nonzero_dimension_value ----

TEST(HtmlMicrosyntaxesTest, ParseNonZeroDimensionLength) {
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_nonzero_dimension_value("10px", val, type));
    EXPECT_FLOAT_EQ(val, 10.0f);
    EXPECT_EQ(type, html_length);
}

TEST(HtmlMicrosyntaxesTest, ParseNonZeroDimensionPercentage) {
    float val = 0;
    html_dimension_type type;
    EXPECT_TRUE(html_parse_nonzero_dimension_value("50%", val, type));
    EXPECT_FLOAT_EQ(val, 50.0f);
    EXPECT_EQ(type, html_percentage);
}

TEST(HtmlMicrosyntaxesTest, ParseNonZeroDimensionZero) {
    float val = 0;
    html_dimension_type type;
    EXPECT_FALSE(html_parse_nonzero_dimension_value("0", val, type));
}

TEST(HtmlMicrosyntaxesTest, ParseNonZeroDimensionZeroPercent) {
    float val = 0;
    html_dimension_type type;
    EXPECT_FALSE(html_parse_nonzero_dimension_value("0%", val, type));
}
