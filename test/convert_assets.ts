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
const NOTO_JP_PATH = path.resolve(
  __dirname,
  "../external/skia/resources/fonts/NotoSansCJK-VF-subset.otf.ttc",
);
const MS_GOTHIC_PATH = "C:\\Windows\\Fonts\\msgothic.ttc";

async function convertAssets() {
  console.log("--- Batch HTML to SVG Conversion Start ---");

  try {
    const createSatoruModule = require(WASM_JS_PATH);
    const instance = await createSatoruModule({
      locateFile: (url: string) =>
        url.endsWith(".wasm") ? WASM_BINARY_PATH : url,
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

    loadFont(ROBOTO_PATH, "Roboto");
    // Register as default
    loadFont(ROBOTO_PATH, "sans-serif");

    // Japanese fonts
    loadFont(NOTO_JP_PATH, "Noto Sans JP");
    loadFont(MS_GOTHIC_PATH, "MS Gothic");
    // Register MS Gothic as a fallback for generic families if needed
    loadFont(MS_GOTHIC_PATH, "serif");
    loadFont(MS_GOTHIC_PATH, "monospace");

    if (!fs.existsSync(TEMP_DIR)) {
      fs.mkdirSync(TEMP_DIR, { recursive: true });
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
      const bgRegex = /background-image:\s*url\(['"]?(data:image\/[^'"]+)['"]?\)/g;
      
      let match;
      while ((match = imgRegex.exec(html)) !== null) dataUrls.add(match[1]);
      while ((match = bgRegex.exec(html)) !== null) dataUrls.add(match[1]);

      for (const url of dataUrls) {
        const name = url;
        instance.ccall(
            'load_image',
            null,
            ['string', 'string', 'number', 'number'],
            [name, url, 0, 0]
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