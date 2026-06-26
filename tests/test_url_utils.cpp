#include <gtest/gtest.h>
#include "litehtml/url.h"
#include "litehtml/url_path.h"

using namespace litehtml;

// ---- url::encode / url::decode ----

TEST(UrlTest, EncodeEmpty) {
    EXPECT_EQ(url::encode(""), "");
}

TEST(UrlTest, EncodeNoSpecial) {
    EXPECT_EQ(url::encode("hello"), "hello");
}

TEST(UrlTest, EncodeSpaces) {
    EXPECT_EQ(url::encode("hello world"), "hello%20world");
}

TEST(UrlTest, EncodeSpecialChars) {
    EXPECT_EQ(url::encode("a&b=c"), "a%26b%3Dc");
}

TEST(UrlTest, DecodeEmpty) {
    EXPECT_EQ(url::decode(""), "");
}

TEST(UrlTest, DecodeNoEncoding) {
    EXPECT_EQ(url::decode("hello"), "hello");
}

TEST(UrlTest, DecodePercentEncoded) {
    EXPECT_EQ(url::decode("hello%20world"), "hello world");
}

TEST(UrlTest, DecodeSpecialChars) {
    EXPECT_EQ(url::decode("a%26b%3Dc"), "a&b=c");
}

TEST(UrlTest, Roundtrip) {
    std::string original = "hello world & special chars=123";
    EXPECT_EQ(url::decode(url::encode(original)), original);
}

// ---- is_url_path_absolute ----

TEST(UrlPathTest, IsAbsolute) {
    EXPECT_TRUE(is_url_path_absolute("/foo/bar"));
    EXPECT_TRUE(is_url_path_absolute("/"));
}

TEST(UrlPathTest, IsRelative) {
    EXPECT_FALSE(is_url_path_absolute("foo/bar"));
    EXPECT_FALSE(is_url_path_absolute(""));
}

// ---- url_path_base_name ----

TEST(UrlPathTest, BaseName) {
    EXPECT_EQ(url_path_base_name("/foo/bar.txt"), "bar.txt");
    EXPECT_EQ(url_path_base_name("/foo/bar"), "bar");
    EXPECT_EQ(url_path_base_name("/"), "");
}

// ---- url_path_directory_name ----

TEST(UrlPathTest, DirectoryName) {
    EXPECT_EQ(url_path_directory_name("/foo/bar.txt"), "/foo/");
    EXPECT_EQ(url_path_directory_name("/foo/bar"), "/foo/");
    EXPECT_EQ(url_path_directory_name("/"), "/");
}

// ---- url_path_append ----

TEST(UrlPathTest, Append) {
    EXPECT_EQ(url_path_append("/foo/", "bar"), "/foo/bar");
    EXPECT_EQ(url_path_append("/foo", "bar"), "/foo/bar");
    EXPECT_EQ(url_path_append("/foo/", "/bar"), "/foo//bar");
}
