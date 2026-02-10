import fs from "fs";
import path from "path";
import os from "os";
import { Worker } from "worker_threads";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ASSETS_DIR = path.resolve(__dirname, "../../../assets");
const TEMP_DIR = path.resolve(__dirname, "../temp");
const WASM_JS_PATH = path.resolve(__dirname, "../../satoru/dist/satoru.js");
const WASM_BINARY_PATH = path.resolve(__dirname, "../../satoru/dist/satoru.wasm");
const WORKER_PATH = path.resolve(__dirname, "./convert_worker.ts");

const FONT_MAP = [
  {
    name: "Roboto",
    url: "https://fonts.gstatic.com/s/roboto/v50/KFO7CnqEu92Fr1ME7kSn66aGLdTylUAMa3yUBA.woff2",
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
  console.log("--- Multi-threaded Asset Conversion Start ---");
  const startTotal = Date.now();

  if (!fs.existsSync(TEMP_DIR)) {
    fs.mkdirSync(TEMP_DIR, { recursive: true });
  }

  for (const font of FONT_MAP) {
    await downloadFont(font.url, font.path);
  }

  const args = process.argv.slice(2);
  const verbose = args.includes("--verbose") || args.includes("-v");
  let files = args.filter((arg) => !arg.startsWith("-"));

  if (files.length === 0) {
    files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));
  } else {
    files = files.map((f) => (f.endsWith(".html") ? f : f + ".html"));
    const missing = files.filter((f) => !fs.existsSync(path.join(ASSETS_DIR, f)));
    if (missing.length > 0) {
      console.warn(`Warning: Files not found in ${ASSETS_DIR}: ${missing.join(", ")}`);
      files = files.filter((f) => fs.existsSync(path.join(ASSETS_DIR, f)));
    }
  }

  if (files.length === 0) {
    console.log("No files to convert.");
    return;
  }

  const cpuCount = Math.min(os.cpus().length, files.length);
  const chunks = Array.from({ length: cpuCount }, () => [] as string[]);
  
  files.forEach((file, i) => chunks[i % cpuCount].push(file));

  console.log(`Spawning ${cpuCount} workers for ${files.length} files...`);

  const workerPromises = chunks.map((chunk, i) => {
    return new Promise<void>((resolve, reject) => {
      const worker = new Worker(WORKER_PATH, {
        execArgv: ["--import", "tsx"],
        workerData: {
          files: chunk,
          assetsDir: ASSETS_DIR,
          tempDir: TEMP_DIR,
          wasmJsPath: WASM_JS_PATH,
          wasmBinaryPath: WASM_BINARY_PATH,
          fontMap: FONT_MAP
        }
      });

      worker.on("message", (msg) => {
        if (msg.type === "progress") {
          console.log(`[Worker ${i}] Finished: ${msg.file} in ${msg.duration}ms`);
        } else if (msg.type === "log") {
          if (verbose) {
            const levelStr = ["DEBUG", "INFO", "WARNING", "ERROR"][msg.level] || "UNKNOWN";
            console.log(`[WASM][${msg.file || "init"}][${levelStr}] ${msg.message}`);
          }
        } else if (msg.type === "done") {
          resolve();
        }
      });

      worker.on("error", (err) => {
        console.error(`[Worker ${i}] Error:`, err);
        reject(err);
      });
      worker.on("exit", (code) => {
        if (code !== 0) reject(new Error(`Worker stopped with exit code ${code}`));
      });
    });
  });

  try {
    await Promise.all(workerPromises);
    const durationTotal = ((Date.now() - startTotal) / 1000).toFixed(2);
    console.log(`--- Finished! All files converted in ${durationTotal}s. ---`);
  } catch (err) {
    console.error("Conversion failed:", err);
    process.exit(1);
  }
}

convertAssets();