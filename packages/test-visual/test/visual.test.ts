import { describe, it, expect, beforeAll, afterAll, beforeEach } from "vitest";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru, RequiredResource } from "satoru";
import pixelmatch from "pixelmatch";
import { PNG } from "pngjs";
import { chromium, Browser, Page } from "playwright";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");
const REFERENCE_DIR = path.resolve(__dirname, "../reference");
const DIFF_DIR = path.resolve(__dirname, "../diff");
const TEMP_DIR = path.resolve(ROOT_DIR, "../temp");

const BASELINE_PATH = path.join(__dirname, "mismatch-baselines.json");
const baselines: Record<string, { direct: number; svg: number }> =
  fs.existsSync(BASELINE_PATH)
    ? JSON.parse(fs.readFileSync(BASELINE_PATH, "utf8"))
    : {};

const FONT_MAP: {
  name: string;
  url: string;
  path: string;
}[] = [
  {
    name: "Roboto",
    url: "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2",
    path: path.join(TEMP_DIR, "Roboto-400.woff2"),
  },
  {
    name: "Noto Sans JP",
    url: "https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-400-normal.woff2",
    path: path.join(TEMP_DIR, "NotoSansJP-400.woff2"),
  },
];

async function downloadFont(url: string, dest: string) {
  if (fs.existsSync(dest)) return;
  const response = await fetch(url);
  if (!response.ok)
    throw new Error(`Failed to download font: ${response.statusText}`);
  const arrayBuffer = await response.arrayBuffer();
  fs.writeFileSync(dest, Buffer.from(arrayBuffer));
}

function flattenAlpha(img: PNG) {
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

function padImage(img: PNG, w: number, h: number): PNG {
  if (img.width === w && img.height === h) return img;
  const newImg = new PNG({ width: w, height: h });
  for (let i = 0; i < newImg.data.length; i += 4) {
    newImg.data[i] = 255;
    newImg.data[i + 1] = 255;
    newImg.data[i + 2] = 255;
    newImg.data[i + 3] = 255;
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

function compareImages(img1: PNG, img2: PNG, diffPath: string): number {
  const maxWidth = Math.max(img1.width, img2.width);
  const maxHeight = Math.max(img1.height, img2.height);

  const final1 = flattenAlpha(padImage(img1, maxWidth, maxHeight));
  const final2 = flattenAlpha(padImage(img2, maxWidth, maxHeight));

  const diff = new PNG({ width: maxWidth, height: maxHeight });
  const numDiffPixels = pixelmatch(
    final1.data,
    final2.data,
    diff.data,
    maxWidth,
    maxHeight,
    { threshold: 0.1 },
  );

  if (numDiffPixels > 0) {
    fs.writeFileSync(diffPath, PNG.sync.write(diff));
  }
  return (numDiffPixels / (maxWidth * maxHeight)) * 100;
}

describe("Visual Regression Tests", () => {
  let satoru: Satoru;
  let browser: Browser;
  let page: Page;

  beforeAll(async () => {
    [DIFF_DIR, TEMP_DIR].forEach(
      (dir) => !fs.existsSync(dir) && fs.mkdirSync(dir, { recursive: true }),
    );
    const wasmPath = path.resolve(ROOT_DIR, "packages/satoru/dist/satoru.wasm");
    satoru = await Satoru.init(undefined, {
      locateFile: (url: string) => (url.endsWith(".wasm") ? wasmPath : url),
    });
    browser = await chromium.launch();
    page = await browser.newPage();
  });

  beforeEach(() => {
    satoru.clearFonts();
    satoru.clearImages();
  });

  afterAll(async () => {
    if (browser) await browser.close();
    if (process.env.UPDATE_SNAPSHOTS || !fs.existsSync(BASELINE_PATH)) {
      fs.writeFileSync(BASELINE_PATH, JSON.stringify(baselines, null, 2));
      console.log(`Updated mismatch baselines in ${BASELINE_PATH}`);
    }
  });

  const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

  files.forEach((file) => {
    describe(`${file}`, () => {
      const inputPath = path.join(ASSETS_DIR, file);
      const refPath = path.join(REFERENCE_DIR, file.replace(".html", ".png"));
      const html = fs.readFileSync(inputPath, "utf8");

      it(`Rendering and Comparison`, async () => {
        if (!fs.existsSync(refPath)) {
          console.warn(`Reference not found for ${file}`);
          return;
        }
        const refImg = PNG.sync.read(fs.readFileSync(refPath));

        const resolveResource = async (r: RequiredResource) => {
          if (r.type === "font") {
            const font = FONT_MAP.find(
              (f) => f.url === r.url || f.name === r.name,
            );
            if (font) {
              await downloadFont(font.url, font.path);
              return new Uint8Array(fs.readFileSync(font.path));
            }
          }
          return null;
        };

        // 1. Direct PNG (Skia)
        const directPngData = (await satoru.render(html, 800, {
          format: "png",
          resolveResource,
        })) as Uint8Array;
        const directDiff = compareImages(
          refImg,
          PNG.sync.read(Buffer.from(directPngData!)),
          path.join(DIFF_DIR, `direct-${file.replace(".html", ".png")}`),
        );

        // 2. SVG -> Browser PNG
        const svg = (await satoru.render(html, 800, {
          format: "svg",
          resolveResource,
        })) as string;
        const widthMatch = svg.match(/width="(\d+)"/);
        const heightMatch = svg.match(/height="(\d+)"/);
        const svgWidth = widthMatch ? parseInt(widthMatch[1], 10) : 800;
        const svgHeight = heightMatch ? parseInt(heightMatch[1], 10) : 1000;

        await page.setViewportSize({ width: svgWidth, height: svgHeight });
        await page.setContent(
          `<style>body { margin: 0; padding: 0; overflow: hidden; }</style>${svg}`,
        );
        const svgPngBuffer = await page.screenshot({ omitBackground: true });
        const svgDiff = compareImages(
          refImg,
          PNG.sync.read(svgPngBuffer),
          path.join(DIFF_DIR, `svg-${file.replace(".html", ".png")}`),
        );

        // Log results
        console.log(
          `${file}: [Direct PNG Diff: ${directDiff.toFixed(2)}%] [SVG PNG Diff: ${svgDiff.toFixed(2)}%]`,
        );

        const threshold =
          file.includes("gradients") || file.includes("09-complex") ? 30 : 10;

        const baseline = baselines[file];
        if (!baseline || process.env.UPDATE_SNAPSHOTS) {
          baselines[file] = { direct: directDiff, svg: svgDiff };
        } else {
          // Use a multiplier in CI environment to account for OS-level rendering differences
          const factor = process.env.GITHUB_ACTIONS ? 2.0 : 1.05; // 100% allowance in CI, 5% locally
          expect(
            directDiff,
            `Direct PNG diff increased: ${directDiff.toFixed(2)}% (baseline: ${baseline.direct.toFixed(2)}%)`,
          ).toBeLessThanOrEqual(Math.max(baseline.direct, 0.01) * factor);
          expect(
            svgDiff,
            `SVG PNG diff increased: ${svgDiff.toFixed(2)}% (baseline: ${baseline.svg.toFixed(2)}%)`,
          ).toBeLessThanOrEqual(Math.max(baseline.svg, 0.01) * factor);

          if (directDiff < baseline.direct) baseline.direct = directDiff;
          if (svgDiff < baseline.svg) baseline.svg = svgDiff;
        }

        expect(
          directDiff,
          `Direct PNG diff too high: ${directDiff.toFixed(2)}%`,
        ).toBeLessThan(threshold);
        expect(
          svgDiff,
          `SVG PNG diff too high: ${svgDiff.toFixed(2)}%`,
        ).toBeLessThan(threshold);
      });
    });
  });
});