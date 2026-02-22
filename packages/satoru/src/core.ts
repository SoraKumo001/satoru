import { LogLevel } from "./log-level.js";

export interface SatoruModule {
  create_instance: () => any;
  destroy_instance: (inst: any) => void;
  collect_resources: (inst: any, html: string, width: number) => void;
  get_pending_resources: (inst: any) => string;
  add_resource: (
    inst: any,
    url: string,
    type: number,
    data: Uint8Array,
  ) => void;
  scan_css: (inst: any, css: string) => void;
  load_font: (inst: any, name: string, data: Uint8Array) => void;
  load_image: (
    inst: any,
    name: string,
    url: string,
    width: number,
    height: number,
  ) => void;
  set_log_level: (level: number) => void;
  init_document: (inst: any, html: string, width: number) => void;
  layout_document: (inst: any, width: number) => void;
  render_from_state: (
    inst: any,
    width: number,
    height: number,
    format: number,
    svgTextToPaths: boolean,
  ) => Uint8Array | null;
  render: (
    inst: any,
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
  defaultResolver: (resource: RequiredResource) => Promise<Uint8Array | null>,
) => Promise<Uint8Array | ArrayBufferView | null>;

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

export async function resolveGoogleFonts(
  resource: RequiredResource,
  userAgent?: string,
): Promise<Uint8Array | null> {
  if (!resource.url.startsWith("provider:google-fonts")) return null;

  const urlObj = new URL(resource.url);
  const family = urlObj.searchParams.get("family");
  const weight = urlObj.searchParams.get("weight") || "400";
  const italic = urlObj.searchParams.get("italic") === "1";

  if (!family) return null;

  let targetFamily = family;
  let forceNormalStyle = false;

  if (family === "sans-serif") {
    targetFamily = "Noto Sans JP";
    forceNormalStyle = true;
  } else if (family === "serif") {
    targetFamily = "Noto Serif JP";
    forceNormalStyle = true;
  } else if (family === "monospace") {
    targetFamily = "Noto Sans Mono";
    forceNormalStyle = true;
  } else if (family === "cursive") {
    targetFamily = "Klee One";
    forceNormalStyle = true;
  } else if (family === "fantasy") {
    targetFamily = "Mochiy Pop One";
    forceNormalStyle = true;
  } else if (
    family.includes("Noto Sans JP") ||
    family.includes("Noto Serif JP") ||
    family.includes("CJK")
  ) {
    forceNormalStyle = true;
  }

  const useItalic = italic && !forceNormalStyle;

  const googleFontUrl = `https://fonts.googleapis.com/css2?family=${encodeURIComponent(
    targetFamily,
  )}:ital,wght@${useItalic ? "1" : "0"},${weight}&display=swap`;

  const headers: Record<string, string> = {};
  if (userAgent) {
    headers["User-Agent"] = userAgent;
  }
  try {
    const resp = await fetch(googleFontUrl, { headers });
    if (!resp.ok) return null;
    const buf = await resp.arrayBuffer();
    return new Uint8Array(buf);
  } catch {
    return null;
  }
}

export abstract class SatoruBase {
  private factory: any;
  private modPromise?: Promise<SatoruModule>;

  protected constructor(factory: any) {
    this.factory = factory;
  }

  private async getModule(): Promise<SatoruModule> {
    if (!this.modPromise) {
      this.modPromise = (async () => {
        let currentLogLevel = LogLevel.None;
        let currentUserOnLog:
          | ((level: LogLevel, message: string) => void)
          | undefined;

        const initialModule: any = {
          onLog: (level: LogLevel, message: string) => {
            if (
              currentLogLevel !== LogLevel.None &&
              level <= currentLogLevel &&
              currentUserOnLog
            ) {
              currentUserOnLog(level, message);
            }
          },
          print: (text: string) => {
            if (
              currentLogLevel !== LogLevel.None &&
              LogLevel.Info <= currentLogLevel &&
              currentUserOnLog
            ) {
              currentUserOnLog(LogLevel.Info, text);
            }
          },
          printErr: (text: string) => {
            if (
              currentLogLevel !== LogLevel.None &&
              LogLevel.Error <= currentLogLevel &&
              currentUserOnLog
            ) {
              currentUserOnLog(LogLevel.Error, text);
            }
          },
        };

        const mod: SatoruModule = (await this.factory(
          initialModule,
        )) as SatoruModule;

        mod.logLevel = LogLevel.None;

        // Sync mod state to our local variables
        const originalSetLogLevel = mod.set_log_level;
        mod.set_log_level = (level: number) => {
          currentLogLevel = level;
          originalSetLogLevel(level);
        };

        // We need to intercept mod.onLog and mod.logLevel updates
        // In render(), these are set directly.
        // So we use getters/setters on mod itself.
        let internalOnLog = mod.onLog;
        Object.defineProperty(mod, "onLog", {
          get: () => internalOnLog,
          set: (val) => {
            internalOnLog = val;
            currentUserOnLog = val;
          },
        });

        let internalLogLevel = mod.logLevel;
        Object.defineProperty(mod, "logLevel", {
          get: () => internalLogLevel,
          set: (val) => {
            internalLogLevel = val;
            currentLogLevel = val;
          },
        });

        return mod;
      })();
    }
    return this.modPromise;
  }

  async initDocument(
    options: Omit<RenderOptions, "value"> & { html: string },
  ): Promise<any> {
    const mod = await this.getModule();
    const inst = mod.create_instance();
    mod.init_document(inst, options.html, options.width);
    return inst;
  }

  async layoutDocument(inst: any, width: number): Promise<void> {
    const mod = await this.getModule();
    mod.layout_document(inst, width);
  }

  async renderFromState(
    inst: any,
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

  async destroyInstance(inst: any): Promise<void> {
    const mod = await this.getModule();
    mod.destroy_instance(inst);
  }

  protected abstract resolveDefaultResource(
    resource: RequiredResource,
    baseUrl?: string,
    userAgent?: string,
  ): Promise<Uint8Array | null>;

  protected abstract fetchHtml(
    url: string,
    userAgent?: string,
  ): Promise<string>;

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

    if (!options.userAgent) {
      options.userAgent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
    }

    if (url && !value) {
      if (!baseUrl) {
        baseUrl = url;
      }
      value = await this.fetchHtml(url, options.userAgent);
    }

    if (!value) {
      throw new Error("Either 'value' or 'url' must be provided.");
    }

    const prevLogLevel = mod.logLevel;
    const prevOnLog = mod.onLog;

    mod.logLevel = logLevel ?? LogLevel.None;
    mod.set_log_level(mod.logLevel);
    mod.onLog = onLog;

    const instancePtr = mod.create_instance();

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

      const defaultResolver = (r: RequiredResource) =>
        this.resolveDefaultResource(r, baseUrl, options.userAgent);

      const resolver: (
        resource: RequiredResource,
      ) => Promise<Uint8Array | ArrayBufferView | null> =
        options.resolveResource
          ? async (r) => {
              return await options.resolveResource!(r, defaultResolver);
            }
          : defaultResolver;

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
                const data = await resolver({ ...r });
                if (
                  data &&
                  (data instanceof Uint8Array || ArrayBuffer.isView(data))
                ) {
                  const uint8 =
                    data instanceof Uint8Array
                      ? data
                      : new Uint8Array(
                          (data as ArrayBufferView).buffer,
                          (data as ArrayBufferView).byteOffset,
                          (data as ArrayBufferView).byteLength,
                        );
                  let typeInt = 1;
                  if (r.type === "image") typeInt = 2;
                  if (r.type === "css") typeInt = 3;
                  mod.add_resource(instancePtr, r.url, typeInt, uint8);
                }
              } catch (e) {
                console.warn(`Failed to resolve resource: ${r.url}`, e);
              }
            }),
          );
        }

        resolvedUrls.forEach((url) => {
          const escapedUrl = url.replace(/[.*+?^${}()|[\]]/g, "\\$&");
          const linkRegex = new RegExp(
            `<link[^>]*href\\s*=\\s*(["'])${escapedUrl}\\1[^>]*>`,
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
      mod.destroy_instance(instancePtr);
      mod.logLevel = prevLogLevel;
      mod.onLog = prevOnLog;
    }
  }
}
