#include <gtest/gtest.h>
#include "core/logical_geometry.h"
#include "libs/litehtml/include/litehtml/types.h"

// ──────────────────────────────────────────────
// Wave 7: LiteHTML Extensions Logic Tests
//
// The litehtml_extensions.cpp methods are thin wrappers over WritingModeContext
// (tested in Wave 4). The unique logic is:
//   1. block_start_pos()  — vertical-rl: container_width - x - width
//   2. block_shift()      — vertical-rl: x -= delta (others: x += delta or y += delta)
//   3. inline_shift()     — vertical: y += delta, horizontal: x += delta
//   4. place_logical()    — delegates to wm.to_physical() then place()
//   5. flex_item_row/column_direction::finalize_position()
//
// We test these by exercising WritingModeContext + logical_pos directly,
// which is the same math the wrapper methods use.
// ──────────────────────────────────────────────

using namespace litehtml;
using namespace satoru;

// ──────────────────────────────────────────────
// Helpers that mirror the extension logic
// ──────────────────────────────────────────────

/// Mirrors render_item::logical_accessor::block_start_pos()
static pixel_t compute_block_start_pos(const WritingModeContext& wm,
                                        pixel_t x, pixel_t y, pixel_t w, pixel_t /*h*/) {
    if (wm.mode() == writing_mode_vertical_rl)
        return wm.container_width() - x - w;
    return wm.is_vertical() ? x : y;
}

/// Mirrors render_item::logical_accessor::inline_start_pos()
static pixel_t compute_inline_start_pos(const WritingModeContext& wm,
                                         pixel_t x, pixel_t y) {
    return wm.is_vertical() ? y : x;
}

/// Mirrors render_item::inline_shift() — returns new (x, y)
static std::pair<pixel_t, pixel_t> apply_inline_shift(const WritingModeContext& wm,
                                                       pixel_t x, pixel_t y, pixel_t delta) {
    if (wm.is_vertical())
        return {x, y + delta};
    else
        return {x + delta, y};
}

/// Mirrors render_item::block_shift() — returns new (x, y)
static std::pair<pixel_t, pixel_t> apply_block_shift(const WritingModeContext& wm,
                                                      pixel_t x, pixel_t y, pixel_t delta) {
    if (wm.is_vertical()) {
        if (wm.mode() == writing_mode_vertical_rl)
            return {x - delta, y};
        else
            return {x + delta, y};
    } else {
        return {x, y + delta};
    }
}

/// Mirrors flex_item_row_direction::finalize_position()
static litehtml::position flex_row_finalize(const WritingModeContext& wm,
                                             pixel_t main_pos, pixel_t cross_pos,
                                             pixel_t el_width, pixel_t el_height,
                                             pixel_t offset_left, pixel_t offset_top) {
    logical_pos pos(main_pos, cross_pos);
    logical_size size(el_width, el_height);
    litehtml::position phys_pos = wm.to_physical(pos, size);
    phys_pos.x += offset_left;
    phys_pos.y += offset_top;
    return phys_pos;
}

/// Mirrors flex_item_column_direction::finalize_position()
static litehtml::position flex_column_finalize(const WritingModeContext& wm,
                                                pixel_t main_pos, pixel_t cross_pos,
                                                pixel_t el_width, pixel_t el_height,
                                                pixel_t offset_left, pixel_t offset_top) {
    logical_pos pos(cross_pos, main_pos);
    logical_size size(el_height, el_width);
    litehtml::position phys_pos = wm.to_physical(pos, size);
    phys_pos.x += offset_left;
    phys_pos.y += offset_top;
    return phys_pos;
}

// ──────────────────────────────────────────────
// block_start_pos tests
// ──────────────────────────────────────────────

TEST(BlockStartPosTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 20);
}

TEST(BlockStartPosTest, HorizontalTb_ZeroValues) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 0, 0, 0, 0), 0);
}

TEST(BlockStartPosTest, VerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 10);
}

TEST(BlockStartPosTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 590);
}

TEST(BlockStartPosTest, VerticalRl_AtLeftEdge) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 0, 20, 200, 100), 600);
}

TEST(BlockStartPosTest, VerticalRl_ZeroWidth) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 0, 100), 790);
}

TEST(BlockStartPosTest, SidewaysRl) {
    // sideways-rl is NOT vertical per WritingModeContext.is_vertical()
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 20);
}

TEST(BlockStartPosTest, SidewaysLr) {
    // sideways-lr is NOT vertical per WritingModeContext.is_vertical()
    WritingModeContext wm(writing_mode_sideways_lr, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 20);
}

// ──────────────────────────────────────────────
// inline_start_pos tests
// ──────────────────────────────────────────────

TEST(InlineStartPosTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_inline_start_pos(wm, 10, 20), 10);
}

TEST(InlineStartPosTest, Vertical) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    EXPECT_EQ(compute_inline_start_pos(wm, 10, 20), 20);
}

// ──────────────────────────────────────────────
// inline_shift tests
// ──────────────────────────────────────────────

TEST(InlineShiftTest, HorizontalMovesX) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_inline_shift(wm, 10, 20, 50);
    EXPECT_EQ(x, 60);
    EXPECT_EQ(y, 20);
}

TEST(InlineShiftTest, VerticalMovesY) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto [x, y] = apply_inline_shift(wm, 10, 20, 50);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 70);
}

TEST(InlineShiftTest, ZeroDelta) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_inline_shift(wm, 10, 20, 0);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 20);
}

TEST(InlineShiftTest, NegativeDelta) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_inline_shift(wm, 10, 20, -30);
    EXPECT_EQ(x, -20);
    EXPECT_EQ(y, 20);
}

// ──────────────────────────────────────────────
// block_shift tests
// ──────────────────────────────────────────────

TEST(BlockShiftTest, HorizontalMovesY) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_block_shift(wm, 10, 20, 50);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 70);
}

TEST(BlockShiftTest, VerticalRlMovesXNegative) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto [x, y] = apply_block_shift(wm, 10, 20, 50);
    EXPECT_EQ(x, -40);
    EXPECT_EQ(y, 20);
}

TEST(BlockShiftTest, VerticalLrMovesXPositive) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto [x, y] = apply_block_shift(wm, 10, 20, 50);
    EXPECT_EQ(x, 60);
    EXPECT_EQ(y, 20);
}

TEST(BlockShiftTest, SidewaysRlMovesY) {
    // sideways-rl is NOT vertical per WritingModeContext.is_vertical()
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    auto [x, y] = apply_block_shift(wm, 10, 20, 50);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 70);
}

TEST(BlockShiftTest, NegativeDelta_Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_block_shift(wm, 10, 20, -30);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, -10);
}

TEST(BlockShiftTest, NegativeDelta_VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto [x, y] = apply_block_shift(wm, 10, 20, -30);
    EXPECT_EQ(x, 40);
    EXPECT_EQ(y, 20);
}

// ──────────────────────────────────────────────
// flex_item_row_direction::finalize_position tests
// ──────────────────────────────────────────────

TEST(FlexRowFinalizeTest, Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_row_finalize(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 110);
    EXPECT_EQ(pos.y, 66);
}

TEST(FlexRowFinalizeTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto pos = flex_row_finalize(wm, 100, 50, 200, 100, 10, 16);
    // to_physical(vert_rl): x = 800 - 50 - 100 = 650, y = 100
    // + offsets: x = 660, y = 116
    EXPECT_EQ(pos.x, 660);
    EXPECT_EQ(pos.y, 116);
}

TEST(FlexRowFinalizeTest, VerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto pos = flex_row_finalize(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 60);
    EXPECT_EQ(pos.y, 116);
}

TEST(FlexRowFinalizeTest, ZeroOffsets) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_row_finalize(wm, 100, 50, 200, 100, 0, 0);
    EXPECT_EQ(pos.x, 100);
    EXPECT_EQ(pos.y, 50);
}

// ──────────────────────────────────────────────
// flex_item_column_direction::finalize_position tests
// ──────────────────────────────────────────────

TEST(FlexColumnFinalizeTest, Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_column_finalize(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 60);
    EXPECT_EQ(pos.y, 116);
}

TEST(FlexColumnFinalizeTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto pos = flex_column_finalize(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 510);
    EXPECT_EQ(pos.y, 66);
}

TEST(FlexColumnFinalizeTest, SymmetricSquare) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_column_finalize(wm, 50, 50, 100, 100, 0, 0);
    EXPECT_EQ(pos.x, 50);
    EXPECT_EQ(pos.y, 50);
}

// ──────────────────────────────────────────────
// place_logical equivalent (via to_physical)
// ──────────────────────────────────────────────

TEST(PlaceLogicalTest, Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    logical_pos lpos(200, 300);
    logical_size lsize(200, 100);
    auto phys = wm.to_physical(lpos, lsize);
    EXPECT_EQ(phys.x, 200);
    EXPECT_EQ(phys.y, 300);
}

TEST(PlaceLogicalTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    logical_pos lpos(200, 300);  // inline=200, block=300
    logical_size lsize(200, 100);  // inline=200, block=100
    auto phys = wm.to_physical(lpos, lsize);
    // x = 800 - 300 - 100 = 400, y = 200
    EXPECT_EQ(phys.x, 400);
    EXPECT_EQ(phys.y, 200);
}

// ──────────────────────────────────────────────
// Box arithmetic (content offsets, total width/height)
// ──────────────────────────────────────────────

TEST(BoxArithmeticTest, ContentOffsets) {
    pixel_t margin_l = 5, margin_t = 10, margin_r = 5, margin_b = 10;
    pixel_t padding_l = 3, padding_t = 4, padding_r = 3, padding_b = 4;
    pixel_t border_l = 2, border_t = 2, border_r = 2, border_b = 2;

    pixel_t offset_left  = margin_l + padding_l + border_l;    // 10
    pixel_t offset_top   = margin_t + padding_t + border_t;    // 16
    pixel_t offset_right = margin_r + padding_r + border_r;    // 10
    pixel_t offset_bot   = margin_b + padding_b + border_b;    // 16
    pixel_t offset_w     = offset_left + offset_right;         // 20
    pixel_t offset_h     = offset_top + offset_bot;            // 32

    EXPECT_EQ(offset_left, 10);
    EXPECT_EQ(offset_top, 16);
    EXPECT_EQ(offset_right, 10);
    EXPECT_EQ(offset_bot, 16);
    EXPECT_EQ(offset_w, 20);
    EXPECT_EQ(offset_h, 32);
}

TEST(BoxArithmeticTest, TotalWidthHeight) {
    pixel_t content_w = 200, content_h = 100;
    pixel_t m_l = 5, m_t = 10, m_r = 5, m_b = 10;
    pixel_t p_l = 3, p_t = 4, p_r = 3, p_b = 4;
    pixel_t b_l = 2, b_t = 2, b_r = 2, b_b = 2;

    pixel_t total_w = content_w + (m_l + m_r) + (p_l + p_r) + (b_l + b_r);
    pixel_t total_h = content_h + (m_t + m_b) + (p_t + p_b) + (b_t + b_b);

    EXPECT_EQ(total_w, 220);
    EXPECT_EQ(total_h, 132);
}

// ──────────────────────────────────────────────
// Edge cases
// ──────────────────────────────────────────────

TEST(EdgeCaseTest, LargeValues) {
    WritingModeContext wm(writing_mode_vertical_rl, 10000, 10000);
    pixel_t bs = compute_block_start_pos(wm, 5000, 0, 4000, 0);
    EXPECT_EQ(bs, 1000);
}

TEST(EdgeCaseTest, OverflowPosition) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    pixel_t bs = compute_block_start_pos(wm, 750, 0, 100, 0);
    EXPECT_EQ(bs, -50);
}

TEST(EdgeCaseTest, FlexRowAtContainerEdge) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_row_finalize(wm, 0, 0, 800, 600, 0, 0);
    EXPECT_EQ(pos.x, 0);
    EXPECT_EQ(pos.y, 0);
}

TEST(EdgeCaseTest, FlexColumnNegativeValues) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_column_finalize(wm, -50, -30, 100, 100, 10, 16);
    EXPECT_EQ(pos.x, -20);
    EXPECT_EQ(pos.y, -34);
}
