import { describe, it, expect, beforeAll } from "vitest";
import path from "path";
import { fileURLToPath } from "url";
import { Satoru } from "satoru";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const ROOT_DIR = path.resolve(__dirname, "../../../");
const ASSETS_DIR = path.resolve(ROOT_DIR, "assets");

describe("URL Rendering Tests", () => {
  let satoru: Satoru;

  beforeAll(async () => {
    satoru = await Satoru.create();
  });

  it("should render HTML with background from a string (baseline check)", async () => {
    const html =
      "<html><body><div style='background: red; width: 100px; height: 100px;'></div></body></html>";

    const svg = await satoru.render({
      value: html,
      width: 800,
      format: "svg",
    });

    expect(svg).toContain("<svg");
    expect(svg).toContain('fill="red"');
  });

  it("should fetch and render HTML with background from a URL (data URL)", async () => {
    const html =
      "<html><body><div style='background: blue; width: 100px; height: 100px;'></div></body></html>";
    const dataUrl = `data:text/html;base64,${Buffer.from(html).toString("base64")}`;

    const svg = await satoru.render({
      url: dataUrl,
      width: 800,
      format: "svg",
    });

    expect(svg).toContain("<svg");
    expect(svg).toContain('fill="blue"');
  });

  it("should throw error for invalid URL", async () => {
    await expect(
      satoru.render({
        url: "https://non-existent-domain-satoru-test.com/index.html",
        width: 800,
      }),
    ).rejects.toThrow();
  });
});
