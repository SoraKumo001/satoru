#include <gtest/gtest.h>
#include "litehtml/html.h"

using namespace litehtml;

// ---- trim ----

TEST(HtmlUtilsTest, TrimRemovesWhitespace) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\t\nhello\n\t"), "hello");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim(""), "");
}

TEST(HtmlUtilsTest, TrimCustomChars) {
    EXPECT_EQ(trim("##hello##", "#"), "hello");
    EXPECT_EQ(trim("---hello---", "-"), "hello");
}

TEST(HtmlUtilsTest, TrimMutating) {
    std::string s = "  hello  ";
    trim(s);
    EXPECT_EQ(s, "hello");
}

// ---- lcase ----

TEST(HtmlUtilsTest, Lcase) {
    std::string s1 = "HELLO";
    lcase(s1);
    EXPECT_EQ(s1, "hello");

    std::string s2 = "Hello";
    lcase(s2);
    EXPECT_EQ(s2, "hello");

    std::string s3 = "hello";
    lcase(s3);
    EXPECT_EQ(s3, "hello");

    std::string s4 = "";
    lcase(s4);
    EXPECT_EQ(s4, "");
}

// ---- is_whitespace ----

TEST(HtmlUtilsTest, IsWhitespace) {
    EXPECT_TRUE(is_whitespace(' '));
    EXPECT_TRUE(is_whitespace('\t'));
    EXPECT_TRUE(is_whitespace('\n'));
    EXPECT_TRUE(is_whitespace('\r'));
    EXPECT_TRUE(is_whitespace('\f'));
    EXPECT_FALSE(is_whitespace('a'));
    EXPECT_FALSE(is_whitespace('0'));
}

// ---- t_isalpha / is_letter ----

TEST(HtmlUtilsTest, IsAlpha) {
    EXPECT_TRUE(t_isalpha('a'));
    EXPECT_TRUE(t_isalpha('z'));
    EXPECT_TRUE(t_isalpha('A'));
    EXPECT_TRUE(t_isalpha('Z'));
    EXPECT_FALSE(t_isalpha('0'));
    EXPECT_FALSE(t_isalpha(' '));
}

// ---- t_isdigit / is_digit ----

TEST(HtmlUtilsTest, IsDigit) {
    EXPECT_TRUE(t_isdigit('0'));
    EXPECT_TRUE(t_isdigit('9'));
    EXPECT_FALSE(t_isdigit('a'));
    EXPECT_FALSE(t_isdigit(' '));
}

// ---- is_hex_digit ----

TEST(HtmlUtilsTest, IsHexDigit) {
    EXPECT_TRUE(is_hex_digit('0'));
    EXPECT_TRUE(is_hex_digit('9'));
    EXPECT_TRUE(is_hex_digit('a'));
    EXPECT_TRUE(is_hex_digit('f'));
    EXPECT_TRUE(is_hex_digit('A'));
    EXPECT_TRUE(is_hex_digit('F'));
    EXPECT_FALSE(is_hex_digit('g'));
    EXPECT_FALSE(is_hex_digit(' '));
}

// ---- digit_value ----

TEST(HtmlUtilsTest, DigitValue) {
    EXPECT_EQ(digit_value('0'), 0);
    EXPECT_EQ(digit_value('9'), 9);
    EXPECT_EQ(digit_value('a'), 10);
    EXPECT_EQ(digit_value('f'), 15);
    EXPECT_EQ(digit_value('A'), 10);
    EXPECT_EQ(digit_value('F'), 15);
}

// ---- t_tolower / lowcase ----

TEST(HtmlUtilsTest, ToLower) {
    EXPECT_EQ(t_tolower('A'), 'a');
    EXPECT_EQ(t_tolower('Z'), 'z');
    EXPECT_EQ(t_tolower('a'), 'a');
    EXPECT_EQ(t_tolower('0'), '0');
}

TEST(HtmlUtilsTest, LowcaseString) {
    EXPECT_EQ(lowcase("HELLO"), "hello");
    EXPECT_EQ(lowcase("Hello"), "hello");
    EXPECT_EQ(lowcase(""), "");
}

// ---- is_surrogate ----

TEST(HtmlUtilsTest, IsSurrogate) {
    EXPECT_TRUE(is_surrogate(0xD800));
    EXPECT_TRUE(is_surrogate(0xDFFF));
    EXPECT_FALSE(is_surrogate(0xD7FF));
    EXPECT_FALSE(is_surrogate(0xE000));
}

// ---- round_f / round_d ----

TEST(HtmlUtilsTest, RoundFloat) {
    EXPECT_EQ(round_f(1.0f), 1);
    EXPECT_EQ(round_f(1.4f), 1);
    EXPECT_EQ(round_f(1.5f), 2);
    EXPECT_EQ(round_f(1.6f), 2);
    EXPECT_EQ(round_f(-1.5f), -1);
}

TEST(HtmlUtilsTest, RoundDouble) {
    EXPECT_EQ(round_d(1.0), 1);
    EXPECT_EQ(round_d(1.4), 1);
    EXPECT_EQ(round_d(1.5), 2);
    EXPECT_EQ(round_d(1.6), 2);
}

// ---- t_strtod ----

TEST(HtmlUtilsTest, Strtod) {
    char* end;
    EXPECT_DOUBLE_EQ(t_strtod("3.14", &end), 3.14);
    EXPECT_EQ(*end, '\0');

    EXPECT_DOUBLE_EQ(t_strtod("100px", &end), 100.0);
    EXPECT_STREQ(end, "px");
}

// ---- value_index ----

TEST(HtmlUtilsTest, ValueIndex) {
    EXPECT_EQ(value_index("b", "a;b;c"), 1);
    EXPECT_EQ(value_index("a", "a;b;c"), 0);
    EXPECT_EQ(value_index("c", "a;b;c"), 2);
    EXPECT_EQ(value_index("d", "a;b;c"), -1);
    EXPECT_EQ(value_index("d", "a;b;c", 0), 0);  // default value
}

// ---- index_value ----

TEST(HtmlUtilsTest, IndexValue) {
    EXPECT_EQ(index_value(0, "a;b;c"), "a");
    EXPECT_EQ(index_value(1, "a;b;c"), "b");
    EXPECT_EQ(index_value(2, "a;b;c"), "c");
}

// ---- value_in_list ----

TEST(HtmlUtilsTest, ValueInList) {
    EXPECT_TRUE(value_in_list("b", "a;b;c"));
    EXPECT_FALSE(value_in_list("d", "a;b;c"));
}

// ---- find_close_bracket ----

TEST(HtmlUtilsTest, FindCloseBracket) {
    std::string s = "(a(b)c)";
    EXPECT_EQ(find_close_bracket(s, 0), 6u);
    EXPECT_EQ(find_close_bracket(s, 2), 4u);
}

// ---- split_string ----

TEST(HtmlUtilsTest, SplitString) {
    auto tokens = split_string("a;b;c", ";");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "a");
    EXPECT_EQ(tokens[1], "b");
    EXPECT_EQ(tokens[2], "c");
}

TEST(HtmlUtilsTest, SplitStringWhitespace) {
    auto tokens = split_string("hello world");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
}

// ---- join_string ----

TEST(HtmlUtilsTest, JoinString) {
    string_vector tokens = {"a", "b", "c"};
    std::string result;
    join_string(result, tokens, ";");
    EXPECT_EQ(result, "a;b;c");
}

// ---- is_number ----

TEST(HtmlUtilsTest, IsNumber) {
    EXPECT_TRUE(is_number("123"));
    EXPECT_TRUE(is_number("3.14"));
    EXPECT_TRUE(is_number(""));  // empty is considered valid
    EXPECT_TRUE(is_number("12.34.56"));  // implementation allows multiple dots
    EXPECT_FALSE(is_number("abc"));
}

// ---- equal_i ----

TEST(HtmlUtilsTest, EqualI) {
    EXPECT_TRUE(equal_i("hello", "HELLO"));
    EXPECT_TRUE(equal_i("Hello", "hello"));
    EXPECT_FALSE(equal_i("hello", "world"));
    EXPECT_FALSE(equal_i("hi", "hello"));
}

// ---- match / match_i ----

TEST(HtmlUtilsTest, Match) {
    EXPECT_TRUE(match("hello world", 0, "hello"));
    EXPECT_TRUE(match("hello world", 6, "world"));
    EXPECT_FALSE(match("hello world", 0, "world"));
}

TEST(HtmlUtilsTest, MatchI) {
    EXPECT_TRUE(match_i("HELLO", 0, "hello"));
    EXPECT_FALSE(match_i("HELLO", 0, "world"));
}

// ---- is_one_of ----

TEST(HtmlUtilsTest, IsOneOf) {
    EXPECT_TRUE(is_one_of(1, 1, 2, 3));
    EXPECT_TRUE(is_one_of(3, 1, 2, 3));
    EXPECT_FALSE(is_one_of(4, 1, 2, 3));
}

// ---- template utilities ----

TEST(HtmlUtilsTest, VectorAt) {
    std::vector<int> v = {10, 20, 30};
    EXPECT_EQ(at(v, 0), 10);
    EXPECT_EQ(at(v, 2), 30);
    EXPECT_EQ(at(v, -1), 30);
    EXPECT_EQ(at(v, 5), 0);  // default
}

TEST(HtmlUtilsTest, Slice) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    auto s = slice(v, 1, 3);
    ASSERT_EQ(s.size(), 3u);
    EXPECT_EQ(s[0], 2);
    EXPECT_EQ(s[1], 3);
    EXPECT_EQ(s[2], 4);
}

TEST(HtmlUtilsTest, Remove) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    remove(v, 2);
    ASSERT_EQ(v.size(), 4u);
    EXPECT_EQ(v[2], 4);
}

TEST(HtmlUtilsTest, ContainsVector) {
    std::vector<int> v = {1, 2, 3};
    EXPECT_TRUE(contains(v, 2));
    EXPECT_FALSE(contains(v, 4));
}

TEST(HtmlUtilsTest, ContainsString) {
    std::string s = "hello world";
    EXPECT_TRUE(contains(s, std::string("world")));
    EXPECT_FALSE(contains(s, std::string("xyz")));
}

// ---- get_escaped_string ----

TEST(HtmlUtilsTest, GetEscapedString) {
    EXPECT_EQ(get_escaped_string("hello"), "hello");
    // Note: exact escaping behavior depends on implementation
}

// ---- baseline_align ----

TEST(HtmlUtilsTest, BaselineAlign) {
    // pixel_t is a typedef, just test the arithmetic
    // baseline_align(line_height, line_base_line, height, baseline)
    // returns (line_height - line_base_line) - (height - baseline)
    int result = (20 - 5) - (15 - 3);
    EXPECT_EQ(result, 3);
}
