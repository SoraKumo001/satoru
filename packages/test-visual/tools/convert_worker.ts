import { parentPort, workerData } from "worker_threads";
import fs from "fs";
import path from "path";
import { Satoru } from "satoru";
import { pathToFileURL } from "url";

async function runWorker() {
  const { files, assetsDir, tempDir, wasmJsPath, wasmBinaryPath, fontMap } = workerData;

  try {
    const { default: createSatoruModule } = await import(pathToFileURL(wasmJsPath).href);
    const satoru = await Satoru.init(createSatoruModule, {
      locateFile: (url: string) => url.endsWith(".wasm") ? wasmBinaryPath : url,
    });

    for (const font of fontMap) {
      if (fs.existsSync(font.path)) {
        satoru.loadFont(font.name, new Uint8Array(fs.readFileSync(font.path)));
      }
    }

    for (const file of files) {
      const start = Date.now();
      const inputPath = path.join(assetsDir, file);
      const html = fs.readFileSync(inputPath, "utf-8");

      const svg = await satoru.render(html, 800, { format: "svg" });
      if (typeof svg === "string") {
        fs.writeFileSync(path.join(tempDir, file.replace(".html", ".svg")), svg);
      }

      const png = await satoru.render(html, 800, { format: "png" });
      if (png instanceof Uint8Array) {
        fs.writeFileSync(path.join(tempDir, file.replace(".html", ".png")), png);
      }

      const duration = Date.now() - start;
      parentPort?.postMessage({ type: "progress", file, duration });
    }

    parentPort?.postMessage({ type: "done" });
  } catch (err) {
    parentPort?.postMessage({ type: "error", error: String(err) });
  }
}

runWorker();