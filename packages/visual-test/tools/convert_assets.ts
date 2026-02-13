import fs from "fs";
import path from "path";
import os from "os";
import { fileURLToPath } from "url";
import { createSatoruWorker, LogLevel } from "satoru";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ASSETS_DIR = path.resolve(__dirname, "../../../assets");
const TEMP_DIR = path.resolve(__dirname, "../temp");

async function main() {
  const args = process.argv.slice(2);
  const verbose = args.includes("--verbose");
  const files = args
    .filter((a) => a.endsWith(".html"))
    .concat(
      args.length === 0 || (args.length === 1 && verbose)
        ? fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"))
        : [],
    );

  if (files.length === 0) {
    console.log("No files to convert.");
    return;
  }

  if (!fs.existsSync(TEMP_DIR)) {
    fs.mkdirSync(TEMP_DIR, { recursive: true });
  }

  console.log(`--- Satoru Multi-threaded Asset Conversion Start ---`);

  const cpuCount = Math.min(os.cpus().length, files.length);
  console.log(`Initializing ${cpuCount} workers for ${files.length} files...`);

  const satoru = createSatoruWorker({
    maxParallel: cpuCount,
  });

  const onLog = (level: LogLevel, message: string) => {
    const prefix = "[Satoru Worker]";
    switch (level) {
      case LogLevel.Debug:
        console.debug(`${prefix} DEBUG: ${message}`);
        break;
      case LogLevel.Info:
        console.info(`${prefix} INFO: ${message}`);
        break;
      case LogLevel.Warning:
        console.warn(`${prefix} WARNING: ${message}`);
        break;
      case LogLevel.Error:
        console.error(`${prefix} ERROR: ${message}`);
        break;
    }
  };

  const startTime = Date.now();

  await Promise.all(
    files.map(async (file) => {
      const startFileTime = Date.now();
      const filePath = path.join(ASSETS_DIR, file);
      if (!fs.existsSync(filePath)) return;

      const html = fs.readFileSync(filePath, "utf-8");
      const formats: ("svg" | "png" | "pdf")[] = ["svg", "png", "pdf"];

      for (const format of formats) {
        const result = await satoru.render({
          html,
          width: 800,
          format,
          baseUrl: ASSETS_DIR,
          clear: true,
          onLog: verbose ? onLog : undefined,
          logLevel: verbose ? LogLevel.Debug : LogLevel.None,
        });

        if (result) {
          const ext = `.${format}`;
          const outputPath = path.join(TEMP_DIR, file.replace(".html", ext));
          fs.writeFileSync(outputPath, result);
        }
      }
      console.log(`Finished: ${file} in ${Date.now() - startFileTime}ms`);
    }),
  );

  console.log(
    `--- Finished! All files converted in ${((Date.now() - startTime) / 1000).toFixed(2)}s. ---`,
  );
  process.exit(0);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
