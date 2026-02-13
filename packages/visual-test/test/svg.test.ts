import { describe, it, expect, beforeAll, beforeEach, afterAll } from "vitest";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru, RequiredResource } from "satoru";
import { PNG } from "pngjs";
import { chromium, Browser, Page } from "playwright";
import { downloadFont, compareImages } from "../src/utils";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");
const REFERENCE_DIR = path.resolve(__dirname, "../reference");
const DIFF_DIR = path.resolve(__dirname, "../diff");
const TEMP_DIR = path.resolve(ROOT_DIR, "../temp");
const BASELINE_PATH = path.join(__dirname, "data/mismatch-baselines.json");

const FONT_MAP: {
  name: string;
  url: string;
  path: string;
}[] = [];

describe("SVG (Browser) Visual Tests", () => {
  let satoru: Satoru;
  let browser: Browser;
  let page: Page;
  let baselines: any = {};

  beforeAll(async () => {
    [DIFF_DIR, TEMP_DIR].forEach(
      (dir) => !fs.existsSync(dir) && fs.mkdirSync(dir, { recursive: true }),
    );
    satoru = await Satoru.init();
    browser = await chromium.launch();
    page = await browser.newPage();
    if (fs.existsSync(BASELINE_PATH)) {
      baselines = JSON.parse(fs.readFileSync(BASELINE_PATH, "utf8"));
    }
  });

  beforeEach(() => {
    satoru.clearFonts();
    satoru.clearImages();
  });

  afterAll(async () => {
    if (browser) await browser.close();
    if (process.env.UPDATE_SNAPSHOTS) {
      const current = fs.existsSync(BASELINE_PATH)
        ? JSON.parse(fs.readFileSync(BASELINE_PATH, "utf8"))
        : {};

      for (const file in baselines) {
        if (!current[file]) current[file] = {};
        Object.assign(current[file], baselines[file]);
      }

      fs.writeFileSync(BASELINE_PATH, JSON.stringify(current, null, 2));
    }
  });

  const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

  files.forEach((file) => {
    it(`SVG: ${file}`, async () => {
      const refPath = path.join(REFERENCE_DIR, file.replace(".html", ".png"));
      const html = fs.readFileSync(path.join(ASSETS_DIR, file), "utf8");

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

      const svg = (await satoru.render({
        value: html,
        width: 800,
        format: "svg",
        clear: true,
        css: "body { margin: 8px; }",
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

      const currentImg = PNG.sync.read(svgPngBuffer);
      let refImg: PNG;

      if (fs.existsSync(refPath)) {
        refImg = PNG.sync.read(fs.readFileSync(refPath));
      } else {
        refImg = currentImg;
        console.log(`Generated new reference for ${file} (from SVG)`);
        fs.writeFileSync(refPath, svgPngBuffer);
      }

      const result = compareImages(
        refImg,
        currentImg,
        path.join(DIFF_DIR, `svg-${file.replace(".html", "")}`),
      );

      if (process.env.UPDATE_SNAPSHOTS && fs.existsSync(refPath)) {
        fs.writeFileSync(refPath, svgPngBuffer);
      }

      console.log(
        `${file} (SVG): Fill: ${result.fill.toFixed(2)}%, Outline: ${result.outline.toFixed(2)}%`,
      );

      const baseline = baselines[file]?.svg;
      if (!baseline || process.env.UPDATE_SNAPSHOTS) {
        if (!baselines[file]) baselines[file] = {};
        baselines[file].svg = result;
      } else {
        const factor = process.env.GITHUB_ACTIONS ? 2.0 : 1.0;
        expect(result.outline, `Outline diff increased`).toBeLessThanOrEqual(
          Math.max(baseline.outline, 0.01) * factor,
        );
        if (result.fill < baseline.fill) baseline.fill = result.fill;
        if (result.outline < baseline.outline)
          baseline.outline = result.outline;
      }

      expect(result.outline).toBeLessThan(25);
      expect(result.fill).toBeLessThan(45);
    });
  });
});
