import {
  SatoruBase,
  RequiredResource,
  ResolvedFontResult,
  resolveGoogleFonts,
  RenderOptions,
} from "./core.js";

export * from "./core.js";
export * from "./log-level.js";
export type { SatoruWorker } from "./child-workers.js";

export class Satoru extends SatoruBase {
  static async create(createSatoruModuleFunc: any): Promise<Satoru> {
    return new Satoru(createSatoruModuleFunc);
  }

  /**
   * Captures an HTMLElement and returns an HTMLCanvasElement.
   * Similar to html2canvas.
   */
  async capture(
    element: HTMLElement,
    options: Partial<Omit<RenderOptions, "value" | "url">> = {},
  ): Promise<HTMLCanvasElement> {
    const rect = element.getBoundingClientRect();
    const width = options.width ?? Math.ceil(rect.width);
    const height = options.height ?? Math.ceil(rect.height);

    // 1. Serialize DOM with computed styles
    const serializedHtml = this.serializeElement(element);

    // 2. Render to PNG
    const pngData = await this.render({
      ...options,
      value: serializedHtml,
      width,
      height,
      format: "png",
    });

    // 3. Convert PNG to Canvas
    return this.pngToCanvas(pngData as Uint8Array, width, height);
  }

  private serializeElement(element: HTMLElement): string {
    const clone = element.cloneNode(true) as HTMLElement;

    const absoluteUrl = (url: string) => {
      if (!url || url.startsWith("data:") || url.startsWith("blob:"))
        return url;
      const a = document.createElement("a");
      a.href = url;
      return a.href;
    };

    const applyStyles = (src: HTMLElement, dest: HTMLElement) => {
      const computed = window.getComputedStyle(src);
      const styleArr: string[] = [];
      for (let i = 0; i < computed.length; i++) {
        const prop = computed[i];
        let value = computed.getPropertyValue(prop);

        // Convert relative URLs in styles (e.g., background-image)
        if (value.includes("url(")) {
          value = value.replace(/url\(['"]?([^'"]+)['"]?\)/g, (match, url) => {
            return `url("${absoluteUrl(url)}")`;
          });
        }
        styleArr.push(`${prop}:${value}`);
      }
      dest.setAttribute("style", styleArr.join(";"));

      // Handle pseudo-elements
      const handlePseudo = (pseudoType: "::before" | "::after") => {
        const style = window.getComputedStyle(src, pseudoType);
        const content = style.getPropertyValue("content");
        if (!content || content === "none" || content === "normal") return;

        const pseudoEl = document.createElement("span");
        const pseudoStyles: string[] = [];
        for (let i = 0; i < style.length; i++) {
          const prop = style[i];
          pseudoStyles.push(`${prop}:${style.getPropertyValue(prop)}`);
        }
        pseudoEl.setAttribute("style", pseudoStyles.join(";"));
        // Remove quotes from content for the innerText if it's just a string
        if (content.startsWith('"') && content.endsWith('"')) {
          pseudoEl.innerText = content.slice(1, -1);
        }

        if (pseudoType === "::before") {
          dest.prepend(pseudoEl);
        } else {
          dest.append(pseudoEl);
        }
      };

      handlePseudo("::before");
      handlePseudo("::after");

      // Special handling for specific elements
      if (typeof HTMLCanvasElement !== "undefined" && src instanceof HTMLCanvasElement) {
        const img = document.createElement("img");
        img.src = src.toDataURL();
        img.setAttribute("style", dest.getAttribute("style") || "");
        dest.replaceWith(img);
      } else if (typeof HTMLImageElement !== "undefined" && src instanceof HTMLImageElement) {
        dest.setAttribute("src", absoluteUrl(src.src));
      } else if (typeof HTMLInputElement !== "undefined" && src instanceof HTMLInputElement) {
        if (src.type === "checkbox" || src.type === "radio") {
          if (src.checked) dest.setAttribute("checked", "");
        } else {
          dest.setAttribute("value", src.value);
        }
      } else if (typeof HTMLTextAreaElement !== "undefined" && src instanceof HTMLTextAreaElement) {
        dest.innerText = src.value;
      } else if (typeof HTMLSelectElement !== "undefined" && src instanceof HTMLSelectElement) {
        // Find the selected option and mark it
        const index = src.selectedIndex;
        const clonedSelect = dest as HTMLSelectElement;
        if (clonedSelect.options[index]) {
          clonedSelect.options[index].setAttribute("selected", "");
        }
      }

      for (let i = 0; i < src.children.length; i++) {
        const srcChild = src.children[i] as HTMLElement;
        const destChild = dest.children[i] as HTMLElement;
        if (srcChild && destChild) {
          applyStyles(srcChild, destChild);
        }
      }
    };

    applyStyles(element, clone);

    return `<!DOCTYPE html><html><body style="margin:0;padding:0;overflow:hidden;background:transparent;">${clone.outerHTML}</body></html>`;
  }

  private async pngToCanvas(
    data: Uint8Array,
    width: number,
    height: number,
  ): Promise<HTMLCanvasElement> {
    const blob = new Blob([data as any], { type: "image/png" });
    const url = URL.createObjectURL(blob);
    try {
      const img = new Image();
      img.crossOrigin = "anonymous";
      await new Promise((resolve, reject) => {
        img.onload = resolve;
        img.onerror = reject;
        img.src = url;
      });
      const canvas = document.createElement("canvas");
      canvas.width = width;
      canvas.height = height;
      const ctx = canvas.getContext("2d");
      if (ctx) {
        ctx.drawImage(img, 0, 0);
      }
      return canvas;
    } finally {
      URL.revokeObjectURL(url);
    }
  }

  /**
   * Overrides render to support HTMLElement as value.
   */
  override async render(
    options: RenderOptions & { format: "png" | "webp" | "pdf" },
  ): Promise<Uint8Array>;
  override async render(
    options: RenderOptions & { format?: "svg" },
  ): Promise<string>;
  override async render(options: RenderOptions): Promise<string | Uint8Array>;
  override async render(options: RenderOptions): Promise<string | Uint8Array> {
    if (options.value) {
      const values = Array.isArray(options.value)
        ? options.value
        : [options.value];
      const processedValues: string[] = [];
      let hasElement = false;

      for (const val of values) {
        if (typeof HTMLElement !== "undefined" && val instanceof HTMLElement) {
          processedValues.push(this.serializeElement(val));
          hasElement = true;
        } else {
          processedValues.push(val);
        }
      }

      if (hasElement) {
        options = {
          ...options,
          value: Array.isArray(options.value)
            ? processedValues
            : processedValues[0],
        };
      }
    }

    return super.render(options);
  }

  static async defaultResourceResolver(
    resource: RequiredResource,
    baseUrl?: string,
    userAgent?: string,
  ): Promise<Uint8Array | ResolvedFontResult | null> {
    try {
      if (resource.url.startsWith("provider:google-fonts")) {
        return resolveGoogleFonts(resource, userAgent);
      }

      const isAbsolute = /^[a-z][a-z0-9+.-]*:/i.test(resource.url);
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
  ): Promise<Uint8Array | ResolvedFontResult | null> {
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
