// @ts-ignore
import createSatoruModule from "../dist/satoru.js";

export enum LogLevel {
  Debug = 0,
  Info = 1,
  Warning = 2,
  Error = 3,
}

export interface SatoruModule {
  _init_engine: () => void;
  _html_to_svg: (html: number, width: number, height: number) => number;
  _html_to_png: (html: number, width: number, height: number) => number;
  _html_to_png_binary: (html: number, width: number, height: number) => number;
  _get_png_size: () => number;
  _html_to_pdf_binary: (html: number, width: number, height: number) => number;
  _get_pdf_size: () => number;
  _collect_resources: (html: number, width: number) => number;
  _add_resource: (url: number, type: number, data: number, size: number) => void;
  _scan_css: (css: number) => void;
  _load_font: (name: number, data: number, size: number) => void;
  _clear_fonts: () => void;
  _load_image: (name: number, url: number, width: number, height: number) => void;
  _clear_images: () => void;
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
}

export interface RequiredResource {
  type: "font" | "css" | "image";
  url: string;
  name: string;
}

export type ResourceResolver = (resource: RequiredResource) => Promise<Uint8Array | null>;

export interface SatoruOptions {
  locateFile?: (path: string) => string;
  instantiateWasm?: (imports: any, successCallback: any) => any;
  onLog?: (level: LogLevel, message: string) => void;
}

export { createSatoruModule };

export class Satoru {
  protected mod: SatoruModule;

  protected constructor(mod: SatoruModule) {
    this.mod = mod;
  }

  static async init(createSatoruModuleFunc: any = createSatoruModule, options: SatoruOptions = {}): Promise<Satoru> {
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

    const onLog = (level: LogLevel, message: string) => {
        if (options.onLog) {
            options.onLog(level, message);
        } else {
            defaultOnLog(level, message);
        }
    };

    const mod = await createSatoruModuleFunc({
      onLog,
      print: (text: string) => {
          onLog(LogLevel.Info, text);
      },
      printErr: (text: string) => {
          onLog(LogLevel.Error, text);
      },
      ...options
    });
    
    // Ensure onLog is available on the module instance
    mod.onLog = onLog;
    
    mod._init_engine();
    return new Satoru(mod);
  }

  loadFont(name: string, data: Uint8Array) {
    const namePtr = this.stringToPtr(name);
    const dataPtr = this.mod._malloc(data.length);
    this.mod.HEAPU8.set(data, dataPtr);
    this.mod._load_font(namePtr, dataPtr, data.length);
    this.mod._free(namePtr);
    this.mod._free(dataPtr);
  }

  clearFonts() {
    this.mod._clear_fonts();
  }

  loadImage(name: string, dataUrl: string, width: number, height: number) {
    const namePtr = this.stringToPtr(name);
    const urlPtr = this.stringToPtr(dataUrl);
    this.mod._load_image(namePtr, urlPtr, width, height);
    this.mod._free(namePtr);
    this.mod._free(urlPtr);
  }

  clearImages() {
    this.mod._clear_images();
  }

  async render(
    html: string,
    width: number,
    options: {
      height?: number;
      format?: "svg" | "png" | "pdf" | "dataurl";
      resolveResource?: ResourceResolver;
    } = {}
  ): Promise<string | Uint8Array | null> {
    const { height = 0, format = "svg", resolveResource } = options;

    let processedHtml = html;
    const resolvedUrls = new Set<string>();

    // Increased loop count to 10 to support multi-pass resource resolution (CSS -> Font Segments)
    for (let i = 0; i < 10; i++) {
      const resources = this.getPendingResources(processedHtml, width);
      const pending = resources.filter(r => !resolvedUrls.has(r.url));
      
      if (pending.length === 0) break;

      await Promise.all(
        pending.map(async (r) => {
          try {
            resolvedUrls.add(r.url);
            let data: Uint8Array | null = null;

            if (r.url.startsWith("data:")) {
              const commaIndex = r.url.indexOf(",");
              if (commaIndex !== -1) {
                const metadata = r.url.substring(0, commaIndex);
                const base64 = r.url.substring(commaIndex + 1).replace(/\s/g, "");
                if (metadata.includes(";base64")) {
                  const bin = atob(base64);
                  data = new Uint8Array(bin.length);
                  for (let j = 0; j < bin.length; j++) {
                    data[j] = bin.charCodeAt(j);
                  }
                } else {
                  const decoded = decodeURIComponent(base64);
                  data = new TextEncoder().encode(decoded);
                }
              }
            } else if (resolveResource) {
              data = await resolveResource({ ...r });
            }
            
            if (data instanceof Uint8Array) {
              this.addResource(r.url, r.type, data);
            }
          } catch (e) {
            console.warn(`Failed to resolve resource: ${r.url}`, e);
          }
        })
      );
    }

    // After all resources are resolved, we remove the link tags to prevent litehtml 
    // from attempting to fetch them again during the final layout/drawing pass.
    resolvedUrls.forEach(url => {
      // Escape special regex characters in the URL
      const escapedUrl = url.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
      const linkRegex = new RegExp(`<link[^>]*href=["']${escapedUrl}["'][^>]*>`, 'gi');
      processedHtml = processedHtml.replace(linkRegex, "");
    });

    switch (format) {
      case "png":
        return this.toPngBinary(processedHtml, width, height);
      case "pdf":
        return this.toPdf(processedHtml, width, height);
      case "dataurl":
        return this.toPngDataUrl(processedHtml, width, height);
      default:
        return this.toSvg(processedHtml, width, height);
    }
  }

  toSvg(html: string, width: number, height: number = 0): string {
    const htmlPtr = this.stringToPtr(html);
    const svgPtr = this.mod._html_to_svg(htmlPtr, width, height);
    const svg = this.mod.UTF8ToString(svgPtr);
    this.mod._free(htmlPtr);
    this.mod._free(svgPtr); // CRITICAL: Free the string returned by malloc in C++
    return svg;
  }

  toPngBinary(html: string, width: number, height: number = 0): Uint8Array | null {
    const htmlPtr = this.stringToPtr(html);
    const pngPtr = this.mod._html_to_png_binary(htmlPtr, width, height);
    const size = this.mod._get_png_size();
    
    let result: Uint8Array | null = null;
    if (pngPtr && size > 0) {
      result = new Uint8Array(this.mod.HEAPU8.buffer, pngPtr, size).slice();
    }

    this.mod._free(htmlPtr);
    // Note: pngPtr is managed inside g_context in C++, no need to free here
    return result;
  }

  toPngDataUrl(html: string, width: number, height: number = 0): string {
    const htmlPtr = this.stringToPtr(html);
    const ptr = this.mod._html_to_png(htmlPtr, width, height);
    const dataUrl = this.mod.UTF8ToString(ptr);
    this.mod._free(htmlPtr);
    this.mod._free(ptr); // CRITICAL: Free the string returned by malloc in C++
    return dataUrl;
  }

  toPdf(html: string, width: number, height: number = 0): Uint8Array | null {
    const htmlPtr = this.stringToPtr(html);
    const pdfPtr = this.mod._html_to_pdf_binary(htmlPtr, width, height);
    const size = this.mod._get_pdf_size();
    
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
    const ptr = this.mod._collect_resources(htmlPtr, width);
    const resultStr = this.mod.UTF8ToString(ptr);
    this.mod._free(htmlPtr);
    this.mod._free(ptr); // CRITICAL: Free the string returned by malloc in C++

    if (!resultStr) return [];

    return resultStr.split(";;").map((part: string) => {
      const [url, typeStr, name] = part.split("|");
      const typeInt = parseInt(typeStr, 10);
      let type: "font" | "css" | "image" = "font";
      if (typeInt === 2) type = "image";
      if (typeInt === 3) type = "css";
      
      return { type, name: name || "", url: url || "" };
    });
  }

  addResource(url: string, type: "font" | "css" | "image", data: Uint8Array) {
      const urlPtr = this.stringToPtr(url);
      const dataPtr = this.mod._malloc(data.length);
      this.mod.HEAPU8.set(data, dataPtr);
      
      let typeInt = 1;
      if (type === "image") typeInt = 2;
      if (type === "css") typeInt = 3;
      
      this.mod._add_resource(urlPtr, typeInt, dataPtr, data.length);
      
      this.mod._free(urlPtr);
      this.mod._free(dataPtr);
  }

  scanCss(css: string) {
    const cssPtr = this.stringToPtr(css);
    this.mod._scan_css(cssPtr);
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