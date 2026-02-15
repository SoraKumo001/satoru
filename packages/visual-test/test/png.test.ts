import { describe, it, expect, beforeAll, afterAll } from "vitest";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { createSatoruWorker, render } from "satoru";
import { PNG } from "pngjs";
import { compareImages, softExpect } from "../src/utils";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");
const REFERENCE_DIR = path.resolve(__dirname, "../reference");
const DIFF_DIR = path.resolve(__dirname, "../diff");
const TEMP_DIR = path.resolve(__dirname, "../temp");
const BASELINE_PATH = path.join(__dirname, "data/png-mismatch-baselines.json");

const FONT_MAP: {
  name: string;
  url: string;
  path: string;
}[] = [];

describe("PNG (Skia) Visual Tests", () => {
  let satoru: ReturnType<typeof createSatoruWorker>;
  let baselines: any = {};

  beforeAll(async () => {
    [DIFF_DIR, TEMP_DIR].forEach(
      (dir) => !fs.existsSync(dir) && fs.mkdirSync(dir, { recursive: true }),
    );
    satoru = createSatoruWorker({});
    if (fs.existsSync(BASELINE_PATH)) {
      baselines = JSON.parse(fs.readFileSync(BASELINE_PATH, "utf8"));
    }
  });

  afterAll(() => {
    if (process.env.UPDATE_SNAPSHOTS) {
      const current = fs.existsSync(BASELINE_PATH)
        ? JSON.parse(fs.readFileSync(BASELINE_PATH, "utf8"))
        : {};

      for (const file in baselines) {
        current[file] = baselines[file];
      }

      const dir = path.dirname(BASELINE_PATH);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }

      fs.writeFileSync(BASELINE_PATH, JSON.stringify(current, null, 2));
    }
  });

  const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

  files.forEach((file) => {
    it(`PNG: ${file}`, async () => {
      const refPath = path.join(REFERENCE_DIR, file.replace(".html", ".png"));
      const html = fs.readFileSync(path.join(ASSETS_DIR, file), "utf8");

      const pngData = (await render({
        value: html,
        width: 800,
        format: "png",
        css: "body { margin: 8px; }",
      })) as Uint8Array;

      fs.writeFileSync(
        path.join(TEMP_DIR, file.replace(".html", ".png")),
        Buffer.from(pngData),
      );

      const currentImg = PNG.sync.read(Buffer.from(pngData));
      let refImg: PNG;

      if (fs.existsSync(refPath)) {
        refImg = PNG.sync.read(fs.readFileSync(refPath));
      } else {
        throw new Error(
          `Reference image missing for ${file}. Please run 'pnpm run gen-ref' first.`,
        );
      }

      const result = compareImages(
        refImg,
        currentImg,
        path.join(DIFF_DIR, `png-${file.replace(".html", "")}`),
      );

      console.log(
        `${file} (Png): Fill: ${result.fill.toFixed(2)}%, Outline: ${result.outline.toFixed(2)}%`,
      );

      const baseline = baselines[file];
      if (!baseline || process.env.UPDATE_SNAPSHOTS) {
        baselines[file] = result;
      } else {
        const factor = process.env.GITHUB_ACTIONS ? 20.0 : 1.0;
        const minTolerance = process.env.GITHUB_ACTIONS ? 15.0 : 0.01;
        softExpect(
          result.outline,
          `Outline diff for ${file} increased`,
          (v) => {
            expect(v).toBeLessThanOrEqual(
              Math.max(baseline.outline, minTolerance) * factor,
            );
          },
        );

        if (result.fill < baseline.fill) baseline.fill = result.fill;
        if (result.outline < baseline.outline)
          baseline.outline = result.outline;
      }

      softExpect(
        result.outline,
        `Outline diff for ${file} is too high`,
        (v) => {
          expect(v).toBeLessThan(15);
        },
      );
      softExpect(result.fill, `Fill diff for ${file} is too high`, (v) => {
        expect(v).toBeLessThan(30);
      });
    });
  });
});
