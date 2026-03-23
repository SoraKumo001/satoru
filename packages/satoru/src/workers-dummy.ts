export type { SatoruWorker } from "./child-workers.js";
import { type RenderOptions } from "./index.js";
export type { RenderOptions };

export const render = (
  _params: RenderOptions,
): Promise<string | Uint8Array<ArrayBufferLike>> => {
  return undefined as never;
};

export const setLimit = (_limit: number): void => {};
export const close = () => {};
export const waitAll = () => Promise.resolve();
export const waitReady = (_retryTime?: number) => Promise.resolve();
export const launchWorker = () => Promise.resolve();
