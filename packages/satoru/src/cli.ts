#!/usr/bin/env node
import fs from "fs";
import path from "path";
import { Satoru, LogLevel } from "./single.js";

async function main() {
  const args = process.argv.slice(2);
  const options: any = {
    width: 800,
    format: "png",
  };

  let input: string | undefined;

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "-w" || arg === "--width") {
      options.width = parseInt(args[++i]);
    } else if (arg === "-h" || arg === "--height") {
      options.height = parseInt(args[++i]);
    } else if (arg === "-f" || arg === "--format") {
      options.format = args[++i];
    } else if (arg === "-o" || arg === "--output") {
      options.output = args[++i];
    } else if (arg === "--verbose") {
      options.verbose = true;
    } else if (arg === "--help") {
      printHelp();
      return;
    } else if (!arg.startsWith("-")) {
      input = arg;
    }
  }

  if (!input) {
    console.error("Error: Input file or URL is required.");
    printHelp();
    process.exit(1);
  }

  const isUrl = /^[a-z][a-z0-9+.-]*:/i.test(input) && !input.startsWith("data:");
  
  if (!options.output) {
    const ext = `.${options.format}`;
    if (isUrl) {
        options.output = "output" + ext;
    } else {
        const basename = path.basename(input, path.extname(input));
        options.output = basename + ext;
    }
  }

  // Deduce format from output extension if not explicitly set
  if (!args.includes("-f") && !args.includes("--format")) {
    const ext = path.extname(options.output).toLowerCase().slice(1);
    if (["svg", "png", "webp", "pdf"].includes(ext)) {
      options.format = ext;
    }
  }

  const satoru = await Satoru.create();

  const renderOptions: any = {
    width: options.width,
    height: options.height,
    format: options.format,
    logLevel: options.verbose ? LogLevel.Debug : LogLevel.None,
    css: "body { background-color: white; }",
  };

  if (options.verbose) {
    renderOptions.onLog = (level: LogLevel, message: string) => {
      console.error(`[Satoru] ${LogLevel[level]}: ${message}`);
    };
  }

  if (isUrl) {
    renderOptions.url = input;
  } else {
    if (!fs.existsSync(input)) {
      console.error(`Error: File not found: ${input}`);
      process.exit(1);
    }
    renderOptions.value = fs.readFileSync(input, "utf-8");
    renderOptions.baseUrl = path.dirname(path.resolve(input));
  }

  try {
    const result = await satoru.render(renderOptions);
    fs.writeFileSync(options.output, result);
    console.log(`Successfully rendered to ${options.output}`);
  } catch (err) {
    console.error("Error during rendering:", err);
    process.exit(1);
  }
}

function printHelp() {
  console.log(`
Usage: satoru <input-file-or-url> [options]

Options:
  -o, --output <path>    Output file path
  -w, --width <number>   Viewport width (default: 800)
  -h, --height <number>  Viewport height (default: 0, auto-calculate)
  -f, --format <format>  Output format: svg, png, webp, pdf
  --verbose              Enable detailed logging
  --help                 Show this help message
`);
}

main().catch((err) => {
  console.error("Fatal error:", err);
  process.exit(1);
});
