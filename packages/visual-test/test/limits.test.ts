import { describe, it, expect, beforeAll } from "vitest";
import { Satoru, DIAGNOSTIC_CODES } from "satoru-render/single";

describe("RenderLimits", () => {
  let satoru: Satoru;

  beforeAll(async () => {
    satoru = await Satoru.create();
  });

  it("should enforce maxResourceCount limit", async () => {
    let report: any = null;
    const html = `
      <img src="https://example.com/1.png" />
      <img src="https://example.com/2.png" />
      <img src="https://example.com/3.png" />
    `;

    await satoru.render({
      value: html,
      width: 400,
      limits: { maxResourceCount: 1 },
      diagnostics: true,
      onDiagnostics: (r) => { report = r; }
    });

    const skipped = report.resources.filter((r: any) => r.status === "skipped");
    expect(skipped.length).toBeGreaterThanOrEqual(2);
    expect(report.errors.some((e: any) => e.code === DIAGNOSTIC_CODES.LIMIT_RESOURCE_COUNT)).toBe(true);
  });

  it("should block protocols not in allowedProtocols", async () => {
    let report: any = null;
    const html = `<img src="http://example.com/test.png" />`;

    await satoru.render({
      value: html,
      width: 400,
      limits: { allowedProtocols: ["https:"] },
      diagnostics: true,
      onDiagnostics: (r) => { report = r; }
    });

    const blocked = report.resources.find((r: any) => r.url === "http://example.com/test.png");
    expect(blocked.status).toBe("skipped");
    expect(report.errors.some((e: any) => e.code === DIAGNOSTIC_CODES.LIMIT_PROTOCOL_BLOCKED)).toBe(true);
  });

  it("should block hosts in blockedHosts", async () => {
    let report: any = null;
    const html = `<img src="https://malicious.com/test.png" />`;

    await satoru.render({
      value: html,
      width: 400,
      limits: { blockedHosts: ["malicious.com"] },
      diagnostics: true,
      onDiagnostics: (r) => { report = r; }
    });

    const blocked = report.resources.find((r: any) => r.url === "https://malicious.com/test.png");
    expect(blocked.status).toBe("skipped");
    expect(report.errors.some((e: any) => e.code === DIAGNOSTIC_CODES.LIMIT_HOST_BLOCKED)).toBe(true);
  });

  it("should enforce timeoutMs limit", async () => {
    const html = `<div>Long Render</div>`;
    // We force a timeout by setting a very small value. 
    // It might still pass if render is very fast, but 0 or 1ms usually triggers it in the loop.
    await expect(satoru.render({
      value: html,
      width: 400,
      limits: { timeoutMs: -1 }
    })).rejects.toThrow(/timed out/);
  });
});
