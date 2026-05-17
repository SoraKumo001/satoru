import { performance } from "perf_hooks";
import fs from "fs/promises";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru, LogLevel } from "satoru-render";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

async function runBenchmark() {
  const satoru = await Satoru.create();

  const cases = [
    { name: "layout", html: "01-layout.html", width: 800 },
    { name: "typography", html: "02-typography.html", width: 800 },
    { name: "images", html: "05-images.html", width: 800 },
    { name: "grid", html: "09-grid.html", width: 800 },
    { name: "complex-text", html: "11-complex-text.html", width: 800 },
    { name: "vertical-writing", html: "21-vertical-writing.html", width: 800 },
  ];

  const formats = ["svg", "png", "webp"];
  const iterations = 5;
  const warmup = 1;

  console.log(`Render benchmark: ${iterations} iterations, ${warmup} warmup`);
  console.log(`case\tformat\tmin\tmean\tp95\tbytes`);

  for (const c of cases) {
    const htmlPath = path.resolve(__dirname, `../assets/${c.html}`);
    let htmlStr = await fs.readFile(htmlPath, "utf-8");

    const baseUrl = `file:///${path.dirname(htmlPath).replace(/\\/g, "/")}/`;

    for (const format of formats) {
      if (c.name !== "images" && format === "webp") continue;

      const times: number[] = [];
      let lastBytes = 0;

      for (let i = 0; i < iterations + warmup; i++) {
        const start = performance.now();
        const res = await satoru.render({
          value: htmlStr,
          baseUrl,
          width: c.width,
          format: format as "svg" | "png" | "webp",
          logLevel: LogLevel.Info,
          onLog: (level, msg) => console.log(`[C++] ${msg}`),
        });
        const end = performance.now();

        if (i >= warmup) {
          times.push(end - start);
          lastBytes = res.length;
        }
      }

      times.sort((a, b) => a - b);
      const min = times[0];
      const mean = times.reduce((a, b) => a + b, 0) / times.length;
      const p95 = times[Math.floor(times.length * 0.95)];

      console.log(
        `${c.name}\t${format}\t${min.toFixed(1)}ms\t${mean.toFixed(
          1,
        )}ms\t${p95.toFixed(1)}ms\t${lastBytes}`,
      );
    }
  }
}

runBenchmark().catch(console.error);
