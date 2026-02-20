import { initWorker } from "../src/node";

const add = (a: number, b: number) => a + b;

const asyncTask = async (onProgress: (percent: number, status: string) => void) => {
  onProgress(10, "starting");
  onProgress(50, "halfway");
  onProgress(100, "done");
  return "task-result";
};

const map = initWorker({ add, asyncTask });
export type TestWorker = typeof map;
