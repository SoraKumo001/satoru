export enum LogLevel {
  None = 0,
  Error = 1,
  Warning = 2,
  Info = 3,
  Debug = 4,
}

export interface SatoruModule {
  _create_instance: () => number;
  _destroy_instance: (ptr: number) => void;
  _collect_resources: (inst: number, html: number, width: number) => void;
  _get_pending_resources: (inst: number) => number;
  _add_resource: (
    inst: number,
    url: number,
    type: number,
    data: number,
    size: number,
  ) => void;
  _scan_css: (inst: number, css: number) => void;
  _clear_css: (inst: number) => void;
  _load_font: (inst: number, name: number, data: number, size: number) => void;
  _clear_fonts: (inst: number) => void;
  _load_image: (
    inst: number,
    name: number,
    url: number,
    width: number,
    height: number,
  ) => void;
  _clear_images: (inst: number) => void;
  render: (
    inst: number,
    htmls: string | string[],
    width: number,
    height: number,
    format: number,
  ) => Uint8Array | null;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  UTF8ToString: (ptr: number) => string;
  stringToUTF8: (str: string, ptr: number, max: number) => void;
  lengthBytesUTF8: (str: string) => number;
  HEAPU8: {
    buffer: ArrayBuffer;
    set: (data: Uint8Array, ptr: number) => void;
  };
  onLog?: (level: LogLevel, message: string) => void;
  logLevel: LogLevel;
}

export interface RequiredResource {
  type: "font" | "css" | "image";
  url: string;
  name: string;
}

export type ResourceResolver = (
  resource: RequiredResource,
) => Promise<Uint8Array | null>;

export interface RenderOptions {
  value: string | string[];
  width: number;
  height?: number;
  format?: "svg" | "png" | "webp" | "pdf";
  resolveResource?: ResourceResolver;
  fonts?: { name: string; data: Uint8Array }[];
  images?: { name: string; url: string; width?: number; height?: number }[];
  css?: string;
  baseUrl?: string;
  userAgent?: string;
  logLevel?: LogLevel;
  onLog?: (level: LogLevel, message: string) => void;
}

export class Satoru {
  private factory: any;
  private modPromise?: Promise<SatoruModule>;
  private static instances = new Map<number, Satoru>();
  private activeOnLog?: (level: LogLevel, message: string) => void;

  protected constructor(factory: any) {
    this.factory = factory;
  }

  private async getModule(): Promise<SatoruModule> {
    if (!this.modPromise) {
      this.modPromise = (async () => {
        const onLog = (level: LogLevel, message: string) => {
          if (mod && level > mod.logLevel) return;

          for (const inst of Satoru.instances.values()) {
            if (inst.activeOnLog) {
              inst.activeOnLog(level, message);
              return;
            }
          }
        };

        const mod = (await this.factory({
          onLog,
          print: (text: string) => {
            onLog(LogLevel.Info, text);
          },
          printErr: (text: string) => {
            onLog(LogLevel.Error, text);
          },
        })) as SatoruModule;

        mod.onLog = onLog;
        mod.logLevel = LogLevel.None;

        return mod;
      })();
    }
    return this.modPromise;
  }

  private static defaultOnLog(level: LogLevel, message: string) {
    const prefix = "[Satoru WASM]";
    switch (level) {
      case LogLevel.Debug:
        console.debug(`${prefix} DEBUG: ${message}`);
        break;
      case LogLevel.Info:
        console.info(`${prefix} INFO: ${message}`);
        break;
      case LogLevel.Warning:
        console.warn(`${prefix} WARNING: ${message}`);
        break;
      case LogLevel.Error:
        console.error(`${prefix} ERROR: ${message}`);
        break;
    }
  }

  /**
   * Create Satoru instance.
   * @param createSatoruModuleFunc Factory function from satoru.js
   */
  static async create(createSatoruModuleFunc: any): Promise<Satoru> {
    return new Satoru(createSatoruModuleFunc);
  }

  async render(
    options: RenderOptions & { format: "png" | "webp" | "pdf" },
  ): Promise<Uint8Array>;
  async render(options: RenderOptions & { format?: "svg" }): Promise<string>;
  async render(options: RenderOptions): Promise<string | Uint8Array>;
  async render(options: RenderOptions): Promise<string | Uint8Array> {
    const mod = await this.getModule();
    const {
      value,
      width,
      height = 0,
      format = "svg",
      fonts,
      images,
      css,
      baseUrl,
      logLevel,
      onLog,
    } = options;

    const prevLogLevel = mod.logLevel;
    if (logLevel !== undefined) {
      mod.logLevel = logLevel;
    }

    const prevOnLog = this.activeOnLog;
    this.activeOnLog = onLog || Satoru.defaultOnLog;

    const instancePtr = mod._create_instance();
    Satoru.instances.set(instancePtr, this);

    try {
      // Apply local settings
      if (fonts) {
        for (const f of fonts) {
          this.applyFont(mod, instancePtr, f.name, f.data);
        }
      }
      if (images) {
        for (const img of images) {
          this.applyImage(
            mod,
            instancePtr,
            img.name,
            img.url,
            img.width ?? 0,
            img.height ?? 0,
          );
        }
      }
      if (css) {
        this.applyCss(mod, instancePtr, css);
      }

      let { resolveResource } = options;

      if (!resolveResource && baseUrl) {
        resolveResource = async (r: RequiredResource) => {
          try {
            const isAbsolute = /^[a-z][a-z0-9+.-]*:/i.test(r.url);
            if (
              !isAbsolute &&
              typeof process !== "undefined" &&
              process.versions?.node
            ) {
              try {
                const path = await import("path");
                const fs = await import("fs");
                let baseDir = baseUrl.startsWith("file://")
                  ? baseUrl.slice(7)
                  : baseUrl;
                if (process.platform === "win32" && baseDir.startsWith("/")) {
                  baseDir = baseDir.slice(1);
                }
                if (
                  !/^[a-z][a-z0-9+.-]*:\/\//i.test(baseDir) &&
                  !baseDir.startsWith("data:")
                ) {
                  const filePath = path.join(baseDir, r.url);
                  if (fs.existsSync(filePath)) {
                    return new Uint8Array(fs.readFileSync(filePath));
                  }
                }
              } catch (e) {
                // ignore
              }
            }

            let finalUrl: string;
            if (isAbsolute) {
              finalUrl = r.url;
            } else {
              if (!/^[a-z][a-z0-9+.-]*:/i.test(baseUrl)) return null;
              finalUrl = new URL(r.url, baseUrl).href;
            }

            const headers: Record<string, string> = {};
            if (
              typeof process !== "undefined" &&
              process.versions?.node &&
              !options.resolveResource
            ) {
              headers["User-Agent"] =
                options.userAgent ||
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
            }

            const resp = await fetch(finalUrl, { headers });
            if (!resp.ok) return null;
            const buf = await resp.arrayBuffer();
            return new Uint8Array(buf);
          } catch (e) {
            console.warn(`[Satoru] Failed to resolve resource: ${r.url}`, e);
            return null;
          }
        };
      }

      const inputHtmls = Array.isArray(value) ? value : [value];
      const processedHtmls: string[] = [];

      const resolvedUrls = new Set<string>();

      for (const rawHtml of inputHtmls) {
        let processedHtml = rawHtml;
        for (let i = 0; i < 10; i++) {
          const htmlPtr = this.stringToPtr(mod, processedHtml);
          mod._collect_resources(instancePtr, htmlPtr, width);
          mod._free(htmlPtr);

          const pendingPtr = mod._get_pending_resources(instancePtr);
          const json = mod.UTF8ToString(pendingPtr);
          mod._free(pendingPtr);

          if (!json) break;
          const resources = JSON.parse(json) as RequiredResource[];
          const pending = resources.filter((r) => !resolvedUrls.has(r.url));
          if (pending.length === 0) break;

          await Promise.all(
            pending.map(async (r) => {
              try {
                resolvedUrls.add(r.url);
                let data: Uint8Array | null = null;
                if (resolveResource) {
                  data = await resolveResource({ ...r });
                }
                if (data instanceof Uint8Array) {
                  this.addResourceInternal(
                    mod,
                    instancePtr,
                    r.url,
                    r.type,
                    data,
                  );
                }
              } catch (e) {
                console.warn(`Failed to resolve resource: ${r.url}`, e);
              }
            }),
          );
        }

        resolvedUrls.forEach((url) => {
          const escapedUrl = url.replace(/[.*+?^${}()|[\\\\\\]]/g, "\\\\$&");
          const linkRegex = new RegExp(
            `<link[^>]*href\\s*=\\s*([\"'])${escapedUrl}\\1[^>]*>`,
            "gi",
          );
          processedHtml = processedHtml.replace(linkRegex, "");
        });
        processedHtmls.push(processedHtml);
      }

      const formatMap = {
        svg: 0,
        png: 1,
        webp: 2,
        pdf: 3,
      };

      const result = mod.render(
        instancePtr,
        processedHtmls,
        width,
        height,
        formatMap[format as keyof typeof formatMap] ?? 0,
      );

      if (!result) {
        if (format === "svg") return "";
        return new Uint8Array();
      }

      if (format === "svg") {
        return new TextDecoder().decode(result);
      }

      return new Uint8Array(result);
    } finally {
      Satoru.instances.delete(instancePtr);
      mod._destroy_instance(instancePtr);
      mod.logLevel = prevLogLevel;
      this.activeOnLog = prevOnLog;
    }
  }

  private applyFont(
    mod: SatoruModule,
    instPtr: number,
    name: string,
    data: Uint8Array,
  ) {
    const namePtr = this.stringToPtr(mod, name);
    const dataPtr = mod._malloc(data.length);
    mod.HEAPU8.set(data, dataPtr);
    mod._load_font(instPtr, namePtr, dataPtr, data.length);
    mod._free(namePtr);
    mod._free(dataPtr);
  }

  private applyImage(
    mod: SatoruModule,
    instPtr: number,
    name: string,
    dataUrl: string,
    width: number = 0,
    height: number = 0,
  ) {
    const namePtr = this.stringToPtr(mod, name);
    const urlPtr = this.stringToPtr(mod, dataUrl);
    mod._load_image(instPtr, namePtr, urlPtr, width, height);
    mod._free(namePtr);
    mod._free(urlPtr);
  }

  private applyCss(mod: SatoruModule, instPtr: number, css: string) {
    const cssPtr = this.stringToPtr(mod, css);
    mod._scan_css(instPtr, cssPtr);
    mod._free(cssPtr);
  }

  private addResourceInternal(
    mod: SatoruModule,
    instPtr: number,
    url: string,
    type: "font" | "css" | "image",
    data: Uint8Array,
  ) {
    const urlPtr = this.stringToPtr(mod, url);
    const dataPtr = mod._malloc(data.length);
    mod.HEAPU8.set(data, dataPtr);
    let typeInt = 1;
    if (type === "image") typeInt = 2;
    if (type === "css") typeInt = 3;
    mod._add_resource(instPtr, urlPtr, typeInt, dataPtr, data.length);
    mod._free(urlPtr);
    mod._free(dataPtr);
  }

  private stringToPtr(mod: SatoruModule, str: string): number {
    const len = mod.lengthBytesUTF8(str) + 1;
    const ptr = mod._malloc(len);
    mod.stringToUTF8(str, ptr, len);
    return ptr;
  }
}

export type { SatoruWorker } from "./child-workers.js";
