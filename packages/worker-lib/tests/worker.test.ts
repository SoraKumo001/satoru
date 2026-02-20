import { describe, it, expect, afterAll } from "vitest";
import { createWorker } from "../src/node";
import type { TestWorker } from "./worker-script";
import path from "node:path";

describe("worker-lib node", () => {
  const { execute, close } = createWorker<TestWorker>(
    () => {
      // Use tsx to run the TypeScript worker file directly
      const workerPath = path.resolve(__dirname, "./worker-script.ts");
      const normalizedPath = workerPath.split(path.sep).join("/");
      return new (require("node:worker_threads").Worker)(
        `require('tsx/cjs'); require('${normalizedPath}');`,
        { eval: true }
      );
    },
    2
  );

  afterAll(() => {
    close();
  });

  it("should execute a simple function", async () => {
    const result = await execute("add", 10, 20);
    expect(result).toBe(30);
  });

  it("should execute a function with callbacks", async () => {
    const progress: { percent: number; status: string }[] = [];
    
    const result = await execute("asyncTask", (percent, status) => {
      progress.push({ percent, status });
    });

    expect(result).toBe("task-result");
    expect(progress).toHaveLength(3);
    expect(progress[0]).toEqual({ percent: 10, status: "starting" });
    expect(progress[1]).toEqual({ percent: 50, status: "halfway" });
    expect(progress[2]).toEqual({ percent: 100, status: "done" });
  });
});
