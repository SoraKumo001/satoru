import { parentPort, workerData } from "worker_threads";
import fs from "fs";
import path from "path";
import { Satoru } from "satoru";

async function runWorker() {
  const { files, assetsDir, tempDir, fontMap } = workerData;

  try {
    let currentFile = "";
    const satoru = await Satoru.init(undefined, {
      onLog: (level, message) => {
        parentPort?.postMessage({ type: "log", level, message, file: currentFile });
      },
    });

    for (const file of files) {
      currentFile = file;
      satoru.clearFonts();
      satoru.clearImages();

      const start = Date.now();
      const inputPath = path.join(assetsDir, file);
      const html = fs.readFileSync(inputPath, "utf-8");

      const resolveResource = async (r: any) => {
        if (r.type === "font" || r.type === "css") {
          const font = fontMap.find(
            (f: any) => f.url === r.url || f.name === r.name,
          );
          if (font && fs.existsSync(font.path)) {
            return new Uint8Array(fs.readFileSync(font.path));
          }
          // If not in map but is a URL, try fetching it
          if (r.url && r.url.startsWith("http")) {
            try {
              // Set a browser-like User-Agent to get proper fonts from Google Fonts
              const headers = {
                "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
              };
              const resp = await fetch(r.url, { headers });
              if (resp.ok) {
                const buf = await resp.arrayBuffer();
                return new Uint8Array(buf);
              }
            } catch (e) {
              console.error(`Failed to fetch ${r.type} from ${r.url}:`, e);
            }
          }
        } else if (r.type === "image") {
          // Resolve local paths relative to assetsDir
          if (r.url && !r.url.startsWith("data:") && !r.url.startsWith("http")) {
            const imgPath = path.join(assetsDir, r.url);
            if (fs.existsSync(imgPath)) {
              return new Uint8Array(fs.readFileSync(imgPath));
            }
          } else if (r.url && r.url.startsWith("http")) {
             try {
              const resp = await fetch(r.url);
              if (resp.ok) {
                const buf = await resp.arrayBuffer();
                return new Uint8Array(buf);
              }
            } catch (e) {
              console.error(`Failed to fetch image from ${r.url}:`, e);
            }
          }
        }
        return null;
      };

      const svg = await satoru.render({
        html,
        width: 800,
        format: "svg",
        resolveResource,
      });
      if (typeof svg === "string") {
        fs.writeFileSync(
          path.join(tempDir, file.replace(".html", ".svg")),
          svg,
        );
      }

      const png = await satoru.render({
        html,
        width: 800,
        format: "png",
        resolveResource,
      });
      if (png instanceof Uint8Array) {
        fs.writeFileSync(
          path.join(tempDir, file.replace(".html", ".png")),
          png,
        );
      }

      const pdf = await satoru.render({
        html,
        width: 800,
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
