// Tests for logical_geometry.h — WritingMode coordinate transforms.
// Pure logical tests — no Skia, no litehtml customizations — uses real litehtml types.

#include <gtest/gtest.h>
#include <litehtml.h>
#include "core/logical_geometry.h"

using namespace satoru;
using litehtml::writing_mode;
using litehtml::writing_mode_horizontal_tb;
using litehtml::writing_mode_vertical_rl;
using litehtml::writing_mode_vertical_lr;
using litehtml::writing_mode_sideways_rl;
using litehtml::writing_mode_sideways_lr;

// ============ logical_size ============

TEST(LogicalSizeTest, DefaultConstructor) {
    logical_size s;
    EXPECT_EQ(s.inline_size, 0);
    EXPECT_EQ(s.block_size, 0);
}

TEST(LogicalSizeTest, ParameterizedConstructor) {
    logical_size s(100, 200);
    EXPECT_EQ(s.inline_size, 100);
    EXPECT_EQ(s.block_size, 200);
}

TEST(LogicalSizeTest, Equality) {
    logical_size a(100, 200);
    logical_size b(100, 200);
    logical_size c(200, 100);
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
}

// ============ logical_pos ============

TEST(LogicalPosTest, DefaultConstructor) {
    logical_pos p;
    EXPECT_EQ(p.inline_offset, 0);
    EXPECT_EQ(p.block_offset, 0);
}

TEST(LogicalPosTest, ParameterizedConstructor) {
    logical_pos p(50, 150);
    EXPECT_EQ(p.inline_offset, 50);
    EXPECT_EQ(p.block_offset, 150);
}

TEST(LogicalPosTest, Equality) {
    logical_pos a(10, 20);
    logical_pos b(10, 20);
    logical_pos c(20, 10);
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
}

TEST(LogicalPosTest, Addition) {
    logical_pos a(10, 20);
    logical_pos b(30, 40);
    auto r = a + b;
    EXPECT_EQ(r.inline_offset, 40);
    EXPECT_EQ(r.block_offset, 60);
}

// ============ logical_rect ============

TEST(LogicalRectTest, DefaultConstructor) {
    logical_rect r;
    EXPECT_EQ(r.inline_offset, 0);
    EXPECT_EQ(r.block_offset, 0);
    EXPECT_EQ(r.inline_size, 0);
    EXPECT_EQ(r.block_size, 0);
}

TEST(LogicalRectTest, ParameterizedConstructor) {
    logical_rect r(10, 20, 100, 200);
    EXPECT_EQ(r.inline_offset, 10);
    EXPECT_EQ(r.block_offset, 20);
    EXPECT_EQ(r.inline_size, 100);
    EXPECT_EQ(r.block_size, 200);
}

TEST(LogicalRectTest, Pos) {
    logical_rect r(10, 20, 100, 200);
    auto p = r.pos();
    EXPECT_EQ(p.inline_offset, 10);
    EXPECT_EQ(p.block_offset, 20);
}

TEST(LogicalRectTest, Size) {
    logical_rect r(10, 20, 100, 200);
    auto s = r.size();
    EXPECT_EQ(s.inline_size, 100);
    EXPECT_EQ(s.block_size, 200);
}

TEST(LogicalRectTest, InlineStartEnd) {
    logical_rect r(10, 20, 100, 200);
    EXPECT_EQ(r.inline_start(), 10);
    EXPECT_EQ(r.inline_end(), 110);
}

TEST(LogicalRectTest, BlockStartEnd) {
    logical_rect r(10, 20, 100, 200);
    EXPECT_EQ(r.block_start(), 20);
    EXPECT_EQ(r.block_end(), 220);
}

TEST(LogicalRectTest, ToPhysicalHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    logical_rect r(10, 20, 100, 200);
    auto phys = r.to_physical(wm);
    EXPECT_EQ(phys.x, 10);
    EXPECT_EQ(phys.y, 20);
    EXPECT_EQ(phys.width, 100);
    EXPECT_EQ(phys.height, 200);
}

TEST(LogicalRectTest, ToPhysicalVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    logical_rect r(10, 20, 100, 200);
    auto phys = r.to_physical(wm);
    // In vertical-rl: inline=10 → y, block=20 → x from right, inline_size=100 → height, block_size=200 → width
    // x = 800 - 20 - 200 = 580, y = 10, width = 200, height = 100
    EXPECT_EQ(phys.x, 580);
    EXPECT_EQ(phys.y, 10);
    EXPECT_EQ(phys.width, 200);
    EXPECT_EQ(phys.height, 100);
}

TEST(LogicalRectTest, ToPhysicalVerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    logical_rect r(10, 20, 100, 200);
    auto phys = r.to_physical(wm);
    // In vertical-lr: inline=10 → y, block=20 → x, inline_size=100 → height, block_size=200 → width
    // x = 20, y = 10, width = 200, height = 100
    EXPECT_EQ(phys.x, 20);
    EXPECT_EQ(phys.y, 10);
    EXPECT_EQ(phys.width, 200);
    EXPECT_EQ(phys.height, 100);
}

// ============ WritingModeContext: constructor, accessors ============

TEST(WritingModeContextTest, Constructor) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    EXPECT_EQ(wm.mode(), writing_mode_vertical_rl);
    EXPECT_EQ(wm.container_width(), 800);
    EXPECT_EQ(wm.container_height(), 600);
}

TEST(WritingModeContextTest, UpdateContainerSize) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    wm.update_container_size(1024, 768);
    EXPECT_EQ(wm.container_width(), 1024);
    EXPECT_EQ(wm.container_height(), 768);
}

// ============ is_vertical ============

TEST(WritingModeContextTest, IsVerticalHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    EXPECT_FALSE(wm.is_vertical());
}

TEST(WritingModeContextTest, IsVerticalVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    EXPECT_TRUE(wm.is_vertical());
}

TEST(WritingModeContextTest, IsVerticalVerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    EXPECT_TRUE(wm.is_vertical());
}

TEST(WritingModeContextTest, IsVerticalSidewaysRl) {
    // Note: sideways-rl is NOT treated as vertical by is_vertical()
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    EXPECT_FALSE(wm.is_vertical());
}

TEST(WritingModeContextTest, IsVerticalSidewaysLr) {
    WritingModeContext wm(writing_mode_sideways_lr, 800, 600);
    EXPECT_FALSE(wm.is_vertical());
}

// ============ to_physical ============

TEST(WritingModeToPhysicalTest, HorizontalTb) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto phys = wm.to_physical({10, 20}, {100, 200});
    EXPECT_EQ(phys.x, 10);
    EXPECT_EQ(phys.y, 20);
    EXPECT_EQ(phys.width, 100);
    EXPECT_EQ(phys.height, 200);
}

TEST(WritingModeToPhysicalTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // block_offset=20, block_size=200 → x = 800 - 20 - 200 = 580
    // inline_offset=10 → y = 10
    // block_size=200 → width = 200
    // inline_size=100 → height = 100
    auto phys = wm.to_physical({10, 20}, {100, 200});
    EXPECT_EQ(phys.x, 580);
    EXPECT_EQ(phys.y, 10);
    EXPECT_EQ(phys.width, 200);
    EXPECT_EQ(phys.height, 100);
}

TEST(WritingModeToPhysicalTest, VerticalRlZeroOffset) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto phys = wm.to_physical({0, 0}, {100, 50});
    EXPECT_EQ(phys.x, 750);  // 800 - 0 - 50
    EXPECT_EQ(phys.y, 0);
    EXPECT_EQ(phys.width, 50);
    EXPECT_EQ(phys.height, 100);
}

TEST(WritingModeToPhysicalTest, VerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    // block_offset=20 → x = 20
    // inline_offset=10 → y = 10
    // block_size=200 → width = 200
    // inline_size=100 → height = 100
    auto phys = wm.to_physical({10, 20}, {100, 200});
    EXPECT_EQ(phys.x, 20);
    EXPECT_EQ(phys.y, 10);
    EXPECT_EQ(phys.width, 200);
    EXPECT_EQ(phys.height, 100);
}

TEST(WritingModeToPhysicalTest, VerticalLrZeroOffset) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto phys = wm.to_physical({0, 0}, {100, 50});
    EXPECT_EQ(phys.x, 0);
    EXPECT_EQ(phys.y, 0);
    EXPECT_EQ(phys.width, 50);
    EXPECT_EQ(phys.height, 100);
}

TEST(WritingModeToPhysicalTest, HorizontalEdgeValues) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    // Large offsets at edges
    auto phys = wm.to_physical({795, 595}, {5, 5});
    EXPECT_EQ(phys.x, 795);
    EXPECT_EQ(phys.y, 595);
    EXPECT_EQ(phys.width, 5);
    EXPECT_EQ(phys.height, 5);
}

TEST(WritingModeToPhysicalTest, VerticalRlEdgeValues) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // block_offset=795, inline_offset=595
    // x = 800 - 795 - 5 = 0, y = 595
    auto phys = wm.to_physical({595, 795}, {5, 5});
    EXPECT_EQ(phys.x, 0);    // at right edge
    EXPECT_EQ(phys.y, 595);  // near bottom
    EXPECT_EQ(phys.width, 5);
    EXPECT_EQ(phys.height, 5);
}

// ============ to_logical ============

TEST(WritingModeToLogicalTest, Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto s = wm.to_logical(100, 200);
    EXPECT_EQ(s.inline_size, 100);
    EXPECT_EQ(s.block_size, 200);
}

TEST(WritingModeToLogicalTest, Vertical) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto s = wm.to_logical(100, 200);
    // width↔height swap: inline=height=200, block=width=100
    EXPECT_EQ(s.inline_size, 200);
    EXPECT_EQ(s.block_size, 100);
}

TEST(WritingModeToLogicalTest, VerticalLrSwap) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto s = wm.to_logical(300, 400);
    EXPECT_EQ(s.inline_size, 400);
    EXPECT_EQ(s.block_size, 300);
}

// ============ to_logical_pos ============

TEST(WritingModeToLogicalPosTest, Horizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto pos = wm.to_logical_pos(100, 200, 50, 30);
    EXPECT_EQ(pos.inline_offset, 100);  // x maps to inline
    EXPECT_EQ(pos.block_offset, 200);   // y maps to block
}

TEST(WritingModeToLogicalPosTest, VerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // x=100, width=50 → block_offset = 800 - 100 - 50 = 650
    // y=200 → inline_offset = 200
    auto pos = wm.to_logical_pos(100, 200, 50, 30);
    EXPECT_EQ(pos.inline_offset, 200);
    EXPECT_EQ(pos.block_offset, 650);
}

TEST(WritingModeToLogicalPosTest, VerticalRlAtRightEdge) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    // x=750, width=50 → block_offset = 800 - 750 - 50 = 0
    auto pos = wm.to_logical_pos(750, 0, 50, 30);
    EXPECT_EQ(pos.inline_offset, 0);
    EXPECT_EQ(pos.block_offset, 0);
}

TEST(WritingModeToLogicalPosTest, VerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    // x=100 → block_offset = 100
    // y=200 → inline_offset = 200
    auto pos = wm.to_logical_pos(100, 200, 50, 30);
    EXPECT_EQ(pos.inline_offset, 200);
    EXPECT_EQ(pos.block_offset, 100);
}

TEST(WritingModeToLogicalPosTest, VerticalLrAtRightEdge) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto pos = wm.to_logical_pos(800, 600, 50, 30);
    EXPECT_EQ(pos.inline_offset, 600);
    EXPECT_EQ(pos.block_offset, 800);
}

TEST(WritingModeToLogicalPosTest, SidewaysRlDefaultsToHorizontal) {
    // sideways-rl falls through to default case (horizontal)
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    auto pos = wm.to_logical_pos(100, 200, 50, 30);
    EXPECT_EQ(pos.inline_offset, 100);
    EXPECT_EQ(pos.block_offset, 200);
}

// ============ margin getters ============

// Helper to create margins
static litehtml::margins make_margins(pixel_t l, pixel_t t, pixel_t r, pixel_t b) {
    litehtml::margins m;
    m.left = l;
    m.top = t;
    m.right = r;
    m.bottom = b;
    return m;
}

TEST(WritingModeMarginTest, InlineStartHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto m = make_margins(10, 20, 30, 40);
    EXPECT_EQ(wm.inline_start(m), 10);   // left
    EXPECT_EQ(wm.inline_end(m), 30);     // right
    EXPECT_EQ(wm.block_start(m), 20);    // top
    EXPECT_EQ(wm.block_end(m), 40);      // bottom
}

TEST(WritingModeMarginTest, InlineStartVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto m = make_margins(10, 20, 30, 40);
    EXPECT_EQ(wm.inline_start(m), 20);   // top
    EXPECT_EQ(wm.inline_end(m), 40);     // bottom
    EXPECT_EQ(wm.block_start(m), 30);    // RIGHT (special case for vertical-rl)
    EXPECT_EQ(wm.block_end(m), 10);      // LEFT (special case for vertical-rl)
}

TEST(WritingModeMarginTest, InlineStartVerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto m = make_margins(10, 20, 30, 40);
    EXPECT_EQ(wm.inline_start(m), 20);   // top
    EXPECT_EQ(wm.inline_end(m), 40);     // bottom
    EXPECT_EQ(wm.block_start(m), 10);    // left
    EXPECT_EQ(wm.block_end(m), 30);      // right
}

TEST(WritingModeMarginTest, SidewaysRlMarginMapping) {
    // sideways-rl is NOT vertical for is_vertical(), but block_start/end have a special case
    WritingModeContext wm(writing_mode_sideways_rl, 800, 600);
    auto m = make_margins(10, 20, 30, 40);
    // is_vertical() returns false, so inline_start = left = 10
    EXPECT_EQ(wm.inline_start(m), 10);
    // block_start: is_vertical() is false, m_mode != vertical_rl, so top = 20
    EXPECT_EQ(wm.block_start(m), 20);
}

// ============ margin setters ============

TEST(WritingModeSetMarginTest, SetInlineStartHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_inline_start(m, 50);
    EXPECT_EQ(m.left, 50);   // horizontal: sets left
}

TEST(WritingModeSetMarginTest, SetInlineStartVertical) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_inline_start(m, 50);
    EXPECT_EQ(m.top, 50);    // vertical: sets top
}

TEST(WritingModeSetMarginTest, SetInlineEndHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_inline_end(m, 60);
    EXPECT_EQ(m.right, 60);   // horizontal: sets right
}

TEST(WritingModeSetMarginTest, SetInlineEndVertical) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_inline_end(m, 60);
    EXPECT_EQ(m.bottom, 60);   // vertical: sets bottom
}

TEST(WritingModeSetMarginTest, SetBlockStartHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_block_start(m, 30);
    EXPECT_EQ(m.top, 30);   // horizontal: sets top
}

TEST(WritingModeSetMarginTest, SetBlockStartVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_block_start(m, 30);
    EXPECT_EQ(m.right, 30);  // vertical-rl: sets right
}

TEST(WritingModeSetMarginTest, SetBlockStartVerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_block_start(m, 30);
    EXPECT_EQ(m.left, 30);   // vertical-lr: sets left
}

TEST(WritingModeSetMarginTest, SetBlockEndHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_block_end(m, 40);
    EXPECT_EQ(m.bottom, 40);  // horizontal: sets bottom
}

TEST(WritingModeSetMarginTest, SetBlockEndVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_block_end(m, 40);
    EXPECT_EQ(m.left, 40);    // vertical-rl: sets left
}

TEST(WritingModeSetMarginTest, SetBlockEndVerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    auto m = make_margins(0, 0, 0, 0);
    wm.set_block_end(m, 40);
    EXPECT_EQ(m.right, 40);   // vertical-lr: sets right
}

// ============ css_margin getters ============

TEST(WritingModeCssMarginTest, InlineStartHorizontal) {
    WritingModeContext wm(writing_mode_horizontal_tb, 800, 600);
    // css_margins has left/top/right/bottom as css_length members
    litehtml::css_margins m;
    EXPECT_EQ(&wm.inline_start(m), &m.left);
    EXPECT_EQ(&wm.inline_end(m), &m.right);
    EXPECT_EQ(&wm.block_start(m), &m.top);
    EXPECT_EQ(&wm.block_end(m), &m.bottom);
}

TEST(WritingModeCssMarginTest, InlineStartVerticalRl) {
    WritingModeContext wm(writing_mode_vertical_rl, 800, 600);
    litehtml::css_margins m;
    EXPECT_EQ(&wm.inline_start(m), &m.top);
    EXPECT_EQ(&wm.inline_end(m), &m.bottom);
    EXPECT_EQ(&wm.block_start(m), &m.right);  // vertical-rl special case
    EXPECT_EQ(&wm.block_end(m), &m.left);     // vertical-rl special case
}

TEST(WritingModeCssMarginTest, InlineStartVerticalLr) {
    WritingModeContext wm(writing_mode_vertical_lr, 800, 600);
    litehtml::css_margins m;
    EXPECT_EQ(&wm.inline_start(m), &m.top);
    EXPECT_EQ(&wm.inline_end(m), &m.bottom);
    EXPECT_EQ(&wm.block_start(m), &m.left);
    EXPECT_EQ(&wm.block_end(m), &m.right);
}
