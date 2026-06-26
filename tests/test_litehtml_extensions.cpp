// Tests for litehtml_extensions.cpp logical_accessor and flex_item::finalize_position.
//
// The litehtml_extensions.cpp file CANNOT be compiled in the test build because
// it has pre-existing compile errors (litehtml::borders::width()/height() do not
// exist, but the inline_size/block_size methods use them). The test build
// excludes litehtml_extensions.cpp and render_item.cpp for that reason.
//
// To still verify the production code's mathematical behavior, the pure
// computation functions have been extracted into logical_geometry.h as free
// functions. The production code's logical_accessor methods delegate to
// these same free functions. So testing the free functions tests the real
// production math.
//
// For the inline_size/block_size methods (which have the compile bug), the
// mirrored helpers in the original test file are kept with a clear comment
// noting they should be removed once the production bug is fixed.
//
// For the flex_item_row_direction::finalize_position and
// flex_item_column_direction::finalize_position methods, mirrored helpers are
// used because those methods depend on flex_item internals that can't be
// instantiated in tests.

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
// Mirrored helpers for tests that cannot use the actual production code
// (litehtml_extensions.cpp is not compiled in the test build).
//
// These mirror the production code in litehtml_extensions.cpp. If you change
// the production code, you MUST update these helpers and add a test case to
// verify the change is correctly mirrored.
//
// Future: Once litehtml::borders::width()/height() are added (or the size
// methods are refactored), these helpers should be replaced with direct
// calls to render_item::logical_accessor methods.
// ============================================================================

/// Mirrors render_item::logical_accessor::inline_size() — but DOES NOT call
/// the real method because litehtml_extensions.cpp is not compiled in tests.
static pixel_t compute_inline_size_mirrored(const WritingModeContext& wm, pixel_t pos_w,
                                            pixel_t margins_w, pixel_t borders_w,
                                            pixel_t padding_w, pixel_t pos_h,
                                            pixel_t margins_h, pixel_t borders_h,
                                            pixel_t padding_h) {
    return wm.to_logical(pos_w + margins_w + borders_w + padding_w,
                         pos_h + margins_h + borders_h + padding_h)
        .inline_size;
}

/// Mirrors render_item::logical_accessor::block_size() — see comment above.
static pixel_t compute_block_size_mirrored(const WritingModeContext& wm, pixel_t pos_w,
                                           pixel_t margins_w, pixel_t borders_w,
                                           pixel_t padding_w, pixel_t pos_h,
                                           pixel_t margins_h, pixel_t borders_h,
                                           pixel_t padding_h) {
    return wm.to_logical(pos_w + margins_w + borders_w + padding_w,
                         pos_h + margins_h + borders_h + padding_h)
        .block_size;
}

/// Mirrors render_item::inline_shift() — returns new (x, y).
static std::pair<pixel_t, pixel_t> apply_inline_shift_mirrored(const WritingModeContext& wm,
                                                               pixel_t x, pixel_t y,
                                                               pixel_t delta) {
    if (wm.is_vertical())
        return {x, y + delta};
    else
        return {x + delta, y};
}

/// Mirrors render_item::block_shift() — returns new (x, y).
static std::pair<pixel_t, pixel_t> apply_block_shift_mirrored(const WritingModeContext& wm,
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
// inline_size / block_size (mirrored — see note above)
// ============================================================================

TEST(InlineSizeMirroredTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    // pos=100w, margins=10w, borders=5w, padding=15w → logical inline=130
    EXPECT_EQ(compute_inline_size_mirrored(wm, 100, 10, 5, 15, 200, 20, 10, 30), 130);
}

TEST(InlineSizeMirroredTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // In vertical, inline axis is height, so inline_size = h + h_margins + h_borders + h_padding
    // h=200, h_margins=20, h_borders=10, h_padding=30 → 260
    EXPECT_EQ(compute_inline_size_mirrored(wm, 100, 10, 5, 15, 200, 20, 10, 30), 260);
}

TEST(InlineSizeMirroredTest, ZeroSizes) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_EQ(compute_inline_size_mirrored(wm, 0, 0, 0, 0, 0, 0, 0, 0), 0);
}

TEST(BlockSizeMirroredTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    // pos=200h, margins=20h, borders=10h, padding=30h → logical block=260
    EXPECT_EQ(compute_block_size_mirrored(wm, 100, 10, 5, 15, 200, 20, 10, 30), 260);
}

TEST(BlockSizeMirroredTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // In vertical, block axis is width, so block_size = w + w_margins + w_borders + w_padding
    // w=100, w_margins=10, w_borders=5, w_padding=15 → 130
    EXPECT_EQ(compute_block_size_mirrored(wm, 100, 10, 5, 15, 200, 20, 10, 30), 130);
}

// ============================================================================
// inline_shift (mirrored — see note above)
// ============================================================================

TEST(InlineShiftMirroredTest, HorizontalMovesX) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_inline_shift_mirrored(wm, 10, 20, 50);
    EXPECT_EQ(x, 60);
    EXPECT_EQ(y, 20);
}

TEST(InlineShiftMirroredTest, VerticalMovesY) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto [x, y] = apply_inline_shift_mirrored(wm, 10, 20, 50);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 70);
}

TEST(InlineShiftMirroredTest, ZeroDelta) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_inline_shift_mirrored(wm, 10, 20, 0);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 20);
}

TEST(InlineShiftMirroredTest, NegativeDelta) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_inline_shift_mirrored(wm, 10, 20, -30);
    EXPECT_EQ(x, -20);
    EXPECT_EQ(y, 20);
}

// ============================================================================
// block_shift (mirrored — see note above)
// ============================================================================

TEST(BlockShiftMirroredTest, HorizontalMovesY) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_block_shift_mirrored(wm, 10, 20, 50);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 70);
}

TEST(BlockShiftMirroredTest, VerticalRlMovesXNegative) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto [x, y] = apply_block_shift_mirrored(wm, 10, 20, 50);
    EXPECT_EQ(x, -40);
    EXPECT_EQ(y, 20);
}

TEST(BlockShiftMirroredTest, VerticalLrMovesXPositive) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto [x, y] = apply_block_shift_mirrored(wm, 10, 20, 50);
    EXPECT_EQ(x, 60);
    EXPECT_EQ(y, 20);
}

TEST(BlockShiftMirroredTest, SidewaysRlMovesY) {
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    auto [x, y] = apply_block_shift_mirrored(wm, 10, 20, 50);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 70);
}

TEST(BlockShiftMirroredTest, NegativeDelta_Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto [x, y] = apply_block_shift_mirrored(wm, 10, 20, -30);
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, -10);
}

TEST(BlockShiftMirroredTest, NegativeDelta_VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto [x, y] = apply_block_shift_mirrored(wm, 10, 20, -30);
    EXPECT_EQ(x, 40);
    EXPECT_EQ(y, 20);
}

// ============================================================================
// flex_item_row/column_direction::finalize_position (mirrored — see note)
// ============================================================================

/// Mirrors flex_item_row_direction::finalize_position
static litehtml::position flex_row_finalize_mirrored(const WritingModeContext& wm,
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

/// Mirrors flex_item_column_direction::finalize_position
static litehtml::position flex_column_finalize_mirrored(const WritingModeContext& wm,
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

TEST(FlexRowFinalizeMirroredTest, Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_row_finalize_mirrored(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 110);
    EXPECT_EQ(pos.y, 66);
}

TEST(FlexRowFinalizeMirroredTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto pos = flex_row_finalize_mirrored(wm, 100, 50, 200, 100, 10, 16);
    // to_physical(vert_rl): x = 800 - 50 - 100 = 650, y = 100
    // + offsets: x = 660, y = 116
    EXPECT_EQ(pos.x, 660);
    EXPECT_EQ(pos.y, 116);
}

TEST(FlexRowFinalizeMirroredTest, VerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto pos = flex_row_finalize_mirrored(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 60);
    EXPECT_EQ(pos.y, 116);
}

TEST(FlexRowFinalizeMirroredTest, ZeroOffsets) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_row_finalize_mirrored(wm, 100, 50, 200, 100, 0, 0);
    EXPECT_EQ(pos.x, 100);
    EXPECT_EQ(pos.y, 50);
}

TEST(FlexColumnFinalizeMirroredTest, Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_column_finalize_mirrored(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 60);
    EXPECT_EQ(pos.y, 116);
}

TEST(FlexColumnFinalizeMirroredTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto pos = flex_column_finalize_mirrored(wm, 100, 50, 200, 100, 10, 16);
    EXPECT_EQ(pos.x, 510);
    EXPECT_EQ(pos.y, 66);
}

TEST(FlexColumnFinalizeMirroredTest, SymmetricSquare) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_column_finalize_mirrored(wm, 50, 50, 100, 100, 0, 0);
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
    auto pos = flex_row_finalize_mirrored(wm, 0, 0, 800, 600, 0, 0);
    EXPECT_EQ(pos.x, 0);
    EXPECT_EQ(pos.y, 0);
}

TEST(EdgeCaseTest, FlexColumnNegativeValues) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = flex_column_finalize_mirrored(wm, -50, -30, 100, 100, 10, 16);
    EXPECT_EQ(pos.x, -20);
    EXPECT_EQ(pos.y, -34);
}
