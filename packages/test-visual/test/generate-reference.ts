import { chromium } from "playwright";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");
const REFERENCE_DIR = path.resolve(__dirname, "../reference");

async function generateReferences() {
  if (!fs.existsSync(REFERENCE_DIR)) {
    fs.mkdirSync(REFERENCE_DIR, { recursive: true });
  }

  const browser = await chromium.launch();
  const context = await browser.newContext({
    viewport: { width: 800, height: 1000 },
    deviceScaleFactor: 1,
  });
  const page = await context.newPage();

  const files = fs.readdirSync(ASSETS_DIR).filter((f) => f.endsWith(".html"));

  for (const file of files) {
    const inputPath = path.join(ASSETS_DIR, file);
    const outputPath = path.join(REFERENCE_DIR, file.replace(".html", ".png"));
    
    console.log(`Generating reference for: ${file} ...`);
    const html = fs.readFileSync(inputPath, "utf8");
    
    await page.setContent(html);
    await page.addStyleTag({ content: "body { margin: 0; padding: 0; overflow: hidden; }" });
    await page.waitForLoadState("networkidle");
    await page.waitForTimeout(500);

    const body = await page.$("body");
    const box = await body?.boundingBox();
    
    if (box) {
      await page.screenshot({ 
        path: outputPath, 
        clip: { x: 0, y: 0, width: 800, height: Math.ceil(box.height) },
        omitBackground: true 
      });
    } else {
      await page.screenshot({ path: outputPath });
    }
  }

  await browser.close();
  console.log("Done generating references.");
}

generateReferences().catch(console.error);
