import { LogLevel } from "./log-level.js";

export interface SatoruModule {
  create_instance: () => any;
  destroy_instance: (inst: any) => void;
  collect_resources: (
    inst: any,
    html: string,
    width: number,
    height: number,
  ) => void;
  get_pending_resources: (inst: any) => string;
  add_resource: (
    inst: any,
    url: string,
    type: number,
    data: Uint8Array,
  ) => void;
  scan_css: (inst: any, css: string) => void;
  load_font: (inst: any, name: string, data: Uint8Array) => void;
  load_fallback_font: (inst: any, data: Uint8Array) => void;
  load_image: (
    inst: any,
    name: string,
    url: string,
    width: number,
    height: number,
  ) => void;
  load_image_pixels: (
    inst: any,
    name: string,
    width: number,
    height: number,
    pixels: Uint8Array,
    data_url: string,
  ) => void;
  set_font_map: (inst: any, fontMap: Record<string, string>) => void;
  set_log_level: (level: number) => void;
  init_document: (
    inst: any,
    html: string,
    width: number,
    height: number,
  ) => void;
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
  merge_pdfs: (inst: any, pdfs: Uint8Array[]) => Uint8Array | null;
  onLog?: (level: LogLevel, message: string) => void;
  logLevel: LogLevel;
}

export interface RequiredResource {
  type: "font" | "css" | "image";
  url: string;
  name: string;
  characters?: string;
  redraw_on_ready?: boolean;
}

export interface ResolvedFontResult {
  css: Uint8Array;
  fonts: { url: string; data: Uint8Array }[];
}

export type ResourceResolver = (
  resource: RequiredResource,
  defaultResolver: (resource: RequiredResource) => Promise<Uint8Array | ResolvedFontResult | null>,
) => Promise<Uint8Array | ArrayBufferView | ResolvedFontResult | null>;

export interface RenderOptions {
  value?: string | string[] | any | any[];
  url?: string;
  width: number;
  height?: number;
  format?: "svg" | "png" | "webp" | "pdf";
  textToPaths?: boolean;
  resolveResource?: ResourceResolver;
  fonts?: { name: string; data: Uint8Array }[];
  fallbackFonts?: Uint8Array[];
  images?: { name: string; url: string; width?: number; height?: number }[];
  css?: string;
  baseUrl?: string;
  userAgent?: string;
  fontMap?: Record<string, string>;
  logLevel?: LogLevel;
  onLog?: (level: LogLevel, message: string) => void;
}

export const DEFAULT_FONT_MAP: Record<string, string> = {
  "sans-serif": "https://fonts.googleapis.com/css2?family=Noto+Sans+JP",
  serif: "https://fonts.googleapis.com/css2?family=Noto+Serif+JP",
  monospace: "https://fonts.googleapis.com/css2?family=M+PLUS+1+Code",
  cursive: "https://fonts.googleapis.com/css2?family=Yuji+Syuku",
  fantasy: "https://fonts.googleapis.com/css2?family=Reggae+One",
  emoji:
    "https://cdn.jsdelivr.net/npm/@fontsource/noto-color-emoji/files/noto-color-emoji-emoji-400-normal.woff2",
  "Noto Color Emoji":
    "https://cdn.jsdelivr.net/npm/@fontsource/noto-color-emoji/files/noto-color-emoji-emoji-400-normal.woff2",
};

/**
 * Parse unicode-range string into an array of [start, end] codepoint ranges.
 * e.g. "U+0000-00FF, U+0131" → [[0x0000, 0x00FF], [0x0131, 0x0131]]
 */
function parseUnicodeRanges(rangeStr: string): [number, number][] {
  const ranges: [number, number][] = [];
  for (const part of rangeStr.split(",")) {
    const m = part.trim().match(/U\+([0-9A-Fa-f?]+)(?:-([0-9A-Fa-f]+))?/);
    if (!m) continue;
    if (m[1].includes("?")) {
      // Wildcard range: U+4?? means U+400-U+4FF
      const lo = parseInt(m[1].replace(/\?/g, "0"), 16);
      const hi = parseInt(m[1].replace(/\?/g, "F"), 16);
      ranges.push([lo, hi]);
    } else {
      const start = parseInt(m[1], 16);
      const end = m[2] ? parseInt(m[2], 16) : start;
      ranges.push([start, end]);
    }
  }
  return ranges;
}

/**
 * Check if any character's codepoint falls within the given unicode ranges.
 */
function hasMatchingCodepoint(
  ranges: [number, number][],
  characters: string,
): boolean {
  for (let i = 0; i < characters.length; i++) {
    const cp = characters.codePointAt(i)!;
    if (cp > 0xffff) i++; // skip surrogate pair
    for (const [start, end] of ranges) {
      if (cp >= start && cp <= end) return true;
    }
  }
  return false;
}

/**
 * Parse @font-face blocks from CSS text, extracting src url and unicode-range.
 */
function parseFontFaceBlocks(
  cssText: string,
): { url: string; unicodeRange: string | null }[] {
  const blocks: { url: string; unicodeRange: string | null }[] = [];
  const blockRegex = /@font-face\s*\{([^}]+)\}/g;
  let blockMatch: RegExpExecArray | null;
  while ((blockMatch = blockRegex.exec(cssText)) !== null) {
    const body = blockMatch[1];
    const srcMatch = body.match(/src:\s*url\(([^)]+)\)/);
    const rangeMatch = body.match(/unicode-range:\s*([^;]+)/);
    if (srcMatch) {
      const url = srcMatch[1].replace(/['"]/g, "").trim();
      blocks.push({
        url,
        unicodeRange: rangeMatch ? rangeMatch[1].trim() : null,
      });
    }
  }
  return blocks;
}

export async function resolveGoogleFonts(
  resource: RequiredResource,
  userAgent?: string,
): Promise<ResolvedFontResult | Uint8Array | null> {
  if (!resource.url.startsWith("provider:google-fonts")) return null;

  const urlObj = new URL(resource.url);
  const family = urlObj.searchParams.get("family");
  if (!family) return null;

  const weight = urlObj.searchParams.get("weight") || "400";
  const italic = urlObj.searchParams.get("italic") === "1";
  const text = urlObj.searchParams.get("text") || resource.characters;

  let targetFamily = family;
  let forceNormalStyle = false;

  if (
    targetFamily.includes("Noto Sans JP") ||
    targetFamily.includes("Noto Serif JP") ||
    targetFamily.includes("CJK")
  ) {
    forceNormalStyle = true;
  }

  const useItalic = italic && !forceNormalStyle;

  let googleFontUrl = `https://fonts.googleapis.com/css2?family=${encodeURIComponent(
    targetFamily,
  )}:ital,wght@${useItalic ? "1" : "0"},${weight}&display=swap`;

  if (text) {
    if (text.length < 800) {
      googleFontUrl += `&text=${encodeURIComponent(text)}`;
    }
  }

  const headers: Record<string, string> = {};
  if (userAgent) {
    headers["User-Agent"] = userAgent;
  }
  try {
    const resp = await fetch(googleFontUrl, { headers });
    if (!resp.ok) return null;
    const cssText = await resp.text();
    const cssBuf = new TextEncoder().encode(cssText);

    // Parse @font-face blocks with their unicode-range
    const blocks = parseFontFaceBlocks(cssText);

    if (blocks.length === 0) {
      return cssBuf;
    }

    // Filter blocks by unicode-range if characters are available
    let filteredBlocks = blocks;
    if (text && text.length > 0) {
      filteredBlocks = blocks.filter((block) => {
        if (!block.unicodeRange) return true; // no range = always include
        const ranges = parseUnicodeRanges(block.unicodeRange);
        return hasMatchingCodepoint(ranges, text);
      });
    }

    if (filteredBlocks.length === 0) {
      // No matching subsets - still return CSS for C++ to parse
      return cssBuf;
    }

    // Prefetch only the matching font binaries in parallel
    const fontResults = await Promise.all(
      filteredBlocks.map(async (block) => {
        try {
          const fontResp = await fetch(block.url, { headers });
          if (!fontResp.ok) return null;
          const buf = await fontResp.arrayBuffer();
          return { url: block.url, data: new Uint8Array(buf) };
        } catch {
          return null;
        }
      }),
    );

    const fonts: { url: string; data: Uint8Array }[] = [];
    for (const f of fontResults) {
      if (f !== null) fonts.push(f);
    }

    return { css: cssBuf, fonts };
  } catch {
    return null;
  }
}

export abstract class SatoruBase {
  private factory: any;
  private modPromise?: Promise<SatoruModule>;
  protected currentFontMap: Record<string, string> = DEFAULT_FONT_MAP;
  private resourceCache = new Map<string, Uint8Array | ResolvedFontResult>();

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

        const originalSetLogLevel = mod.set_log_level;
        mod.set_log_level = (level: number) => {
          currentLogLevel = level;
          originalSetLogLevel(level);
        };

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
    mod.init_document(inst, options.html, options.width, options.height ?? 0);
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

  async loadFallbackFont(data: Uint8Array): Promise<void> {
    const mod = await this.getModule();
    const inst = mod.create_instance();
    try {
      mod.load_fallback_font(inst, data);
    } finally {
      mod.destroy_instance(inst);
    }
  }

  protected abstract resolveDefaultResource(
    resource: RequiredResource,
    baseUrl?: string,
    userAgent?: string,
  ): Promise<Uint8Array | ResolvedFontResult | null>;

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
    let { format = "svg", value, url, baseUrl } = options;

    if (format === "pdf" && Array.isArray(value) && value.length > 1) {
      const pagePdfs: Uint8Array[] = [];
      const SatoruClass = this.constructor as any;

      for (const html of value) {
        const engine = await SatoruClass.create();
        const pagePdf = await engine.render({
          ...options,
          value: html,
          format: "pdf",
        });
        pagePdfs.push(pagePdf);
      }
      
      const mod = await this.getModule();
      const instancePtr = mod.create_instance();
      try {
        const result = mod.merge_pdfs(instancePtr, pagePdfs);
        if (!result) return new Uint8Array();
        return new Uint8Array(result);
      } finally {
        mod.destroy_instance(instancePtr);
      }
    }

    let mod = await this.getModule();
    const {
      width,
      height = 0,
      fonts,
      images,
      css,
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
    const prevFontMap = this.currentFontMap;

    mod.logLevel = logLevel ?? LogLevel.None;
    mod.set_log_level(mod.logLevel);
    mod.onLog = onLog;
    this.currentFontMap = options.fontMap ?? DEFAULT_FONT_MAP;

    const instancePtr = mod.create_instance();
    mod.set_font_map(instancePtr, this.currentFontMap);

    try {
      if (fonts) {
        for (const f of fonts) {
          mod.load_font(instancePtr, f.name, f.data);
        }
      }
      if (options.fallbackFonts) {
        for (const data of options.fallbackFonts) {
          mod.load_fallback_font(instancePtr, data);
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
      ) => Promise<Uint8Array | ArrayBufferView | ResolvedFontResult | null> =
        options.resolveResource
          ? async (r) => {
              return await options.resolveResource!(r, defaultResolver);
            }
          : defaultResolver;

      const cachedResolver = async (
        r: RequiredResource,
      ): Promise<Uint8Array | ArrayBufferView | ResolvedFontResult | null> => {
        const cacheKey = `${r.type}:${r.url}:${r.characters ?? ""}`;
        const cached = this.resourceCache.get(cacheKey);
        if (cached) return cached;
        const result = await resolver(r);
        if (result) {
          if (result instanceof Uint8Array) {
            this.resourceCache.set(cacheKey, result);
          } else if (
            "css" in (result as any) &&
            "fonts" in (result as any)
          ) {
            this.resourceCache.set(cacheKey, result as ResolvedFontResult);
          }
        }
        return result;
      };

      const loadResourceData = (
        r: RequiredResource,
        uint8: Uint8Array,
      ) => {
        if (
          r.type === "image" &&
          typeof createImageBitmap !== "undefined" &&
          typeof OffscreenCanvas !== "undefined"
        ) {
          return (async () => {
            try {
              const blob = new Blob([uint8.buffer as ArrayBuffer]);
              const bitmap = await createImageBitmap(blob);
              const canvas = new OffscreenCanvas(
                bitmap.width,
                bitmap.height,
              );
              const ctx = canvas.getContext("2d");
              if (ctx) {
                ctx.drawImage(bitmap, 0, 0);
                const imageData = ctx.getImageData(
                  0,
                  0,
                  bitmap.width,
                  bitmap.height,
                );
                mod.load_image_pixels(
                  instancePtr,
                  r.url,
                  bitmap.width,
                  bitmap.height,
                  new Uint8Array(imageData.data.buffer),
                  r.url,
                );
                return;
              }
            } catch (e) {
              // fall through
            }
            let typeInt = 1;
            if (r.type === "image") typeInt = 2;
            if (r.type === "css") typeInt = 3;
            mod.add_resource(instancePtr, r.url, typeInt, uint8);
          })();
        }
        let typeInt = 1;
        if (r.type === "image") typeInt = 2;
        if (r.type === "css") typeInt = 3;
        mod.add_resource(instancePtr, r.url, typeInt, uint8);
      };

      const inputHtmls = Array.isArray(value) ? value : [value];
      const processedHtmls: string[] = [];

      const resolvedResources = new Set<string>();

      for (const rawHtml of inputHtmls) {
        let processedHtml = rawHtml;
        for (let i = 0; i < 10; i++) {
          mod.collect_resources(instancePtr, processedHtml, width, height);

          const json = mod.get_pending_resources(instancePtr);
          if (!json) break;

          const resources = JSON.parse(json) as RequiredResource[];
          const pending = resources.filter((r) => {
            const key = `${r.type}:${r.url}:${r.characters ?? ""}`;
            return !resolvedResources.has(key);
          });
          if (pending.length === 0) break;

          await Promise.all(
            pending.map(async (r) => {
              try {
                if (r.url.startsWith("data:")) {
                  return;
                }
                const key = `${r.type}:${r.url}:${r.characters ?? ""}`;
                resolvedResources.add(key);

                const data = await cachedResolver({ ...r });
                if (!data) return;

                // Handle ResolvedFontResult (CSS + prefetched font binaries)
                if (
                  typeof data === "object" &&
                  "css" in data &&
                  "fonts" in data
                ) {
                  const fontResult = data as ResolvedFontResult;
                  // Load the CSS first so C++ can parse @font-face
                  mod.add_resource(
                    instancePtr,
                    r.url,
                    1, // Font type
                    fontResult.css,
                  );
                  // Then load all prefetched font binaries directly
                  for (const font of fontResult.fonts) {
                    const fontKey = `font:${font.url}:`;
                    resolvedResources.add(fontKey);
                    mod.add_resource(
                      instancePtr,
                      font.url,
                      1, // Font type
                      font.data,
                    );
                  }
                  return;
                }

                // Handle regular Uint8Array / ArrayBufferView
                if (
                  data instanceof Uint8Array ||
                  ArrayBuffer.isView(data)
                ) {
                  const uint8 =
                    data instanceof Uint8Array
                      ? data
                      : new Uint8Array(
                          (data as ArrayBufferView).buffer,
                          (data as ArrayBufferView).byteOffset,
                          (data as ArrayBufferView).byteLength,
                        );
                  await loadResourceData(r, uint8);
                }
              } catch (e) {
                console.warn(`Failed to resolve resource: ${r.url}`, e);
              }
            }),
          );
        }

        const resolvedUrls = new Set<string>();
        resolvedResources.forEach((key) => {
          const parts = key.split(":");
          if (parts.length >= 2) {
            resolvedUrls.add(parts.slice(1, -1).join(":"));
          }
        });

        resolvedUrls.forEach((url) => {
          const escapedUrl = url.replace(/[.*+?^${}()|[\]]/g, "\\$&");
          const linkRegex = new RegExp(
            `<link[^>]*href\\s*=\\s*(["']?)${escapedUrl}\\1[^>]*>`,
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
      this.currentFontMap = prevFontMap;
    }
  }
}
