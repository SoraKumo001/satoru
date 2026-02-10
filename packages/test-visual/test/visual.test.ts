import { describe, it, expect, beforeAll, afterAll, beforeEach } from "vitest";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru, RequiredResource } from "satoru";
import { PNG } from "pngjs";
import { chromium, Browser, Page } from "playwright";
import { downloadFont, compareImages, ComparisonMetrics } from "../src/utils";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");
const REFERENCE_DIR = path.resolve(__dirname, "../reference");
const DIFF_DIR = path.resolve(__dirname, "../diff");
const TEMP_DIR = path.resolve(ROOT_DIR, "../temp");

const BASELINE_PATH = path.join(__dirname, "data/mismatch-baselines.json");
const baselines: Record<
  string,
  { direct: ComparisonMetrics; svg: ComparisonMetrics }
> = fs.existsSync(BASELINE_PATH)
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
      const dataDir = path.dirname(BASELINE_PATH);
      if (!fs.existsSync(dataDir)) fs.mkdirSync(dataDir, { recursive: true });
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
        const directResult = compareImages(
          refImg,
          PNG.sync.read(Buffer.from(directPngData!)),
          path.join(DIFF_DIR, `direct-${file.replace(".html", "")}`),
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
        const svgResult = compareImages(
          refImg,
          PNG.sync.read(svgPngBuffer),
          path.join(DIFF_DIR, `svg-${file.replace(".html", "")}`),
        );

        // Log results
        console.log(
          `${file}: Direct[Fill: ${directResult.fill.toFixed(2)}%, Outline: ${directResult.outline.toFixed(2)}%] ` +
            `SVG[Fill: ${svgResult.fill.toFixed(2)}%, Outline: ${svgResult.outline.toFixed(2)}%]`,
        );

        const fillThreshold = 30;
        const outlineThreshold = 15;

        const baseline = baselines[file];
        if (!baseline || process.env.UPDATE_SNAPSHOTS) {
          baselines[file] = { direct: directResult, svg: svgResult };
        } else {
          const factor = process.env.GITHUB_ACTIONS ? 2.0 : 1.0;

          // Validate Direct
          expect(
            directResult.outline,
            `Direct Outline diff increased (baseline: ${baseline.direct.outline.toFixed(2)}%)`,
          ).toBeLessThanOrEqual(
            Math.max(baseline.direct.outline, 0.01) * factor,
          );

          // Validate SVG
          expect(
            svgResult.outline,
            `SVG Outline diff increased (baseline: ${baseline.svg.outline.toFixed(2)}%)`,
          ).toBeLessThanOrEqual(Math.max(baseline.svg.outline, 0.01) * factor);

          // Update baseline if improved
          if (directResult.fill < baseline.direct.fill)
            baseline.direct.fill = directResult.fill;
          if (directResult.outline < baseline.direct.outline)
            baseline.direct.outline = directResult.outline;
          if (svgResult.fill < baseline.svg.fill)
            baseline.svg.fill = svgResult.fill;
          if (svgResult.outline < baseline.svg.outline)
            baseline.svg.outline = svgResult.outline;
        }

        expect(
          directResult.outline,
          `Direct Outline diff too high`,
        ).toBeLessThan(outlineThreshold);
        expect(svgResult.outline, `SVG Outline diff too high`).toBeLessThan(
          outlineThreshold,
        );
        expect(directResult.fill, `Direct Fill diff too high`).toBeLessThan(
          fillThreshold,
        );
        expect(svgResult.fill, `SVG Fill diff too high`).toBeLessThan(
          fillThreshold,
        );
      }, 30000);
    });
  });
});
