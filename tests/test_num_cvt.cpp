#include <gtest/gtest.h>
#include "litehtml/num_cvt.h"

using namespace litehtml;

// ---- to_roman_lower ----

TEST(NumCvtTest, ToRomanLower) {
    EXPECT_EQ(num_cvt::to_roman_lower(1), "i");
    EXPECT_EQ(num_cvt::to_roman_lower(5), "v");
    EXPECT_EQ(num_cvt::to_roman_lower(10), "x");
    EXPECT_EQ(num_cvt::to_roman_lower(50), "l");
    EXPECT_EQ(num_cvt::to_roman_lower(100), "c");
    EXPECT_EQ(num_cvt::to_roman_lower(500), "d");
    EXPECT_EQ(num_cvt::to_roman_lower(1000), "m");
}

TEST(NumCvtTest, ToRomanLowerComplex) {
    EXPECT_EQ(num_cvt::to_roman_lower(4), "iv");
    EXPECT_EQ(num_cvt::to_roman_lower(9), "ix");
    EXPECT_EQ(num_cvt::to_roman_lower(14), "xiv");
    EXPECT_EQ(num_cvt::to_roman_lower(1994), "mcmxciv");
}

// ---- to_roman_upper ----

TEST(NumCvtTest, ToRomanUpper) {
    EXPECT_EQ(num_cvt::to_roman_upper(1), "I");
    EXPECT_EQ(num_cvt::to_roman_upper(5), "V");
    EXPECT_EQ(num_cvt::to_roman_upper(10), "X");
    EXPECT_EQ(num_cvt::to_roman_upper(1994), "MCMXCIV");
}

// ---- to_latin_lower ----

TEST(NumCvtTest, ToLatinLower) {
    EXPECT_EQ(num_cvt::to_latin_lower(1), "a");
    EXPECT_EQ(num_cvt::to_latin_lower(2), "b");
    EXPECT_EQ(num_cvt::to_latin_lower(26), "z");
}

// ---- to_latin_upper ----

TEST(NumCvtTest, ToLatinUpper) {
    EXPECT_EQ(num_cvt::to_latin_upper(1), "A");
    EXPECT_EQ(num_cvt::to_latin_upper(2), "B");
    EXPECT_EQ(num_cvt::to_latin_upper(26), "Z");
}

// ---- to_greek_lower ----

TEST(NumCvtTest, ToGreekLower) {
    EXPECT_EQ(num_cvt::to_greek_lower(1), "\xce\xb1");  // alpha
    EXPECT_EQ(num_cvt::to_greek_lower(5), "\xce\xb5");  // epsilon
}
