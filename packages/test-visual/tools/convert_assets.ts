import fs from "fs";
import path from "path";
import { fileURLToPath, pathToFileURL } from "url";
import { Satoru } from "satoru";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ASSETS_DIR = path.resolve(__dirname, "../../../assets");
const TEMP_DIR = path.resolve(__dirname, "../../../temp");
const WASM_JS_PATH = path.resolve(__dirname, "../../satoru/dist/satoru.js");
const WASM_BINARY_PATH = path.resolve(__dirname, "../../satoru/dist/satoru.wasm");

const ROBOTO_400 = "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2";

const FONT_MAP = [
  { name: "Roboto", url: ROBOTO_400, path: path.join(TEMP_DIR, "Roboto-400.woff2") },
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
  console.log("--- Batch HTML to SVG & PNG Conversion Start ---");

  if (!fs.existsSync(TEMP_DIR)) {
    fs.mkdirSync(TEMP_DIR, { recursive: true });
  }

  try {
    for (const font of FONT_MAP) {
      await downloadFont(font.url, font.path);
    }

    const { default: createSatoruModule } = await import(pathToFileURL(WASM_JS_PATH).href);
    const satoru = new Satoru(createSatoruModule);
    
    await satoru.init({
      locateFile: (url: string) =>
        url.endsWith(".wasm") ? WASM_BINARY_PATH : url,
      print: (text: string) => console.log(`[WASM] ${text}`),
      printErr: (text: string) => console.error(`[WASM ERROR] ${text}`),
    });

    for (const font of FONT_MAP) {
      if (fs.existsSync(font.path)) {
        const fontData = fs.readFileSync(font.path);
        satoru.loadFont(font.name, new Uint8Array(fontData));
      }
    }

    const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

    for (const file of files) {
      const inputPath = path.join(ASSETS_DIR, file);
      const svgOutputPath = path.join(TEMP_DIR, file.replace(".html", ".svg"));
      const pngOutputPath = path.join(TEMP_DIR, file.replace(".html", ".png"));

      console.log(`Converting: ${file} ...`);
      const html = fs.readFileSync(inputPath, "utf8");

      const resolveResource = async (r: { type: "font" | "css"; name: string; url: string }) => {
        // Handle internal fallback fonts or predefined maps
        const mapped = FONT_MAP.find(f => f.name === r.name);
        const url = mapped ? mapped.url : r.url;

        if (!url) return null;

        try {
          if (url.startsWith("http")) {
            const response = await fetch(url);
            if (!response.ok) {
              console.warn(`  [Fetch Failed] ${url} (${response.status})`);
              return null;
            }
            console.log(`  [Resource Resolved] Type: ${r.type}, Name: ${r.name}`);
            if (r.type === "font") return new Uint8Array(await response.arrayBuffer());
            return await response.text();
          } else {
            const localPath = path.join(TEMP_DIR, url);
            if (fs.existsSync(localPath)) {
              console.log(`  [Resource Resolved Local] Type: ${r.type}, Path: ${url}`);
              const data = fs.readFileSync(localPath);
              return r.type === "font" ? new Uint8Array(data) : data.toString("utf8");
            }
            return null;
          }
        } catch (e) {
          console.warn(`  [Resolve Error] ${url}`, e);
          return null;
        }
      };

      // Preload images (Existing logic)
      satoru.clearImages();
      const dataUrls = new Set<string>();
      const imgRegex = /<img[^>]+src=["'](data:image\/[^'"]+)["']/g;
      const bgRegex = /background-image:\s*url\(["']?(data:image\/[^'"]+)["']?\)/g;

      let match;
      while ((match = imgRegex.exec(html)) !== null) dataUrls.add(match[1]);
      while ((match = bgRegex.exec(html)) !== null) dataUrls.add(match[1]);

      for (const url of dataUrls) {
        satoru.loadImage(url, url);
      }

      // SVG Conversion
      const svg = await satoru.render(html, 800, {
        format: "svg",
        resolveResource
      }) as string;
      if (svg) fs.writeFileSync(svgOutputPath, svg);

      // PNG Conversion
      const pngBuffer = await satoru.render(html, 800, {
        format: "png",
        resolveResource
      }) as Uint8Array;
      if (pngBuffer) {
        fs.writeFileSync(pngOutputPath, Buffer.from(pngBuffer));
      }
    }

    console.log(`--- Finished! ${files.length} files converted. ---`);
  } catch (error) {
    console.error("Conversion failed:", error);
  }
}

convertAssets();
