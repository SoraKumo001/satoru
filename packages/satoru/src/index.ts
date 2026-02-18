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
  collect_resources: (inst: number, html: string, width: number) => void;
  get_pending_resources: (inst: number) => string;
  add_resource: (
    inst: number,
    url: string,
    type: number,
    data: Uint8Array,
  ) => void;
  scan_css: (inst: number, css: string) => void;
  load_font: (inst: number, name: string, data: Uint8Array) => void;
  load_image: (
    inst: number,
    name: string,
    url: string,
    width: number,
    height: number,
  ) => void;
  init_document: (inst: number, html: string, width: number) => void;
  layout_document: (inst: number, width: number) => void;
  render_from_state: (
    inst: number,
    width: number,
    height: number,
    format: number,
    svgTextToPaths: boolean,
  ) => Uint8Array | null;
  render: (
    inst: number,
    htmls: string | string[],
    width: number,
    height: number,
    format: number,
    svgTextToPaths: boolean,
  ) => Uint8Array | null;
  onLog?: (level: LogLevel, message: string) => void;
  logLevel: LogLevel;
}

export interface RequiredResource {
  type: "font" | "css" | "image";
  url: string;
  name: string;
  redraw_on_ready?: boolean;
}

export type ResourceResolver = (
  resource: RequiredResource,
) => Promise<Uint8Array | null>;

export interface RenderOptions {
  value?: string | string[];
  url?: string;
  width: number;
  height?: number;
  format?: "svg" | "png" | "webp" | "pdf";
  textToPaths?: boolean;
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

  protected constructor(factory: any) {
    this.factory = factory;
  }

  private async getModule(): Promise<SatoruModule> {
    if (!this.modPromise) {
      this.modPromise = (async () => {
        var mod: SatoruModule = (await this.factory({
          onLog: (level: LogLevel, message: string) => {
            if (
              mod &&
              mod.logLevel !== LogLevel.None &&
              level <= mod.logLevel &&
              mod.onLog
            ) {
              mod.onLog(level, message);
            }
          },
          print: (text: string) => {
            if (
              mod &&
              mod.logLevel !== LogLevel.None &&
              LogLevel.Info <= mod.logLevel &&
              mod.onLog
            ) {
              mod.onLog(LogLevel.Info, text);
            }
          },
          printErr: (text: string) => {
            if (
              mod &&
              mod.logLevel !== LogLevel.None &&
              LogLevel.Error <= mod.logLevel &&
              mod.onLog
            ) {
              mod.onLog(LogLevel.Error, text);
            }
          },
        })) as SatoruModule;

        mod.logLevel = LogLevel.None;
        return mod;
      })();
    }
    return this.modPromise;
  }

  /**
   * Create Satoru instance.
   * @param createSatoruModuleFunc Factory function from satoru.js
   */
  static async create(createSatoruModuleFunc: any): Promise<Satoru> {
    return new Satoru(createSatoruModuleFunc);
  }

  async initDocument(
    options: Omit<RenderOptions, "value"> & { html: string },
  ): Promise<number> {
    const mod = await this.getModule();
    const inst = mod._create_instance();
    // Load resources similar to render() ...
    // This part is tricky because resource loading logic is tied to render().
    // Ideally we reuse the resource loading logic.
    // For now, I'll just expose the raw init and layout.
    mod.init_document(inst, options.html, options.width);
    return inst;
  }

  async layoutDocument(inst: number, width: number): Promise<void> {
    const mod = await this.getModule();
    mod.layout_document(inst, width);
  }

  async renderFromState(
    inst: number,
    options: {
      width: number;
      height?: number;
      format?: "svg" | "png" | "webp" | "pdf";
      textToPaths?: boolean;
    },
  ): Promise<string | Uint8Array> {
    const mod = await this.getModule();
    const formatMap = {
      svg: 0,
      png: 1,
      webp: 2,
      pdf: 3,
    };
    const format = formatMap[options.format ?? "svg"] ?? 0;
    const result = mod.render_from_state(
      inst,
      options.width,
      options.height ?? 0,
      format,
      options.textToPaths ?? true,
    );

    if (!result) {
      if (options.format === "svg") return "";
      return new Uint8Array();
    }

    if (options.format === "svg") {
      return new TextDecoder().decode(result);
    }
    return new Uint8Array(result);
  }

  async destroyInstance(inst: number): Promise<void> {
    const mod = await this.getModule();
    mod._destroy_instance(inst);
  }

  async render(
    options: RenderOptions & { format: "png" | "webp" | "pdf" },
  ): Promise<Uint8Array>;
  async render(options: RenderOptions & { format?: "svg" }): Promise<string>;
  async render(options: RenderOptions): Promise<string | Uint8Array>;
  async render(options: RenderOptions): Promise<string | Uint8Array> {
    const mod = await this.getModule();
    let {
      value,
      url,
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

    if (url && !value) {
      if (!baseUrl) {
        baseUrl = url;
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
      const resp = await fetch(url, { headers });
      if (!resp.ok) {
        throw new Error(
          `Failed to fetch HTML from URL: ${url} (${resp.status})`,
        );
      }
      value = await resp.text();
    }

    if (!value) {
      throw new Error("Either 'value' or 'url' must be provided.");
    }

    const prevLogLevel = mod.logLevel;
    const prevOnLog = mod.onLog;

    mod.logLevel = logLevel ?? LogLevel.None;
    mod.onLog = onLog;

    const instancePtr = mod._create_instance();

    try {
      if (fonts) {
        for (const f of fonts) {
          mod.load_font(instancePtr, f.name, f.data);
        }
      }
      if (images) {
        for (const img of images) {
          mod.load_image(
            instancePtr,
            img.name,
            img.url,
            img.width ?? 0,
            img.height ?? 0,
          );
        }
      }
      if (css) {
        mod.scan_css(instancePtr, css);
      }

      let { resolveResource } = options;

      if (!resolveResource) {
        resolveResource = async (r: RequiredResource) => {
          try {
            const isAbsolute = /^[a-z][a-z0-9+.-]*:/i.test(r.url);
            if (typeof process !== "undefined" && process.versions?.node) {
              try {
                const path = await import("path");
                const fs = await import("fs");
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
                  const filePath = path.join(baseDir, r.url);
                  if (fs.existsSync(filePath)) {
                    return new Uint8Array(fs.readFileSync(filePath));
                  }
                }
              } catch (e) {
                // ignore
              }
            }

            let finalUrl: string | null = null;
            if (isAbsolute) {
              finalUrl = r.url;
            } else if (baseUrl && /^[a-z][a-z0-9+.-]*:/i.test(baseUrl)) {
              finalUrl = new URL(r.url, baseUrl).href;
            }

            if (!finalUrl) return null;

            const headers: Record<string, string> = {};
            if (
              typeof process !== "undefined" &&
              process.versions?.node &&
              !options.resolveResource &&
              options.userAgent
            ) {
              headers["User-Agent"] = options.userAgent;
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
          mod.collect_resources(instancePtr, processedHtml, width);

          const json = mod.get_pending_resources(instancePtr);
          if (!json) break;

          const resources = JSON.parse(json) as RequiredResource[];
          const pending = resources.filter((r) => !resolvedUrls.has(r.url));
          if (pending.length === 0) break;

          await Promise.all(
            pending.map(async (r) => {
              try {
                if (r.url.startsWith("data:")) {
                  return;
                }
                resolvedUrls.add(r.url);
                let data: Uint8Array | null = null;
                if (resolveResource) {
                  data = await resolveResource({ ...r });
                }
                if (data instanceof Uint8Array) {
                  let typeInt = 1;
                  if (r.type === "image") typeInt = 2;
                  if (r.type === "css") typeInt = 3;
                  mod.add_resource(instancePtr, r.url, typeInt, data);
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
        options.textToPaths ?? true,
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
      mod._destroy_instance(instancePtr);
      mod.logLevel = prevLogLevel;
      mod.onLog = prevOnLog;
    }
  }
}

export type { SatoruWorker } from "./child-workers.js";
