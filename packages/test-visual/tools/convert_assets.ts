import fs from "fs";
import path from "path";
import { Satoru } from "satoru";
import { pathToFileURL } from "url";

const ASSETS_DIR = path.resolve("../../assets");
const TEMP_DIR = path.resolve("./temp");
const WASM_JS_PATH = path.resolve("../satoru/dist/satoru.js");
const WASM_BINARY_PATH = path.resolve("../satoru/dist/satoru.wasm");

const FONT_MAP = [
  {
    name: "Roboto",
    url: "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2",
    path: path.join(TEMP_DIR, "roboto.woff2"),
  },
  {
    name: "Noto Sans JP",
    url: "https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-400-normal.woff2",
    path: path.join(TEMP_DIR, "noto-sans-jp.woff2"),
  },
];

async function downloadFont(url: string, dest: string) {
  if (fs.existsSync(dest)) return;
  const resp = await fetch(url);
  const buf = await resp.arrayBuffer();
  fs.writeFileSync(dest, Buffer.from(buf));
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
    
    // Updated to use the new static init method
    const satoru = await Satoru.init(createSatoruModule, {
      locateFile: (url: string) =>
        url.endsWith(".wasm") ? WASM_BINARY_PATH : url,
      // @ts-ignore - printing methods are internal emscripten options
      print: (text: string) => console.log(`[WASM] ${text}`),
      printErr: (text: string) => console.error(`[WASM ERROR] ${text}`),
    });

    const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

    for (const file of files) {
      const inputPath = path.join(ASSETS_DIR, file);
      const svgOutputPath = path.join(TEMP_DIR, file.replace(".html", ".svg"));
      const pngOutputPath = path.join(TEMP_DIR, file.replace(".html", ".png"));

      console.log(`Converting: ${file} ...`);
      const html = fs.readFileSync(inputPath, "utf-8");

      const resourceResolver = async (r: any) => {
        const isFont = r.type === "font" || r.url.match(/\.(woff2?|ttf|otf)$/i);
        
        // Check if we have it locally first
        if (isFont) {
           const fontMatch = FONT_MAP.find(f => f.name === r.name || r.url.includes(f.name.toLowerCase().replace(/ /g, '-')));
           if (fontMatch && fs.existsSync(fontMatch.path)) {
              return new Uint8Array(fs.readFileSync(fontMatch.path));
           }
        }

        try {
          const resp = await fetch(r.url);
          if (!resp.ok) return null;
          if (r.type === "css") return await resp.text();
          return new Uint8Array(await resp.arrayBuffer());
        } catch (e) {
          return null;
        }
      };

      // 1. Render SVG
      const svg = await satoru.render(html, 800, {
        format: "svg",
        resolveResource: resourceResolver,
      });
      if (typeof svg === "string") {
        fs.writeFileSync(svgOutputPath, svg);
      }

      // 2. Render PNG
      const png = await satoru.render(html, 800, {
        format: "png",
        resolveResource: resourceResolver,
      });
      if (png instanceof Uint8Array) {
        fs.writeFileSync(pngOutputPath, png);
      }
    }

    console.log(`--- Finished! ${files.length} files converted. ---`);
  } catch (err) {
    console.error("Conversion failed:", err);
    process.exit(1);
  }
}

convertAssets();