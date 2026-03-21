import { describe, it, expect, beforeAll } from "vitest";
import { Satoru } from "satoru-render";

describe("Multi-page PDF Rendering", () => {
  let satoru: Satoru;

  beforeAll(async () => {
    satoru = await Satoru.create();
  });

  it("should render a multi-page PDF from an array of HTML strings without errors", async () => {
    const htmls = [
      "<html><body><h1>Page 1</h1><p>This is the first page.</p></body></html>",
      "<html><body><h1>Page 2</h1><p>This is the second page.</p></body></html>",
    ];

    const pdfData = await satoru.render({
      value: htmls,
      width: 800,
      format: "pdf",
    });

    expect(pdfData).toBeInstanceOf(Uint8Array);
    expect(pdfData.length).toBeGreaterThan(0);
    
    // Simple PDF header check
    const header = new TextDecoder().decode(pdfData.slice(0, 5));
    expect(header).toBe("%PDF-");
  });

  it("should render two isolated single-page PDFs without errors", async () => {
    const satoru1 = await Satoru.create();
    const pdfData1 = await satoru1.render({
      value: "A",
      width: 800,
      format: "pdf",
    });
    expect(pdfData1.length).toBeGreaterThan(0);

    const satoru2 = await Satoru.create();
    const pdfData2 = await satoru2.render({
      value: "B",
      width: 800,
      format: "pdf",
    });
    expect(pdfData2.length).toBeGreaterThan(0);
  });
});
