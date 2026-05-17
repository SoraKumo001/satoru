import { performance } from "perf_hooks";
import fs from "fs/promises";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru, LogLevel } from "satoru-render";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const profileEnabled = process.env.SATORU_BENCH_PROFILE === "1";
const iterations = Number(process.env.SATORU_BENCH_ITERATIONS ?? 5);
const warmup = Number(process.env.SATORU_BENCH_WARMUPS ?? 1);

function formatProfile(key: string, value: number) {
  if (key.endsWith("Count")) return `${key}=${value.toFixed(1)}`;
  return `${key}=${value.toFixed(1)}ms`;
}

function addProfile(target: Record<string, number>, source?: Record<string, number>) {
  if (!source) return;
  for (const [key, value] of Object.entries(source)) {
    target[key] = (target[key] ?? 0) + value;
  }
}

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
  const selected = new Set(process.argv.slice(2));
  const profileRows: string[] = [];

  console.log(`Render benchmark: ${iterations} iterations, ${warmup} warmup`);
  console.log(`case\tformat\tmin\tmean\tp95\tbytes`);

  for (const c of cases) {
    if (selected.size > 0 && !selected.has(c.name)) continue;

    const htmlPath = path.resolve(__dirname, `../assets/${c.html}`);
    let htmlStr = await fs.readFile(htmlPath, "utf-8");

    const baseUrl = `file:///${path.dirname(htmlPath).replace(/\\/g, "/")}/`;

    for (const format of formats) {
      if (c.name !== "images" && format === "webp") continue;

      const times: number[] = [];
      const profileTotals: Record<string, number> = {};
      let lastBytes = 0;

      for (let i = 0; i < iterations + warmup; i++) {
        let profile: Record<string, number> | undefined;
        const start = performance.now();
        const res = await satoru.render({
          value: htmlStr,
          baseUrl,
          width: c.width,
          format: format as "svg" | "png" | "webp",
          logLevel: LogLevel.None,
          profile: profileEnabled,
          onProfile: (p) => {
            profile = p;
          },
        });
        const end = performance.now();

        if (i >= warmup) {
          times.push(end - start);
          lastBytes = res.length;
          addProfile(profileTotals, profile);
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

      if (profileEnabled) {
        const profileParts = Object.entries(profileTotals)
          .sort((a, b) => b[1] - a[1])
          .map(([key, value]) => formatProfile(key, value / iterations));
        profileRows.push(`${c.name}\t${format}\t${profileParts.join("\t")}`);
      }
    }
  }

  if (profileRows.length > 0) {
    console.log("\nProfile means:");
    console.log("case\tformat\tsections");
    console.log(profileRows.join("\n"));
  }
}

runBenchmark().catch(console.error);
