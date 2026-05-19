import { describe, it, expect } from "vitest";
import { Satoru } from "satoru-render";

describe("PDF Metadata", () => {
  it("should include metadata in generated PDF", async () => {
    const satoru = await Satoru.create();
    const pdf = await satoru.render({
      value: "<h1>Metadata Test</h1>",
      width: 400,
      format: "pdf",
      pdfTitle: "Custom Title",
      pdfAuthor: "Satoru Author",
      pdfSubject: "Test Subject",
      pdfKeywords: "Test, Metadata, Satoru",
      pdfCreator: "Satoru Creator",
      pdfProducer: "Satoru Producer",
    });

    expect(pdf).toBeInstanceOf(Uint8Array);
    expect(pdf.length).toBeGreaterThan(0);

    const pdfText = new TextDecoder().decode(pdf);
    
    // Metadata in PDF is often stored in /Title (String) format
    // Skia PDF output usually includes these in the info dict
    expect(pdfText).toContain("Custom Title");
    expect(pdfText).toContain("Satoru Author");
    expect(pdfText).toContain("Test Subject");
    expect(pdfText).toContain("Satoru Creator");
    expect(pdfText).toContain("Satoru Producer");
  });
});
