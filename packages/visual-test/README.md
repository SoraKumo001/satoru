# Visual Regression Test Suite

This package provides a robust visual regression testing environment for Satoru, comparing its output against reference images and browser rendering.

## Testing Architecture

The suite performs two types of visual validation:

1.  **PNG (Skia) vs. Reference**: Compares the direct PNG output from Satoru (Skia backend) against pre-generated reference images.
2.  **SVG (Satoru) vs. Browser (SVG)**: Renders HTML to SVG via Satoru, then renders that SVG in a headless browser (Playwright/Chromium) and takes a screenshot to compare against the same reference images.

## Commands

### `pnpm test`
Runs the visual tests using Vitest. It checks if the current rendering results match the baselines defined in `test/data/mismatch-baselines.json`.

### `pnpm test:update`
Updates both the **reference images** and the **baseline metrics**.
-   **Reference Images**: The `.png` files in the `reference/` directory are overwritten with the latest rendering results.
-   **Baseline Metrics**: The `test/data/mismatch-baselines.json` file is updated with the latest difference percentages (Fill/Outline).
-   **Comparison**: When updating, it compares the new result with the *previous* reference image and logs the amount of change before overwriting.

### `pnpm gen-ref`
Generates initial reference images using a browser-based rendering as the "ground truth".

### `pnpm convert-assets`
A utility to batch-convert all HTML assets in the `assets/` directory into PNG/SVG/PDF files in the `temp/` directory for manual inspection.

## Key Files

-   `reference/`: Contains the "correct" PNG images for each asset.
-   `test/data/mismatch-baselines.json`: Stores the allowed difference thresholds for each test case.
-   `diff/`: (Generated) Contains visual delta images showing where rendering differs from the reference.
-   `src/utils.ts`: Core image comparison logic using `pixelmatch`.
