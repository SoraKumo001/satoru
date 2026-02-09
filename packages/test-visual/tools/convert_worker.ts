import { parentPort, workerData } from "worker_threads";
import fs from "fs";
import path from "path";
import { Satoru } from "satoru";

async function runWorker() {
  const { files, assetsDir, tempDir, fontMap } = workerData;

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
        } else if (r.type === "image") {
          // Resolve local paths relative to assetsDir
          if (r.url && !r.url.startsWith("data:") && !r.url.startsWith("http")) {
            const imgPath = path.join(assetsDir, r.url);
            if (fs.existsSync(imgPath)) {
              return new Uint8Array(fs.readFileSync(imgPath));
            }
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

      const pdf = await satoru.render(html, 800, {
        format: "pdf",
        resolveResource,
      });
      if (pdf instanceof Uint8Array) {
        fs.writeFileSync(
          path.join(tempDir, file.replace(".html", ".pdf")),
          pdf,
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
