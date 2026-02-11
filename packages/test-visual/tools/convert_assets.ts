import fs from "fs";
import path from "path";
import os from "os";
import { fileURLToPath } from "url";
import { createSatoruWorker } from "satoru/workers";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ASSETS_DIR = path.resolve(__dirname, "../../../assets");
const TEMP_DIR = path.resolve(__dirname, "../temp");

async function convertAssets() {
  console.log("--- Satoru Multi-threaded Asset Conversion Start ---");
  const startTotal = Date.now();

  if (!fs.existsSync(TEMP_DIR)) {
    fs.mkdirSync(TEMP_DIR, { recursive: true });
  }

  const args = process.argv.slice(2);
  let files = args.filter((arg) => !arg.startsWith("-"));

  if (files.length === 0) {
    files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));
  } else {
    files = files.map((f) => (f.endsWith(".html") ? f : f + ".html"));
    const missing = files.filter(
      (f) => !fs.existsSync(path.join(ASSETS_DIR, f)),
    );
    if (missing.length > 0) {
      console.warn(
        `Warning: Files not found in ${ASSETS_DIR}: ${missing.join(", ")}`,
      );
      files = files.filter((f) => fs.existsSync(path.join(ASSETS_DIR, f)));
    }
  }

  if (files.length === 0) {
    console.log("No files to convert.");
    return;
  }

  const cpuCount = Math.min(os.cpus().length, files.length);
  console.log(`Initializing ${cpuCount} workers for ${files.length} files...`);

  const satoru = createSatoruWorker({ maxParallel: cpuCount });

  // Give some time for NodeWorker to be imported in the background
  await new Promise((resolve) => setTimeout(resolve, 500));

  // Set assets directory for all workers
  await satoru.setAssetsDir(ASSETS_DIR);

  const tasks = files.map(async (file) => {
    const start = Date.now();
    const inputPath = path.join(ASSETS_DIR, file);
    const html = fs.readFileSync(inputPath, "utf-8");

    // Clear caches for a clean state
    await satoru.clearAll();

    const formats: ("svg" | "png" | "pdf")[] = ["svg", "png", "pdf"];

    for (const format of formats) {
      const result = await satoru.render({
        html,
        width: 800,
        format,
      });

      if (result) {
        const ext = `.${format}`;
        const outputPath = path.join(TEMP_DIR, file.replace(".html", ext));
        if (typeof result === "string") {
          fs.writeFileSync(outputPath, result);
        } else {
          fs.writeFileSync(outputPath, Buffer.from(result));
        }
      }
    }

    const duration = Date.now() - start;
    console.log(`Finished: ${file} in ${duration}ms`);
  });

  try {
    await Promise.all(tasks);
    const durationTotal = ((Date.now() - startTotal) / 1000).toFixed(2);
    console.log(`--- Finished! All files converted in ${durationTotal}s. ---`);
  } catch (err) {
    console.error("Conversion failed:", err);
    process.exit(1);
  } finally {
    await satoru.close();
  }
}

convertAssets();
