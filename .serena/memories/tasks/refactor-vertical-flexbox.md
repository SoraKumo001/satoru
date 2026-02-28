# Refactor Flexbox for Vertical Writing Mode Compatibility

## Problem
In `vertical-rl/lr` writing modes, the Flexbox Main Axis maps to the physical Y-axis (inline) for `row` direction and the physical X-axis (block) for `column` direction. Currently, `litehtml` fixedly maps `row` to X and `column` to Y, causing layout corruption in vertical modes. Fixing it for vertical breaks horizontal layout because they both rely on the same physical code paths.

## Proposed Solution: Full Logical Property Abstraction
Instead of physical coordinates (top, left, width, height), refactor the layout engine to use logical properties (inline-start, block-start, inline-size, block-size).

### 1. Abstract Axis Mapping
Implement a mapping layer to resolve `Main Axis` and `Cross Axis` into `Inline` or `Block` based on `flex-direction`.
- `row` / `row-reverse` -> Main: `Inline`, Cross: `Block`
- `column` / `column-reverse` -> Main: `Block`, Cross: `Inline`

### 2. Refactor `flex_item` Logic
Eliminate all direct physical property access in `src/cpp/libs/litehtml/src/flex_item.cpp`.
- Replace `el->width()` / `el->height()` with `el->inline_size()` / `el->block_size()`.
- Replace `el->pos().x` / `el->pos().y` with logical positioning calls.
- Use `render_item::place_logical(inline_pos, block_pos, ...)` for final placement.

### 3. Orthogonal Flow Support
Ensure that `containing_block_context` correctly passes `inline_size` constraints.
Address cases where a vertical flex container is placed inside a horizontal parent (and vice versa).

## Implementation Steps
1. **Strengthen `logical_geometry.h`**: Add helpers to convert `main`/`cross` coordinates to `inline`/`block`.
2. **Refactor `flex_item` Accessors**: Standardize on `get_main_size()`, `get_cross_size()`, etc., using logical properties internally.
3. **Logic Clean-up**: Remove `flex_item_row_direction` and `flex_item_column_direction` sub-classes if they become redundant after abstraction, or refactor them to be "Main Axis is Inline" and "Main Axis is Block" respectively.
4. **Validation**: Use `17-vertical-writing.html` with Flexbox and ensure existing visual tests for horizontal layouts still pass.
