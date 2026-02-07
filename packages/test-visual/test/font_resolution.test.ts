import { describe, it, expect, beforeAll } from "vitest";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru } from "satoru";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const ROOT_DIR = path.resolve(__dirname, "../../../");

describe("Font Resolution", () => {
  let satoru: Satoru;

  beforeAll(async () => {
    satoru = new Satoru();
    const wasmPath = path.resolve(ROOT_DIR, "packages/satoru/dist/satoru.wasm");
    await satoru.init({
      locateFile: (url: string) => url.endsWith(".wasm") ? wasmPath : url,
      print: (text: string) => console.log(`[C++] ${text}`),
      printErr: (text: string) => console.error(`[C++ ERR] ${text}`),
    });
  });

  it("should detect missing fonts via getRequiredResources", async () => {
    const html = `
      <html>
        <head>
          <style>
            @font-face {
              font-family: 'MyCustomFont';
              src: url('https://example.com/font.woff2');
            }
            body {
              font-family: 'MyCustomFont', sans-serif;
            }
          </style>
        </head>
        <body>
          <div style="font-family: 'AnotherFont'">Hello</div>
        </body>
      </html>
    `;

    // We expect getRequiredResources to return the fonts needed.
    // The C++ side parses CSS and finds @font-face rules.
    const resources = satoru.getRequiredResources(html, 800);
    console.log("Detected resources:", resources);

    const customFont = resources.find(r => r.name.includes("MyCustomFont"));
    expect(customFont).toBeDefined();
    expect(customFont?.url).toBe("https://example.com/font.woff2");

    // "AnotherFont" might be detected if the layout engine realizes it's missing and we don't have a URL for it yet
    // but the @font-face one is the critical one for the regex fix.
  });
});