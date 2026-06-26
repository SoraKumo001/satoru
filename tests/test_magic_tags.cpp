#include <gtest/gtest.h>
#include "magic_tags.h"

using namespace satoru;

// ---- MagicTag enum values ----

TEST(MagicTagTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::Shadow), 1);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::TextShadow), 2);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::TextDraw), 3);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::FilterPush), 4);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::FilterPop), 5);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::LayerPush), 6);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::LayerPop), 7);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::ClipPush), 8);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::ClipPop), 9);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::GlyphPath), 10);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::ClipPathPush), 11);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::ClipPathPop), 12);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::BackdropFilterPush), 13);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::BackdropFilterPop), 14);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::MaskPush), 15);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::MaskPop), 16);
    EXPECT_EQ(static_cast<uint8_t>(MagicTag::LayerPushBlend), 17);
}

// ---- MagicTagExtended enum values ----

TEST(MagicTagExtendedTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(MagicTagExtended::ImageDraw), 0);
    EXPECT_EQ(static_cast<uint8_t>(MagicTagExtended::ConicGradient), 1);
    EXPECT_EQ(static_cast<uint8_t>(MagicTagExtended::RadialGradient), 2);
    EXPECT_EQ(static_cast<uint8_t>(MagicTagExtended::LinearGradient), 3);
    EXPECT_EQ(static_cast<uint8_t>(MagicTagExtended::InlineSvg), 4);
    EXPECT_EQ(static_cast<uint8_t>(MagicTagExtended::BorderImage), 5);
    EXPECT_EQ(static_cast<uint8_t>(MagicTagExtended::TextClipLinearGradient), 6);
}

// ---- make_magic_color (MagicTag) ----

TEST(MakeMagicColorTest, DefaultIndex) {
    SkColor c = make_magic_color(MagicTag::Shadow);
    EXPECT_EQ(SkColorGetA(c), 255);
    EXPECT_EQ(SkColorGetR(c), 0);  // index=0, type=0 → R bits all zero
    EXPECT_EQ(SkColorGetG(c), 1);  // tag value
    EXPECT_EQ(SkColorGetB(c), 0);  // index low bits
}

TEST(MakeMagicColorTest, WithIndex) {
    SkColor c = make_magic_color(MagicTag::TextShadow, 42);
    EXPECT_EQ(SkColorGetA(c), 255);
    EXPECT_EQ(SkColorGetG(c), 2);  // tag value
    EXPECT_EQ(SkColorGetB(c), 42); // index low 8 bits
}

TEST(MakeMagicColorTest, HighIndex) {
    // index=256 → high 6 bits = 1, low 8 bits = 0
    SkColor c = make_magic_color(MagicTag::Shadow, 256);
    EXPECT_EQ(SkColorGetA(c), 255);
    EXPECT_EQ(SkColorGetR(c), 4);  // (256>>8)&0x3F = 1, shifted left 2 = 4
    EXPECT_EQ(SkColorGetB(c), 0);  // 256 & 0xFF = 0
}

// ---- make_magic_color (MagicTagExtended) ----

TEST(MakeMagicColorExtendedTest, DefaultIndex) {
    SkColor c = make_magic_color(MagicTagExtended::ImageDraw);
    EXPECT_EQ(SkColorGetA(c), 255);
    EXPECT_EQ(SkColorGetR(c) & 0x03, 1);  // type bit = 1 (extended)
    EXPECT_EQ(SkColorGetG(c), 0);  // tag value
    EXPECT_EQ(SkColorGetB(c), 0);
}

TEST(MakeMagicColorExtendedTest, WithIndex) {
    SkColor c = make_magic_color(MagicTagExtended::LinearGradient, 100);
    EXPECT_EQ(SkColorGetA(c), 255);
    EXPECT_EQ(SkColorGetR(c) & 0x03, 1);  // type bit
    EXPECT_EQ(SkColorGetG(c), 3);  // tag value
    EXPECT_EQ(SkColorGetB(c), 100);
}

// ---- decode_magic_color ----

TEST(DecodeMagicColorTest, NotMagic) {
    // R=0x02 → type bits = 10 (binary) = 2 → not magic
    auto result = decode_magic_color(0x02, 0x05, 0x01);
    EXPECT_FALSE(result.is_magic);
}

TEST(DecodeMagicColorTest, MagicTag) {
    // R=0x00 → type bits = 00 → magic (not extended), G=3, B=5
    // index = ((0 >> 2) << 8) | 5 = 5
    auto result = decode_magic_color(0x00, 0x03, 0x05);
    EXPECT_TRUE(result.is_magic);
    EXPECT_FALSE(result.is_extended);
    EXPECT_EQ(result.tag_value, 3);
    EXPECT_EQ(result.index, 5);
}

TEST(DecodeMagicColorTest, MagicTagExtended) {
    // R=0x01 → type bits = 01 → extended, G=7, B=200
    // index = ((1 >> 2) << 8) | 200 = 200
    auto result = decode_magic_color(0x01, 0x07, 200);
    EXPECT_TRUE(result.is_magic);
    EXPECT_TRUE(result.is_extended);
    EXPECT_EQ(result.tag_value, 7);
    EXPECT_EQ(result.index, 200);
}

// ---- Roundtrip encode/decode ----

TEST(MagicTagRoundtrip, Basic) {
    for (int idx = 0; idx < 10; ++idx) {
        SkColor c = make_magic_color(MagicTag::Shadow, idx);
        uint8_t r = SkColorGetR(c);
        uint8_t g = SkColorGetG(c);
        uint8_t b = SkColorGetB(c);
        auto decoded = decode_magic_color(r, g, b);
        EXPECT_TRUE(decoded.is_magic);
        EXPECT_FALSE(decoded.is_extended);
        EXPECT_EQ(decoded.tag_value, static_cast<int>(MagicTag::Shadow));
        EXPECT_EQ(decoded.index, idx);
    }
}

TEST(MagicTagRoundtrip, Extended) {
    for (int idx = 0; idx < 10; ++idx) {
        SkColor c = make_magic_color(MagicTagExtended::LinearGradient, idx);
        uint8_t r = SkColorGetR(c);
        uint8_t g = SkColorGetG(c);
        uint8_t b = SkColorGetB(c);
        auto decoded = decode_magic_color(r, g, b);
        EXPECT_TRUE(decoded.is_magic);
        EXPECT_TRUE(decoded.is_extended);
        EXPECT_EQ(decoded.tag_value, static_cast<int>(MagicTagExtended::LinearGradient));
        EXPECT_EQ(decoded.index, idx);
    }
}
