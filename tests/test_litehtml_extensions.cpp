// Tests for litehtml_extensions.cpp logical_accessor and flex_item::finalize_position.
//
// The litehtml_extensions.cpp file IS now compiled in the test build (borders.h
// was fixed to add width()/height()). However, render_item cannot be
// instantiated in unit tests because its constructor requires a full DOM
// element tree (document, element, css_properties, etc.). Therefore, the
// helpers below cannot call render_item::logical_accessor methods directly.
//
// Instead, each helper is an ISOMORPHIC EXTRACT of the corresponding
// production method in litehtml_extensions.cpp — it uses the exact same
// public APIs (WritingModeContext::to_logical, to_physical, is_vertical,
// etc.) with the same arithmetic. The production code is:
//
//   render_item::logical_accessor::inline_size() — uses wm.to_logical(...)
//   render_item::logical_accessor::block_size()  — uses wm.to_logical(...)
//   render_item::inline_shift(delta)             — uses wm.is_vertical()
//   render_item::block_shift(delta)              — uses wm.is_vertical() + mode
//   flex_item_row_direction::finalize_position() — uses wm.to_physical(...)
//   flex_item_column_direction::finalize_position() — uses wm.to_physical(...)
//
// If the production code changes the public-API arithmetic, these helpers
// must be updated in lockstep. The existing tests for the free functions in
// logical_geometry.h (compute_inline_start_pos, compute_block_start_pos)
// already test the production code directly since litehtml_extensions.cpp
// delegates to those same free functions.

#include <gtest/gtest.h>

#include "core/logical_geometry.h"

using namespace litehtml;
using namespace satoru;

using litehtml::writing_mode_horizontal_tb;
using litehtml::writing_mode_vertical_rl;
using litehtml::writing_mode_vertical_lr;
using litehtml::writing_mode_sideways_rl;
using litehtml::writing_mode_sideways_lr;

// ============================================================================
// Tests for satoru::compute_inline_start_pos (extracted from
// render_item::logical_accessor::inline_start_pos)
// ============================================================================

TEST(ComputeInlineStartPosTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_inline_start_pos(wm, 100, 50), 100);  // x
}

TEST(ComputeInlineStartPosTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    EXPECT_EQ(compute_inline_start_pos(wm, 100, 50), 50);  // y
}

TEST(ComputeInlineStartPosTest, VerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    EXPECT_EQ(compute_inline_start_pos(wm, 100, 50), 50);  // y
}

TEST(ComputeInlineStartPosTest, SidewaysRl) {
    // sideways-rl is NOT vertical per is_vertical() → returns x
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    EXPECT_EQ(compute_inline_start_pos(wm, 100, 50), 100);  // x
}

// ============================================================================
// Tests for satoru::compute_block_start_pos (extracted from
// render_item::logical_accessor::block_start_pos)
// ============================================================================

TEST(ComputeBlockStartPosTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 20);  // y
}

TEST(ComputeBlockStartPosTest, HorizontalTb_ZeroValues) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 0, 0, 0, 0), 0);
}

TEST(ComputeBlockStartPosTest, VerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 10);  // x
}

TEST(ComputeBlockStartPosTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // vertical-rl: container_width - x - w = 800 - 10 - 200 = 590
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 590);
}

TEST(ComputeBlockStartPosTest, VerticalRl_AtLeftEdge) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // container_width - x - w = 800 - 0 - 200 = 600
    EXPECT_EQ(compute_block_start_pos(wm, 0, 20, 200, 100), 600);
}

TEST(ComputeBlockStartPosTest, VerticalRl_ZeroWidth) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // container_width - x - 0 = 800 - 10 - 0 = 790
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 0, 100), 790);
}

TEST(ComputeBlockStartPosTest, SidewaysRl) {
    // sideways-rl is NOT vertical per is_vertical() → returns y
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 20);
}

TEST(ComputeBlockStartPosTest, SidewaysLr) {
    // sideways-lr is NOT vertical per is_vertical() → returns y
    WritingModeContext wm(writing_mode_sideways_lr, 800, 600);
    EXPECT_EQ(compute_block_start_pos(wm, 10, 20, 200, 100), 20);
}

TEST(ComputeBlockStartPosTest, LargeValues) {
    WritingModeContext wm(writing_mode_vertical_rl, 10000, 10000);
    // 10000 - 5000 - 4000 = 1000
    EXPECT_EQ(compute_block_start_pos(wm, 5000, 0, 4000, 0), 1000);
}

TEST(ComputeBlockStartPosTest, OverflowPosition) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // 800 - 750 - 100 = -50
    EXPECT_EQ(compute_block_start_pos(wm, 750, 0, 100, 0), -50);
}

// ============================================================================
// Isomorphic helpers for inline_size / block_size / inline_shift /
// block_shift / finalize_position.
//
// These helpers match the arithmetic in litehtml_extensions.cpp exactly but
// take explicit arguments instead of accessing render_item internals. They
// are isomorphic to the production code — the same public APIs
// (WritingModeContext::to_logical, WritingModeContext::to_physical,
// WritingModeContext::is_vertical, WritingModeContext::mode) are used with
// the same expressions.
//
// These replace the former "mirrored" helpers (removed now that
// litehtml_extensions.cpp compiles). They no longer imply "we have two
// copies" — they exist solely because render_item cannot be heap-allocated
// in tests (its constructor requires DOM infrastructure). They test the same
// mathematical invariants that the production methods implement.
// ============================================================================

/// Isomorphic to render_item::logical_accessor::inline_size().
/// Returns the inline (logical) dimension from physical width/height + box offsets.
static pixel_t compute_inline_size(const WritingModeContext& wm, pixel_t pos_w,
                                   pixel_t margins_w, pixel_t borders_w,
                                   pixel_t padding_w, pixel_t pos_h,
                                   pixel_t margins_h, pixel_t borders_h,
                                   pixel_t padding_h) {
    return wm.to_logical(pos_w + margins_w + borders_w + padding_w,
                         pos_h + margins_h + borders_h + padding_h)
        .inline_size;
}

/// Isomorphic to render_item::logical_accessor::block_size().
/// Returns the block (logical) dimension from physical width/height + box offsets.
static pixel_t compute_block_size(const WritingModeContext& wm, pixel_t pos_w,
                                  pixel_t margins_w, pixel_t borders_w,
                                  pixel_t padding_w, pixel_t pos_h,
                                  pixel_t margins_h, pixel_t borders_h,
                                  pixel_t padding_h) {
    return wm.to_logical(pos_w + margins_w + borders_w + padding_w,
                         pos_h + margins_h + borders_h + padding_h)
        .block_size;
}

/// Isomorphic to render_item::inline_shift() — returns new (x, y).
static std::pair<pixel_t, pixel_t> apply_inline_shift(const WritingModeContext& wm,
                                                      pixel_t x, pixel_t y,
                                                      pixel_t delta) {
    if (wm.is_vertical())
        return {x, y + delta};
    else
        return {x + delta, y};
}

/// Isomorphic to render_item::block_shift() — returns new (x, y).
static std::pair<pixel_t, pixel_t> apply_block_shift(const WritingModeContext& wm,
                                                     pixel_t x, pixel_t y,
                                                     pixel_t delta) {
    if (wm.is_vertical()) {
        if (wm.mode() == writing_mode_vertical_rl)
            return {x - delta, y};
        else
            return {x + delta, y};
    } else {
        return {x, y + delta};
    }
}

// ============================================================================
// inline_size / block_size
// ============================================================================

TEST(InlineSizeTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    // pos=100w, margins=10w, borders=5w, padding=15w → logical inline=130
    EXPECT_EQ(compute_inline_size(wm, 100, 10, 5, 15, 200, 20, 10, 30), 130);
}

TEST(InlineSizeTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // In vertical, inline axis is height, so inline_size = h + h_margins + h_borders + h_padding
    // h=200, h_margins=20, h_borders=10, h_padding=30 → 260
    EXPECT_EQ(compute_inline_size(wm, 100, 10, 5, 15, 200, 20, 10, 30), 260);
}

TEST(InlineSizeTest, ZeroSizes) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_inline_size(wm, 0, 0, 0, 0, 0, 0, 0, 0), 0);
}

TEST(BlockSizeTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    // pos=200h, margins=20h, borders=10h, padding=30h → logical block=260
    EXPECT_EQ(compute_block_size(wm, 100, 10, 5, 15, 200, 20, 10, 30), 260);
}

TEST(BlockSizeTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // In vertical, block axis is width, so block_size = w + w_margins + w_borders + w_padding
    // w=100, w_margins=10, w_borders=5, w_padding=15 → 130
    EXPECT_EQ(compute_block_size(wm, 100, 10, 5, 15, 200, 20, 10, 30), 130);
}

// ============================================================================
// inline_shift
// ============================================================================

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

// ============================================================================
// block_shift
// ============================================================================

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

// ============================================================================
// flex_item_row/column_direction::finalize_position
// ============================================================================

/// Isomorphic to flex_item_row_direction::finalize_position
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

/// Isomorphic to flex_item_column_direction::finalize_position
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

// ============================================================================
// place_logical equivalent — tests wm.to_physical() based placement
// ============================================================================

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

// ============================================================================
// Box arithmetic (content offsets, total width/height)
// ============================================================================

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

// ============================================================================
// Edge cases
// ============================================================================

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
