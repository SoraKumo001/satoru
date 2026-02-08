import { describe, it, expect, beforeAll } from "vitest";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru } from "satoru";
import pixelmatch from "pixelmatch";
import { PNG } from "pngjs";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");
const OUTPUT_DIR = path.resolve(__dirname, "../output");
const REFERENCE_DIR = path.resolve(__dirname, "../reference");
const DIFF_DIR = path.resolve(__dirname, "../diff");
const TEMP_DIR = path.resolve(ROOT_DIR, "temp");

const ROBOTO_400 =
  "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2";

const FONT_MAP = [
  {
    name: "Roboto",
    url: ROBOTO_400,
    path: path.join(TEMP_DIR, "Roboto-400.woff2"),
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

describe("Visual Regression Tests", () => {
  let satoru: Satoru;

  beforeAll(async () => {
    [OUTPUT_DIR, DIFF_DIR, TEMP_DIR].forEach((dir) => {
      if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
    });

    for (const font of FONT_MAP) {
      await downloadFont(font.url, font.path);
    }

    const wasmPath = path.resolve(ROOT_DIR, "packages/satoru/dist/satoru.wasm");
    satoru = await Satoru.init(undefined, {
      locateFile: (url: string) => (url.endsWith(".wasm") ? wasmPath : url),
    });

    for (const font of FONT_MAP) {
      if (fs.existsSync(font.path)) {
        const fontData = fs.readFileSync(font.path);
        satoru.loadFont(font.name, new Uint8Array(fontData));
      }
    }
  });

  const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

  files.forEach((file) => {
    it(`should render ${file} correctly`, async () => {
      const inputPath = path.join(ASSETS_DIR, file);
      const html = fs.readFileSync(inputPath, "utf8");

      satoru.clearImages();
      const dataUrls = new Set<string>();
      const imgRegex = /<img[^>]+src=["'](data:image\/[^'"]+)["']/g;
      const bgRegex =
        /background-image:\s*url\(['"]?(data:image\/[^'"]+)['"]?\)/g;

      let match;
      while ((match = imgRegex.exec(html)) !== null) dataUrls.add(match[1]);
      while ((match = bgRegex.exec(html)) !== null) dataUrls.add(match[1]);

      for (const url of dataUrls) {
        satoru.loadImage(url, url, 0, 0);
      }

      // Render PNG
      const pngBuffer = satoru.toPngBinary(html, 800);
      expect(pngBuffer).not.toBeNull();
      if (!pngBuffer) return;

      const outputPath = path.join(OUTPUT_DIR, file.replace(".html", ".png"));
      fs.writeFileSync(outputPath, Buffer.from(pngBuffer));

      // Compare with reference if exists
      const refPath = path.join(REFERENCE_DIR, file.replace(".html", ".png"));
      if (fs.existsSync(refPath)) {
        const img1 = PNG.sync.read(fs.readFileSync(refPath));
        const img2 = PNG.sync.read(Buffer.from(pngBuffer));

        let finalImg1 = img1;
        let finalImg2 = img2;

        if (img1.width !== img2.width || img1.height !== img2.height) {
          const maxWidth = Math.max(img1.width, img2.width);
          const maxHeight = Math.max(img1.height, img2.height);

          console.warn(
            `Dimension mismatch for ${file}: Ref(${img1.width}x${img1.height}) vs Satoru(${img2.width}x${img2.height}). Padding to ${maxWidth}x${maxHeight}.`,
          );

          const pad = (img: any, w: number, h: number) => {
            if (img.width === w && img.height === h) return img;
            const newImg = new PNG({ width: w, height: h });
            newImg.data.fill(0);
            for (let y = 0; y < img.height; y++) {
              for (let x = 0; x < img.width; x++) {
                const srcIdx = (img.width * y + x) << 2;
                const dstIdx = (w * y + x) << 2;
                newImg.data[dstIdx] = img.data[srcIdx];
                newImg.data[dstIdx + 1] = img.data[srcIdx + 1];
                newImg.data[dstIdx + 2] = img.data[srcIdx + 2];
                newImg.data[dstIdx + 3] = img.data[srcIdx + 3];
              }
            }
            return newImg;
          };

          finalImg1 = pad(img1, maxWidth, maxHeight);
          finalImg2 = pad(img2, maxWidth, maxHeight);
        }

        const { width, height } = finalImg1;
        const diff = new PNG({ width, height });

        const numDiffPixels = pixelmatch(
          finalImg1.data,
          finalImg2.data,
          diff.data,
          width,
          height,
          { threshold: 0.1 },
        );

        if (numDiffPixels > 0) {
          fs.writeFileSync(
            path.join(DIFF_DIR, file.replace(".html", ".png")),
            PNG.sync.write(diff),
          );
        }

        const diffPercentage = (numDiffPixels / (width * height)) * 100;
        expect(
          diffPercentage,
          `Too many differing pixels in ${file}: ${numDiffPixels} pixels (${diffPercentage.toFixed(2)}%)`,
        ).toBeLessThan(10);
      } else {
        console.warn(
          `Reference image not found for ${file}. Run 'pnpm gen-ref' first.`,
        );
      }
      const svg = satoru.toSvg(html, 800);
      expect(svg).toMatchSnapshot();
    });
  });
});
