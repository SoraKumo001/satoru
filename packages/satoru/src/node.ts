import {
  SatoruBase,
  RequiredResource,
  resolveGoogleFonts,
} from "./core.js";
import * as path from "path";
import * as fs from "fs";

export * from "./core.js";
export * from "./log-level.js";
export type { SatoruWorker } from "./child-workers.js";

export class Satoru extends SatoruBase {
  static async create(createSatoruModuleFunc: any): Promise<Satoru> {
    return new Satoru(createSatoruModuleFunc);
  }

  static async defaultResourceResolver(
    resource: RequiredResource,
    baseUrl?: string,
    userAgent?: string,
  ): Promise<Uint8Array | null> {
    try {
      if (resource.url.startsWith("provider:google-fonts")) {
        return resolveGoogleFonts(resource, userAgent);
      }

      const isAbsolute = /^[a-z][a-z0-9+.-]*:/i.test(resource.url);

      let baseDir = baseUrl
        ? baseUrl.startsWith("file://")
          ? baseUrl.slice(7)
          : baseUrl
        : process.cwd();

      if (process.platform === "win32" && baseDir.startsWith("/")) {
        baseDir = baseDir.slice(1);
      }

      if (
        !isAbsolute &&
        !/^[a-z][a-z0-9+.-]*:\/\//i.test(baseDir) &&
        !baseDir.startsWith("data:")
      ) {
        const filePath = path.join(baseDir, resource.url);
        if (fs.existsSync(filePath)) {
          return new Uint8Array(fs.readFileSync(filePath));
        }
      }

      let finalUrl: string | null = null;
      if (isAbsolute) {
        finalUrl = resource.url;
      } else if (baseUrl && /^[a-z][a-z0-9+.-]*:/i.test(baseUrl)) {
        finalUrl = new URL(resource.url, baseUrl).href;
      }

      if (!finalUrl) return null;

      const headers: Record<string, string> = {};
      if (userAgent) {
        headers["User-Agent"] = userAgent;
      }
      const resp = await fetch(finalUrl, { headers });
      if (!resp.ok) return null;
      const buf = await resp.arrayBuffer();
      return new Uint8Array(buf);
    } catch (e) {
      console.warn(`[Satoru] Failed to resolve resource: ${resource.url}`, e);
      return null;
    }
  }

  protected resolveDefaultResource(
    resource: RequiredResource,
    baseUrl?: string,
    userAgent?: string,
  ): Promise<Uint8Array | null> {
    return Satoru.defaultResourceResolver(resource, baseUrl, userAgent);
  }

  protected async fetchHtml(url: string, userAgent?: string): Promise<string> {
    const headers: Record<string, string> = {};
    if (userAgent) {
      headers["User-Agent"] = userAgent;
    }
    const resp = await fetch(url, { headers });
    if (!resp.ok) {
      throw new Error(`Failed to fetch HTML from URL: ${url} (${resp.status})`);
    }
    return await resp.text();
  }
}
