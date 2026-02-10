import { Satoru, LogLevel } from "../src/index.js";
import { describe, it, expect, vi } from "vitest";

describe("Satoru Logging", () => {
  it("should receive logs from WASM", async () => {
    const logs: { level: LogLevel; message: string }[] = [];
    const onLog = (level: LogLevel, message: string) => {
      logs.push({ level, message });
    };

    const satoru = await Satoru.init(undefined, { onLog });
    
    // api_init_engine should have triggered an info log
    expect(logs.length).toBeGreaterThan(0);
    expect(logs[0].level).toBe(LogLevel.Info);
    expect(logs[0].message).toContain("Satoru Engine Initializing");
  });
});
