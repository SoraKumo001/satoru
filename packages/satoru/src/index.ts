// satoru.js and satoru.wasm are in the dist directory
// @ts-ignore
import createSatoruModule from "../dist/satoru.js";

// We don't import wasm here anymore to avoid Vite build errors.
// Host applications (like Cloudflare Workers or Vite apps) should handle WASM loading.

export { createSatoruModule };

export interface SatoruModule {
  _init_engine: () => void;
  _html_to_svg: (htmlPtr: number, width: number, height: number) => number;
  _html_to_png: (htmlPtr: number, width: number, height: number) => number;
  _html_to_png_binary: (htmlPtr: number, width: number, height: number) => number;
  _get_png_size: () => number;
  _get_required_fonts: (htmlPtr: number, width: number) => number;
  _collect_resources: (htmlPtr: number, width: number) => number;
  _add_resource: (urlPtr: number, type: number, dataPtr: number, size: number) => void;
  _scan_css: (cssPtr: number) => void;
  _load_font: (namePtr: number, dataPtr: number, size: number) => void;
  _clear_fonts: () => void;
  _load_image: (namePtr: number, dataUrlPtr: number, width: number, height: number) => void;
  _clear_images: () => void;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  UTF8ToString: (ptr: number) => string;
  stringToUTF8: (str: string, ptr: number, maxBytes: number) => void;
  lengthBytesUTF8: (str: string) => number;
  HEAPU8: Uint8Array;
}

export interface SatoruOptions {
  locateFile?: (url: string) => string;
  print?: (text: string) => void;
  printErr?: (text: string) => void;
  wasmBinary?: ArrayBuffer | WebAssembly.Module;
  instantiateWasm?: (imports: any, successCallback: any) => any;
  mainScriptUrlOrBlob?: string;
  noInitialRun?: boolean;
}

export interface RequiredResource {
  type: "font" | "css" | "image";
  name: string;
  url: string;
}

export type ResourceResolver = (resource: RequiredResource) => Promise<Uint8Array | string | null>;

export class Satoru {
  private module: SatoruModule | null = null;

  constructor(private createModule: (options?: any) => Promise<SatoruModule> = createSatoruModule) {}

  async init(options?: SatoruOptions) {
    this.module = await this.createModule({
      ...options,
    });
    this.module!._init_engine();
  }

  private get mod(): SatoruModule {
    if (!this.module) throw new Error("Satoru not initialized. Call init() first.");
    return this.module;
  }

  loadFont(name: string, data: Uint8Array) {
    const namePtr = this.stringToPtr(name);
    const dataPtr = this.mod._malloc(data.length);
    this.mod.HEAPU8.set(data, dataPtr);

    this.mod._load_font(namePtr, dataPtr, data.length);

    this.mod._free(namePtr);
    this.mod._free(dataPtr);
  }

  scanCss(css: string) {
    if (this.mod._scan_css) {
      const cssPtr = this.stringToPtr(css);
      this.mod._scan_css(cssPtr);
      this.mod._free(cssPtr);
    }
  }

  clearFonts() {
    this.mod._clear_fonts();
  }

  loadImage(name: string, dataUrl: string, width: number = 0, height: number = 0) {
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
      format?: "svg" | "png" | "dataurl";
      resolveResource?: ResourceResolver;
    } = {}
  ): Promise<string | Uint8Array | null> {
    const { height = 0, format = "svg", resolveResource } = options;

    let processedHtml = html;
    const resolvedUrls = new Set<string>();

    if (resolveResource) {
      for (let i = 0; i < 3; i++) {
        const resources = this.getPendingResources(processedHtml, width);
        const pending = resources.filter(r => !resolvedUrls.has(r.url));
        
        if (pending.length === 0) break;

        await Promise.all(
          pending.map(async (r) => {
            try {
              resolvedUrls.add(r.url);
              
              const isFontUrl = /\.(woff2?|ttf|otf|eot)(\?.*)?$/i.test(r.url);
              
              const data = await resolveResource({ ...r });
              
              if (r.type === "css" && typeof data === "string") {
                this.scanCss(data);
                processedHtml = `<style>${data}</style>\n` + processedHtml;
              } else if (data instanceof Uint8Array) {
                if (isFontUrl && r.type === "css") {
                   let fontName = r.url.split('/').pop()?.split('.')[0] || r.name;
                   if (r.url.includes("noto-sans-jp")) fontName = "Noto Sans JP";
                   
                   const fontFace = `@font-face { font-family: '${fontName}'; src: url('${r.url}'); }`;
                   this.scanCss(fontFace);
                   processedHtml = `<style>${fontFace}</style>\n` + processedHtml;
                   
                   this.loadFont(fontName, data); // Legacy load for synthesized font-face
                } else {
                   this.addResource(r.url, r.type, data);
                }
              }
              
              const escapedUrl = r.url.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
              const linkRegex = new RegExp(`<link[^>]+href=["']${escapedUrl}["'][^>]*>`, 'gi');
              processedHtml = processedHtml.replace(linkRegex, "");
            } catch (e) {
              console.warn(`Failed to resolve resource: ${r.url}`, e);
            }
          })
        );
      }
    }

    switch (format) {
      case "png":
        return this.toPngBinary(processedHtml, width, height);
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
    this.mod._free(svgPtr);
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
    return result;
  }

  toPngDataUrl(html: string, width: number, height: number = 0): string {
    const htmlPtr = this.stringToPtr(html);
    const ptr = this.mod._html_to_png(htmlPtr, width, height);
    const dataUrl = this.mod.UTF8ToString(ptr);
    this.mod._free(htmlPtr);
    this.mod._free(ptr);
    return dataUrl;
  }

  getRequiredResources(html: string, width: number): RequiredResource[] {
    return this.getPendingResources(html, width);
  }

  getPendingResources(html: string, width: number): RequiredResource[] {
    const htmlPtr = this.stringToPtr(html);
    const ptr = this.mod._collect_resources(htmlPtr, width);
    const resultStr = this.mod.UTF8ToString(ptr);
    this.mod._free(htmlPtr);
    this.mod._free(ptr); 

    if (!resultStr) return [];

    return resultStr.split(";;").map((part) => {
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