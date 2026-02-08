import { parentPort, workerData } from "worker_threads";
import fs from "fs";
import path from "path";
import { Satoru } from "satoru";

async function runWorker() {
  const { files, assetsDir, tempDir, wasmBinaryPath, fontMap } = workerData;

  try {
    const satoru = await Satoru.init();

    for (const file of files) {
      satoru.clearFonts();
      satoru.clearImages();

      const start = Date.now();
      const inputPath = path.join(assetsDir, file);
      const html = fs.readFileSync(inputPath, "utf-8");

      const resolveResource = async (r: any) => {
        if (r.type === "font") {
          const font = fontMap.find(
            (f: any) => f.url === r.url || f.name === r.name,
          );
          if (font && fs.existsSync(font.path)) {
            return new Uint8Array(fs.readFileSync(font.path));
          }
        }
        return null;
      };

      const svg = await satoru.render(html, 800, {
        format: "svg",
        resolveResource,
      });
      if (typeof svg === "string") {
        fs.writeFileSync(
          path.join(tempDir, file.replace(".html", ".svg")),
          svg,
        );
      }

      const png = await satoru.render(html, 800, {
        format: "png",
        resolveResource,
      });
      if (png instanceof Uint8Array) {
        fs.writeFileSync(
          path.join(tempDir, file.replace(".html", ".png")),
          png,
        );
      }

      const duration = Date.now() - start;
      parentPort?.postMessage({ type: "progress", file, duration });
    }

    parentPort?.postMessage({ type: "done" });
  } catch (err) {
    console.error("Worker error:", err);
    parentPort?.postMessage({ type: "error", error: String(err) });
  }
}

runWorker();
