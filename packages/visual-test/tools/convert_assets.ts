import fs from "fs";
import path from "path";
import os from "os";
import { fileURLToPath } from "url";
import { createSatoruWorker, LogLevel } from "satoru-render/workers";

const __filename = fileURLToPath(import.meta.url);

const ASSETS_DIR = path.resolve(process.cwd(), "../../assets");
const TEMP_DIR = path.resolve(process.cwd(), "temp");

function getFilePath(file: string): string {
  if (path.isAbsolute(file)) {
    return file;
  }
  return path.resolve(process.cwd(), file);
}

async function main() {
  const args = process.argv.slice(2);
  const verbose = args.includes("--verbose");
  const noOutline = args.includes("--no-outline");
  const outline = args.includes("--outline");

  let width = 800;
  const widthIdx = args.findIndex((a) => a === "--width");
  if (widthIdx !== -1 && args[widthIdx + 1]) {
    width = parseInt(args[widthIdx + 1]);
  } else {
    const widthArg = args.find((a) => a.startsWith("--width="));
    if (widthArg) {
      width = parseInt(widthArg.split("=")[1]);
    }
  }

  const files = args.filter((a) => !a.startsWith("-"));

  if (files.length === 0) {
    const assetsDirFiles = fs
      .readdirSync(ASSETS_DIR)
      .filter((f) => f.endsWith(".html"));
    files.push(...assetsDirFiles.map((f) => path.join(ASSETS_DIR, f)));
  }

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
      const filePath = getFilePath(file);

      if (!fs.existsSync(filePath)) {
        console.error(`File not found: ${filePath}`);
        return;
      }

      const fileName = path.basename(filePath, path.extname(filePath));
      const html = fs.readFileSync(filePath, "utf-8");
      // Use the file's directory as base URL
      const fileDir = path.dirname(filePath);
      const formats: ("svg" | "png" | "webp" | "pdf")[] = [
        "svg",
        "png",
        "webp",
        "pdf",
      ];

      for (const format of formats) {
        try {
          const result = await satoru.render({
            value: html,
            width,
            format,
            baseUrl: fileDir,
            css: "body { margin: 8px; }",
            textToPaths: format === "svg" ? outline : !noOutline,
            onLog: verbose ? onLog : undefined,
            logLevel: verbose ? LogLevel.Debug : LogLevel.None,
          });

          if (result) {
            const ext = `.${format}`;
            const outputPath = path.join(TEMP_DIR, `${fileName}${ext}`);
            fs.writeFileSync(outputPath, result);
          }
        } catch (e) {
          console.error(`Error converting ${file} to ${format}:`, e);
        }
      }
      console.log(`Finished: ${fileName} in ${Date.now() - startFileTime}ms`);
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
