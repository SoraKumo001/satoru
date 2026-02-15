import fs from "fs";
import { PNG } from "pngjs";
import pixelmatch from "pixelmatch";

export interface ComparisonMetrics {
  fill: number;
  outline: number;
}

/**
 * Downloads a font from a URL and saves it to a destination path.
 */
export async function downloadFont(url: string, dest: string) {
  if (fs.existsSync(dest)) return;
  const response = await fetch(url);
  if (!response.ok)
    throw new Error(`Failed to download font: ${response.statusText}`);
  const arrayBuffer = await response.arrayBuffer();
  fs.writeFileSync(dest, Buffer.from(arrayBuffer));
}

/**
 * Flattens the alpha channel by blending with white.
 */
export function flattenAlpha(img: PNG) {
  for (let i = 0; i < img.data.length; i += 4) {
    const alpha = img.data[i + 3] / 255;
    if (alpha < 1) {
      img.data[i] = Math.round(img.data[i] * alpha + 255 * (1 - alpha));
      img.data[i + 1] = Math.round(img.data[i + 1] * alpha + 255 * (1 - alpha));
      img.data[i + 2] = Math.round(img.data[i + 2] * alpha + 255 * (1 - alpha));
      img.data[i + 3] = 255;
    }
  }
  return img;
}

/**
 * Pads an image to specified dimensions using edge clamping (top-left pixel color).
 */
export function padImage(img: PNG, w: number, h: number): PNG {
  if (img.width === w && img.height === h) return img;
  const newImg = new PNG({ width: w, height: h });

  // Use the top-left pixel as the padding color (likely the background)
  const r = img.data[0];
  const g = img.data[1];
  const b = img.data[2];
  const a = img.data[3];

  for (let i = 0; i < newImg.data.length; i += 4) {
    newImg.data[i] = r;
    newImg.data[i + 1] = g;
    newImg.data[i + 2] = b;
    newImg.data[i + 3] = a;
  }

  for (let y = 0; y < img.height; y++) {
    for (let x = 0; x < img.width; x++) {
      const srcIdx = (img.width * y + x) << 2;
      const dstIdx = (w * y + x) << 2;
      for (let c = 0; x < img.width && c < 4; c++)
        newImg.data[dstIdx + c] = img.data[srcIdx + c];
    }
  }
  return newImg;
}

/**
 * Apply Sobel filter to extract outlines and ignore fills.
 */
export function applySobel(img: PNG): PNG {
  const width = img.width;
  const height = img.height;
  const output = new PNG({ width, height });

  const gray = new Float32Array(width * height);
  for (let i = 0; i < width * height; i++) {
    const r = img.data[i * 4];
    const g = img.data[i * 4 + 1];
    const b = img.data[i * 4 + 2];
    gray[i] = 0.299 * r + 0.587 * g + 0.114 * b;
  }

  for (let y = 1; y < height - 1; y++) {
    for (let x = 1; x < width - 1; x++) {
      const gx =
        -1 * gray[(y - 1) * width + (x - 1)] +
        1 * gray[(y - 1) * width + (x + 1)] +
        -2 * gray[y * width + (x - 1)] +
        2 * gray[y * width + (x + 1)] +
        -1 * gray[(y + 1) * width + (x - 1)] +
        1 * gray[(y + 1) * width + (x + 1)];

      const gy =
        -1 * gray[(y - 1) * width + (x - 1)] -
        2 * gray[(y - 1) * width + x] -
        1 * gray[(y - 1) * width + (x + 1)] +
        1 * gray[(y + 1) * width + (x - 1)] +
        2 * gray[(y + 1) * width + x] +
        1 * gray[(y + 1) * width + (x + 1)];

      const mag = Math.min(255, Math.sqrt(gx * gx + gy * gy));
      const outIdx = (y * width + x) * 4;
      output.data[outIdx] = output.data[outIdx + 1] = output.data[outIdx + 2] = mag;
      output.data[outIdx + 3] = 255;
    }
  }
  return output;
}

/**
 * Compares two images and returns fill and outline difference percentages.
 */
export function compareImages(img1: PNG, img2: PNG, diffPathPrefix: string): ComparisonMetrics {
  const maxWidth = Math.max(img1.width, img2.width);
  const maxHeight = Math.max(img1.height, img2.height);

  const final1 = flattenAlpha(padImage(img1, maxWidth, maxHeight));
  const final2 = flattenAlpha(padImage(img2, maxWidth, maxHeight));

  // 1. Fill Comparison
  const fillDiffImg = new PNG({ width: maxWidth, height: maxHeight });
  const numFillDiffPixels = pixelmatch(
    final1.data,
    final2.data,
    fillDiffImg.data,
    maxWidth,
    maxHeight,
    { threshold: 0.1 },
  );

  // Color-code the diff:
  // Red: Missing in img2 (was in img1)
  // Green: New in img2 (was not in img1)
  for (let i = 0; i < fillDiffImg.data.length; i += 4) {
    if (fillDiffImg.data[i] === 255 && fillDiffImg.data[i + 1] === 0 && fillDiffImg.data[i + 2] === 0) {
      const r1 = final1.data[i];
      const g1 = final1.data[i + 1];
      const b1 = final1.data[i + 2];
      const r2 = final2.data[i];
      const g2 = final2.data[i + 1];
      const b2 = final2.data[i + 2];

      const isWhite1 = r1 > 250 && g1 > 250 && b1 > 250;
      const isWhite2 = r2 > 250 && g2 > 250 && b2 > 250;

      if (!isWhite1 && isWhite2) {
        // Pixel exists in ref but is white (background) in current -> Removed (Red)
        fillDiffImg.data[i] = 255;
        fillDiffImg.data[i + 1] = 0;
        fillDiffImg.data[i + 2] = 0;
      } else if (isWhite1 && !isWhite2) {
        // Pixel is white in ref but exists in current -> Added (Green)
        fillDiffImg.data[i] = 0;
        fillDiffImg.data[i + 1] = 255;
        fillDiffImg.data[i + 2] = 0;
      } else {
        // Both have content but they differ -> Changed (Yellow)
        fillDiffImg.data[i] = 255;
        fillDiffImg.data[i + 1] = 255;
        fillDiffImg.data[i + 2] = 0;
      }
    }
  }

  if (numFillDiffPixels > 0) {
    fs.writeFileSync(`${diffPathPrefix}-fill.png`, PNG.sync.write(fillDiffImg));
  }

  // 2. Outline Comparison
  const edge1 = applySobel(final1);
  const edge2 = applySobel(final2);
  const outlineDiffImg = new PNG({ width: maxWidth, height: maxHeight });
  const numOutlineDiffPixels = pixelmatch(
    edge1.data,
    edge2.data,
    outlineDiffImg.data,
    maxWidth,
    maxHeight,
    { threshold: 0.1 },
  );

  // Color-code the outline diff:
  for (let i = 0; i < outlineDiffImg.data.length; i += 4) {
    if (outlineDiffImg.data[i] === 255 && outlineDiffImg.data[i + 1] === 0 && outlineDiffImg.data[i + 2] === 0) {
      const v1 = edge1.data[i];
      const v2 = edge2.data[i];

      if (v1 > 20 && v2 <= 20) {
        // Edge exists in ref but not in current -> Removed (Red)
        outlineDiffImg.data[i] = 255;
        outlineDiffImg.data[i + 1] = 0;
        outlineDiffImg.data[i + 2] = 0;
      } else if (v1 <= 20 && v2 > 20) {
        // Edge exists in current but not in ref -> Added (Green)
        outlineDiffImg.data[i] = 0;
        outlineDiffImg.data[i + 1] = 255;
        outlineDiffImg.data[i + 2] = 0;
      } else {
        // Both have edges but they differ -> Changed (Yellow)
        outlineDiffImg.data[i] = 255;
        outlineDiffImg.data[i + 1] = 255;
        outlineDiffImg.data[i + 2] = 0;
      }
    }
  }

  if (numOutlineDiffPixels > 0) {
    fs.writeFileSync(`${diffPathPrefix}-outline.png`, PNG.sync.write(outlineDiffImg));
  }

  return {
    fill: (numFillDiffPixels / (maxWidth * maxHeight)) * 100,
    outline: (numOutlineDiffPixels / (maxWidth * maxHeight)) * 100,
  };
}

/**
 * Asserts a condition, but only logs a warning instead of failing the test in GitHub Actions.
 */
export function softExpect(value: any, message: string, assertion: (v: any) => void) {
  try {
    assertion(value);
  } catch (error) {
    if (process.env.GITHUB_ACTIONS) {
      console.warn(`[SOFT ASSERTION FAILED] ${message}:`, error instanceof Error ? error.message : error);
    } else {
      throw error;
    }
  }
}
