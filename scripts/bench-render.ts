import * as fs from "node:fs";
import * as path from "node:path";
import { performance } from "node:perf_hooks";
import { Satoru } from "../packages/satoru/dist/single.js";

type Format = "svg" | "png" | "webp" | "pdf";

interface CaseDef {
  name: string;
  file: string;
  width: number;
  height?: number;
  formats: Format[];
}

const root = process.cwd();
const warmups = Number(process.env.SATORU_BENCH_WARMUPS ?? 1);
const iterations = Number(process.env.SATORU_BENCH_ITERATIONS ?? 5);
const profileEnabled = process.env.SATORU_BENCH_PROFILE === "1";

const cases: CaseDef[] = [
  {
    name: "layout",
    file: "assets/01-layout.html",
    width: 1200,
    height: 800,
    formats: ["svg", "png"],
  },
  {
    name: "typography",
    file: "assets/02-typography.html",
    width: 1200,
    height: 900,
    formats: ["svg", "png"],
  },
  {
    name: "images",
    file: "assets/05-images.html",
    width: 1200,
    height: 900,
    formats: ["svg", "png", "webp"],
  },
  {
    name: "grid",
    file: "assets/09-grid.html",
    width: 1200,
    height: 900,
    formats: ["svg", "png"],
  },
  {
    name: "vertical-writing",
    file: "assets/21-vertical-writing.html",
    width: 1200,
    height: 900,
    formats: ["svg", "png"],
  },
];

function stats(samples: number[]) {
  const sorted = [...samples].sort((a, b) => a - b);
  const sum = samples.reduce((a, b) => a + b, 0);
  return {
    min: sorted[0],
    mean: sum / samples.length,
    p95: sorted[Math.min(sorted.length - 1, Math.ceil(sorted.length * 0.95) - 1)],
  };
}

function fmtMs(value: number) {
  return `${value.toFixed(1)}ms`;
}

function fmtProfile(key: string, value: number) {
  if (key.endsWith("Count")) return `${key}=${value.toFixed(1)}`;
  return `${key}=${fmtMs(value)}`;
}

function addProfile(target: Record<string, number>, source?: Record<string, number>) {
  if (!source) return;
  for (const [key, value] of Object.entries(source)) {
    target[key] = (target[key] ?? 0) + value;
  }
}

async function timeRender(
  engine: Satoru,
  html: string,
  baseUrl: string,
  width: number,
  height: number | undefined,
  format: Format,
) {
  let profile: Record<string, number> | undefined;
  const started = performance.now();
  const result = await engine.render({
    value: html,
    width,
    height,
    format,
    baseUrl,
    textToPaths: format === "svg",
    profile: profileEnabled,
    onProfile: (p: Record<string, number>) => {
      profile = p;
    },
  } as any);
  const elapsed = performance.now() - started;
  const bytes = typeof result === "string" ? Buffer.byteLength(result) : result.byteLength;
  return { elapsed, bytes, profile };
}

async function main() {
  const selected = new Set(process.argv.slice(2));
  const engine = await Satoru.create();
  const rows: string[] = [];

  rows.push(
    `Render benchmark: ${iterations} iterations, ${warmups} warmup${warmups === 1 ? "" : "s"}`,
  );
  rows.push("case\tformat\tmin\tmean\tp95\tbytes");
  const profileRows: string[] = [];

  for (const c of cases) {
    if (selected.size > 0 && !selected.has(c.name)) continue;

    const filePath = path.join(root, c.file);
    const html = fs.readFileSync(filePath, "utf8");
    const baseUrl = path.dirname(filePath);

    for (const format of c.formats) {
      for (let i = 0; i < warmups; i++) {
        await timeRender(engine, html, baseUrl, c.width, c.height, format);
      }

      const samples: number[] = [];
      const profileTotals: Record<string, number> = {};
      let bytes = 0;
      for (let i = 0; i < iterations; i++) {
        const result = await timeRender(engine, html, baseUrl, c.width, c.height, format);
        samples.push(result.elapsed);
        bytes = result.bytes;
        addProfile(profileTotals, result.profile);
      }

      const s = stats(samples);
      rows.push(
        `${c.name}\t${format}\t${fmtMs(s.min)}\t${fmtMs(s.mean)}\t${fmtMs(s.p95)}\t${bytes}`,
      );

      if (profileEnabled) {
        const parts = Object.entries(profileTotals)
          .sort((a, b) => b[1] - a[1])
          .map(([key, value]) => fmtProfile(key, value / iterations));
        profileRows.push(`${c.name}\t${format}\t${parts.join("\t")}`);
      }
    }
  }

  console.log(rows.join("\n"));
  if (profileRows.length > 0) {
    console.log("\nProfile means:");
    console.log("case\tformat\tsections");
    console.log(profileRows.join("\n"));
  }
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
