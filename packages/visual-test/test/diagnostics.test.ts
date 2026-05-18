import { describe, it, expect, beforeAll } from "vitest";
import { Satoru } from "satoru-render/single";

describe("RenderDiagnostics", () => {
  let satoru: Satoru;

  beforeAll(async () => {
    satoru = await Satoru.create();
  });

  it("should return a stable diagnostics report format", async () => {
    let report: any = null;
    const html = `<div style="font-family: 'MissingFont'; color: red;">Hello</div><img src="https://example.com/not-found.png" />`;

    await satoru.render({
      value: html,
      width: 400,
      format: "png",
      diagnostics: true,
      onDiagnostics: (r) => {
        report = r;
      },
    });

    expect(report).not.toBeNull();
    expect(report.version).toBe(1);
    expect(report.format).toBe("png");
    expect(report.width).toBe(400);
    expect(report.mediaType).toBe("screen");
    expect(report.timings).toBeDefined();

    expect(report.fonts).toBeDefined();
    const missingFont = report.fonts.find((f: any) => f.family === "MissingFont");
    expect(missingFont).toBeDefined();

    expect(report.resources).toBeDefined();
    const failedImg = report.resources.find((r: any) => r.url === "https://example.com/not-found.png");
    expect(failedImg).toBeDefined();
    expect(failedImg.type).toBe("image");
    expect(failedImg.status).toBe("failed");
  });

  it("should not change the default render return type", async () => {
    const html = `<div>Hello</div>`;
    
    const png1 = await satoru.render({
      value: html,
      width: 400,
      format: "png",
    });

    const png2 = await satoru.render({
      value: html,
      width: 400,
      format: "png",
      diagnostics: true,
      onDiagnostics: () => {},
    });

    expect(png1).toBeInstanceOf(Uint8Array);
    expect(png2).toBeInstanceOf(Uint8Array);
    expect((png1 as Uint8Array).byteLength).toBeGreaterThan(0);
    expect((png2 as Uint8Array).byteLength).toBeGreaterThan(0);
  });
});
