// @ts-ignore
import createSatoruModule from "../dist/satoru.js";

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
  _html_to_svg: (
    inst: number,
    html: number,
    width: number,
    height: number,
  ) => number;
  _html_to_png: (
    inst: number,
    html: number,
    width: number,
    height: number,
  ) => number;
  _get_png_size: (inst: number) => number;
  _html_to_pdf: (
    inst: number,
    html: number,
    width: number,
    height: number,
  ) => number;
  _get_pdf_size: (inst: number) => number;
  _collect_resources: (inst: number, html: number, width: number) => void;
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
  onRequestResource?: (
    handle: number,
    url: string,
    type: number,
    name: string,
  ) => void;
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

export interface SatoruOptions {
  locateFile?: (path: string) => string;
  instantiateWasm?: (imports: any, successCallback: any) => any;
  onLog?: (level: LogLevel, message: string) => void;
}

export interface RenderOptions {
  html: string;
  width: number;
  height?: number;
  format?: "svg" | "png" | "pdf";
  resolveResource?: ResourceResolver;
  fonts?: { name: string; data: Uint8Array }[];
  images?: { name: string; url: string; width?: number; height?: number }[];
  css?: string;
  clear?: boolean;
  baseUrl?: string;
  logLevel?: LogLevel;
  removeDefaultMargin?: boolean;
}

export { createSatoruModule };

export class Satoru {
  protected mod: SatoruModule;
  protected instancePtr: number;
  private static instances = new Map<number, Satoru>();

  protected constructor(mod: SatoruModule, instancePtr: number) {
    this.mod = mod;
    this.instancePtr = instancePtr;
    Satoru.instances.set(instancePtr, this);

    if (!mod.onRequestResource) {
      mod.onRequestResource = (
        handle: number,
        url: string,
        typeInt: number,
        name: string,
      ) => {
        const inst = Satoru.instances.get(handle);
        if (inst && inst.onResourceRequested) {
          inst.onResourceRequested(url, typeInt, name);
        }
      };
    }
  }

  private onResourceRequested?: (
    url: string,
    type: number,
    name: string,
  ) => void;

  /**
   * Initialize Satoru.
   * @param createSatoruModuleFunc Factory function from satoru.js
   * @param options Initialization options
   * @param logLevel Default log level (defaults to None)
   */
  static async init(
    createSatoruModuleFunc: any = createSatoruModule,
    options: SatoruOptions = {},
    logLevel: LogLevel = LogLevel.None,
  ): Promise<Satoru> {
    const defaultOnLog = (level: LogLevel, message: string) => {
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
    };

    let mod: SatoruModule;
    const onLog = (level: LogLevel, message: string) => {
      // If module is not yet assigned or level is higher than current setting, skip
      // (level 0 = None, messages are 1-4, so level > None(0) is always true)
      const currentLevel = mod ? mod.logLevel : logLevel;
      if (level > currentLevel) return;

      if (options.onLog) {
        options.onLog(level, message);
      } else {
        defaultOnLog(level, message);
      }
    };

    mod = (await createSatoruModuleFunc({
      onLog,
      print: (text: string) => {
        onLog(LogLevel.Info, text);
      },
      printErr: (text: string) => {
        onLog(LogLevel.Error, text);
      },
      ...options,
    })) as SatoruModule;

    // Ensure onLog is available on the module instance
    mod.onLog = onLog;
    mod.logLevel = logLevel;

    const instancePtr = mod._create_instance();
    return new Satoru(mod, instancePtr);
  }

  destroy() {
    Satoru.instances.delete(this.instancePtr);
    this.mod._destroy_instance(this.instancePtr);
  }

  loadFont(name: string, data: Uint8Array) {
    const namePtr = this.stringToPtr(name);
    const dataPtr = this.mod._malloc(data.length);
    this.mod.HEAPU8.set(data, dataPtr);
    this.mod._load_font(this.instancePtr, namePtr, dataPtr, data.length);
    this.mod._free(namePtr);
    this.mod._free(dataPtr);
  }

  clearFonts() {
    this.mod._clear_fonts(this.instancePtr);
  }

  loadImage(name: string, dataUrl: string, width: number, height: number) {
    const namePtr = this.stringToPtr(name);
    const urlPtr = this.stringToPtr(dataUrl);
    this.mod._load_image(this.instancePtr, namePtr, urlPtr, width, height);
    this.mod._free(namePtr);
    this.mod._free(urlPtr);
  }

  clearImages() {
    this.mod._clear_images(this.instancePtr);
  }

  clearCss() {
    this.mod._clear_css(this.instancePtr);
  }

  clearAll() {
    this.clearFonts();
    this.clearImages();
    this.clearCss();
  }

  render(
    options: RenderOptions & { format: "png" | "pdf" },
  ): Promise<Uint8Array>;
  render(options: RenderOptions & { format?: "svg" }): Promise<string>;
  render(options: RenderOptions): Promise<string | Uint8Array>;
  async render(options: RenderOptions): Promise<string | Uint8Array> {
    const {
      html,
      width,
      height = 0,
      format = "svg",
      fonts,
      images,
      css,
      clear = false,
      baseUrl,
      logLevel,
      removeDefaultMargin = true,
    } = options;

    const prevLogLevel = this.mod.logLevel;
    if (logLevel !== undefined) {
      this.mod.logLevel = logLevel;
    }

    try {
      if (clear) {
        this.clearAll();
      }
      if (fonts) {
        for (const f of fonts) {
          this.loadFont(f.name, f.data);
        }
      }
      if (images) {
        for (const img of images) {
          this.loadImage(img.name, img.url, img.width ?? 0, img.height ?? 0);
        }
      }
      if (removeDefaultMargin) {
        this.scanCss("body { margin: 0; }");
      }
      if (css) {
        this.scanCss(css);
      }

      let { resolveResource } = options;

      // Provide a default resource resolver if baseUrl is present
      if (!resolveResource && baseUrl) {
        resolveResource = async (r: RequiredResource) => {
          try {
            const isAbsolute = /^[a-z][a-z0-9+.-]*:/i.test(r.url);

            // Node.js local file resolution for relative URLs
            if (
              !isAbsolute &&
              typeof process !== "undefined" &&
              process.versions?.node
            ) {
              try {
                const path = await import("path");
                const fs = await import("fs");
                // If baseUrl is a path or file:// URL
                let baseDir = baseUrl.startsWith("file://")
                  ? baseUrl.slice(7)
                  : baseUrl;

                // On Windows, slice(7) might leave a leading slash like /C:/...
                if (process.platform === "win32" && baseDir.startsWith("/")) {
                  baseDir = baseDir.slice(1);
                }

                // Check if it's NOT a protocol (http://, https://, etc.)
                // We assume it's a local path if it doesn't have "://" or "data:"
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
              // baseUrl must be an absolute URL (with protocol) if we reach here
              if (!/^[a-z][a-z0-9+.-]*:/i.test(baseUrl)) return null;
              finalUrl = new URL(r.url, baseUrl).href;
            }

            const resp = await fetch(finalUrl);
            if (!resp.ok) return null;

            const buf = await resp.arrayBuffer();
            return new Uint8Array(buf);
          } catch (e) {
            console.warn(`[Satoru] Failed to resolve resource: ${r.url}`, e);
            return null;
          }
        };
      }

      let processedHtml = html;
      const resolvedUrls = new Set<string>();

      // Increased loop count to 10 to support multi-pass resource resolution (CSS -> Font Segments)
      for (let i = 0; i < 10; i++) {
        const resources = this.getPendingResources(processedHtml, width);
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
                this.addResource(r.url, r.type, data);
              }
            } catch (e) {
              console.warn(`Failed to resolve resource: ${r.url}`, e);
            }
          }),
        );
      }

      // After all resources are resolved, we remove the link tags to prevent litehtml
      // from attempting to fetch them again during the final layout/drawing pass.
      resolvedUrls.forEach((url) => {
        // Escape special regex characters in the URL
        const escapedUrl = url.replace(/[.*+?^${}()|[\\\]]/g, "\\$&");
        const linkRegex = new RegExp(
          `<link[^>]*href\\s*=\\s*(["'])${escapedUrl}\\1[^>]*>`,
          "gi",
        );
        processedHtml = processedHtml.replace(linkRegex, "");
      });

      switch (format) {
        case "png":
          return this.toPng(processedHtml, width, height) || new Uint8Array();
        case "pdf":
          return this.toPdf(processedHtml, width, height) || new Uint8Array();
        default:
          return this.toSvg(processedHtml, width, height);
      }
    } finally {
      this.mod.logLevel = prevLogLevel;
    }
  }

  toSvg(html: string, width: number, height: number = 0): string {
    const htmlPtr = this.stringToPtr(html);
    const svgPtr = this.mod._html_to_svg(
      this.instancePtr,
      htmlPtr,
      width,
      height,
    );
    const svg = this.mod.UTF8ToString(svgPtr);
    this.mod._free(htmlPtr);
    this.mod._free(svgPtr); // CRITICAL: Free the string returned by malloc in C++
    return svg;
  }

  toPng(html: string, width: number, height: number = 0): Uint8Array | null {
    const htmlPtr = this.stringToPtr(html);
    const pngPtr = this.mod._html_to_png(
      this.instancePtr,
      htmlPtr,
      width,
      height,
    );
    const size = this.mod._get_png_size(this.instancePtr);

    let result: Uint8Array | null = null;
    if (pngPtr && size > 0) {
      result = new Uint8Array(this.mod.HEAPU8.buffer, pngPtr, size).slice();
    }

    this.mod._free(htmlPtr);
    // Note: pngPtr is managed inside g_context in C++, no need to free here
    return result;
  }

  toPdf(html: string, width: number, height: number = 0): Uint8Array | null {
    const htmlPtr = this.stringToPtr(html);
    const pdfPtr = this.mod._html_to_pdf(
      this.instancePtr,
      htmlPtr,
      width,
      height,
    );
    const size = this.mod._get_pdf_size(this.instancePtr);

    let result: Uint8Array | null = null;
    if (pdfPtr && size > 0) {
      result = new Uint8Array(this.mod.HEAPU8.buffer, pdfPtr, size).slice();
    }

    this.mod._free(htmlPtr);
    // Note: pdfPtr is managed inside g_context in C++, no need to free here
    return result;
  }

  getRequiredResources(html: string, width: number): RequiredResource[] {
    return this.getPendingResources(html, width);
  }

  getPendingResources(html: string, width: number): RequiredResource[] {
    const htmlPtr = this.stringToPtr(html);
    const resources: RequiredResource[] = [];

    // Temporary callback to collect resources from WASM
    this.onResourceRequested = (url: string, typeInt: number, name: string) => {
      let type: "font" | "css" | "image" = "font";
      if (typeInt === 2) type = "image";
      if (typeInt === 3) type = "css";
      resources.push({ type, name, url });
    };

    this.mod._collect_resources(this.instancePtr, htmlPtr, width);

    // Cleanup
    this.mod._free(htmlPtr);
    this.onResourceRequested = undefined;

    return resources;
  }
  addResource(url: string, type: "font" | "css" | "image", data: Uint8Array) {
    const urlPtr = this.stringToPtr(url);
    const dataPtr = this.mod._malloc(data.length);
    this.mod.HEAPU8.set(data, dataPtr);

    let typeInt = 1;
    if (type === "image") typeInt = 2;
    if (type === "css") typeInt = 3;

    this.mod._add_resource(
      this.instancePtr,
      urlPtr,
      typeInt,
      dataPtr,
      data.length,
    );

    this.mod._free(urlPtr);
    this.mod._free(dataPtr);
  }

  scanCss(css: string) {
    const cssPtr = this.stringToPtr(css);
    this.mod._scan_css(this.instancePtr, cssPtr);
    this.mod._free(cssPtr);
  }

  /** @deprecated use getRequiredResources */
  getRequiredFonts(html: string, width: number): RequiredResource[] {
    return this.getRequiredResources(html, width);
  }

  private stringToPtr(str: string): number {
    const len = this.mod.lengthBytesUTF8(str) + 1;
    const ptr = this.mod._malloc(len);
    this.mod.stringToUTF8(str, ptr, len);
    return ptr;
  }
}

export type { SatoruWorker } from "./child-workers.js";
