import { chromium, Browser } from "playwright";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");
const REFERENCE_DIR = path.resolve(__dirname, "../reference");
const CONCURRENCY = 8; // 並列実行数

async function processFile(browser: Browser, file: string) {
  const inputPath = path.join(ASSETS_DIR, file);
  const outputPath = path.join(REFERENCE_DIR, file.replace(".html", ".png"));

  const context = await browser.newContext({
    viewport: { width: 800, height: 1000 },
    deviceScaleFactor: 1,
  });
  const page = await context.newPage();

  try {
    const htmlSource = fs.readFileSync(inputPath, "utf8");
    const html = `<style>body { margin: 8px; }</style>${htmlSource}`;
    await page.setContent(html);
    await page.waitForLoadState("networkidle");
    await page.waitForTimeout(500);

    const height = await page.evaluate(() => {
      const doc = document.documentElement;
      const body = document.body;
      return Math.max(
        doc.scrollHeight,
        doc.offsetHeight,
        doc.clientHeight,
        body ? body.scrollHeight : 0,
        body ? body.offsetHeight : 0,
      );
    });

    const finalHeight = Math.max(1, Math.ceil(height));
    await page.setViewportSize({ width: 800, height: finalHeight });

    await page.screenshot({
      path: outputPath,
      clip: { x: 0, y: 0, width: 800, height: finalHeight },
      omitBackground: false,
    });
    console.log(`✓ Generated: ${file}`);
  } finally {
    await context.close();
  }
}

async function generateReferences() {
  if (!fs.existsSync(REFERENCE_DIR)) {
    fs.mkdirSync(REFERENCE_DIR, { recursive: true });
  }

  const browser = await chromium.launch();
  const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

  console.log(
    `Starting reference generation for ${files.length} files (concurrency: ${CONCURRENCY})...`,
  );
  const start = Date.now();

  // バッチ処理で並列実行
  for (let i = 0; i < files.length; i += CONCURRENCY) {
    const batch = files.slice(i, i + CONCURRENCY);
    await Promise.all(batch.map((file) => processFile(browser, file)));
  }

  await browser.close();
  const duration = ((Date.now() - start) / 1000).toFixed(2);
  console.log(`Done generating references in ${duration}s.`);
}

generateReferences().catch(console.error);
