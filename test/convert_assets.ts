import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { createRequire } from "module";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);

const ASSETS_DIR = path.resolve(__dirname, "../public/assets");
const TEMP_DIR = path.resolve(__dirname, "../temp");
const WASM_JS_PATH = path.resolve(__dirname, "../public/satoru.js");
const WASM_BINARY_PATH = path.resolve(__dirname, "../public/satoru.wasm");

// Font paths
const ROBOTO_PATH = path.resolve(
  __dirname,
  "../external/skia/resources/fonts/Roboto-Regular.ttf",
);

// Using the same fonts as main.ts
const ROBOTO_400 = "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2";
const NOTO_JP_400 = "https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-400-normal.woff2";

const FONT_MAP = [
  { name: "Roboto", url: ROBOTO_400, path: path.join(TEMP_DIR, "Roboto-400.woff2") },
  { name: "Noto Sans JP", url: NOTO_JP_400, path: path.join(TEMP_DIR, "NotoSansJP-400.woff2") },
  { name: "sans-serif", url: NOTO_JP_400, path: path.join(TEMP_DIR, "NotoSansJP-400.woff2") },
];

async function downloadFont(url: string, dest: string) {
  if (fs.existsSync(dest)) return;
  console.log(`Downloading font from ${url}...`);
  const response = await fetch(url);
  if (!response.ok)
    throw new Error(`Failed to download font: ${response.statusText}`);
  const arrayBuffer = await response.arrayBuffer();
  fs.writeFileSync(dest, Buffer.from(arrayBuffer));
  console.log(`Font saved to ${dest}`);
}

async function convertAssets() {
  console.log("--- Batch HTML to SVG Conversion Start ---");

  if (!fs.existsSync(TEMP_DIR)) {
    fs.mkdirSync(TEMP_DIR, { recursive: true });
  }

  try {
    for (const font of FONT_MAP) {
      await downloadFont(font.url, font.path);
    }

    const createSatoruModule = require(WASM_JS_PATH);
    const instance = await createSatoruModule({
      locateFile: (url: string) =>
        url.endsWith(".wasm") ? WASM_BINARY_PATH : url,
      print: (text: string) => console.log(`[WASM] ${text}`),
      printErr: (text: string) => console.error(`[WASM ERROR] ${text}`),
    });

    instance._init_engine();

    // Helper to load fonts
    const loadFont = (filePath: string, fontName: string) => {
      if (fs.existsSync(filePath)) {
        console.log(
          `Loading font: ${fontName} from ${path.basename(filePath)}`,
        );
        const fontData = fs.readFileSync(filePath);
        const fontPtr = instance._malloc(fontData.length);
        instance.HEAPU8.set(new Uint8Array(fontData), fontPtr);
        const namePtr = instance._malloc(fontName.length + 1);
        instance.stringToUTF8(fontName, namePtr, fontName.length + 1);
        instance._load_font(namePtr, fontPtr, fontData.length);
        instance._free(fontPtr);
        instance._free(namePtr);
      } else {
        console.warn(`Font not found: ${filePath}`);
      }
    };

    // Load downloaded fonts
    for (const font of FONT_MAP) {
      loadFont(font.path, font.name);
    }

    const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

    for (const file of files) {
      const inputPath = path.join(ASSETS_DIR, file);
      const outputPath = path.join(TEMP_DIR, file.replace(".html", ".svg"));

      console.log(`Converting: ${file} ...`);
      const html = fs.readFileSync(inputPath, "utf8");

      // Preload images (data URLs)
      instance._clear_images();
      const dataUrls = new Set<string>();

      const imgRegex = /<img[^>]+src=["'](data:image\/[^'"]+)["']/g;
      const bgRegex =
        /background-image:\s*url\(['"]?(data:image\/[^'"]+)['"]?\)/g;

      let match;
      while ((match = imgRegex.exec(html)) !== null) dataUrls.add(match[1]);
      while ((match = bgRegex.exec(html)) !== null) dataUrls.add(match[1]);

      for (const url of dataUrls) {
        const name = url;
        instance.ccall(
          "load_image",
          null,
          ["string", "string", "number", "number"],
          [name, url, 0, 0],
        );
      }

      const htmlBuffer = Buffer.from(html + "\0", "utf8");
      const htmlPtr = instance._malloc(htmlBuffer.length);
      instance.HEAPU8.set(htmlBuffer, htmlPtr);

      const svgPtr = instance._html_to_svg(htmlPtr, 800, 0);
      const svg = instance.UTF8ToString(svgPtr).replace(/\0/g, "");

      fs.writeFileSync(outputPath, svg);
      instance._free(htmlPtr);
    }

    console.log(`--- Finished! ${files.length} files converted. ---`);
  } catch (error) {
    console.error("Conversion failed:", error);
  }
}

convertAssets();