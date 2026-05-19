import { describe, it, expect } from "vitest";
import { Satoru } from "satoru-render";

describe("PDF Templates and Margins", () => {
  it("should render PDF with margins and header/footer", async () => {
    const satoru = await Satoru.create();
    const pdf = await satoru.render({
      value: ["<div style='height: 1000px'>Page 1 content</div>", "<div style='height: 1000px'>Page 2 content</div>"],
      width: 400,
      format: "pdf",
      pdfMargin: { top: 50, bottom: 50, left: 20, right: 20 },
      pdfHeader: "<div style='background: #eee; height: 100%; display: flex; align-items: center; justify-content: center'>Header - Page {{pageNumber}} / {{totalPages}}</div>",
      pdfFooter: "<div style='background: #ddd; height: 100%; display: flex; align-items: center; justify-content: center'>Footer - Page {{pageNumber}}</div>",
      pdfTitle: "Custom Title",
    });

    expect(pdf).toBeInstanceOf(Uint8Array);
    expect(pdf.length).toBeGreaterThan(0);

    const pdfText = new TextDecoder().decode(pdf);
    
    // Check metadata (usually in plain text in PDF header/info)
    expect(pdfText).toContain("/Title (Custom Title)");
    
    // Check dimensions (MediaBox)
    // MediaBox [0 0 400 1100] -> 400 width, 1000 content + 50 top + 50 bottom = 1100 height
    expect(pdfText).toContain("/MediaBox [0 0 400 1100]");

    // Check page count
    expect(pdfText).toContain("/Count 2");
  });
});
