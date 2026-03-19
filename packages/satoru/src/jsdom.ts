export interface HydrateOptions {
  /**
   * Target URL or raw HTML string to render.
   */
  src: string;

  /**
   * Base URL for resolving relative paths and fetching resources.
   * Recommended when passing raw HTML.
   */
  baseUrl?: string;

  /**
   * Wait condition for hydration to complete.
   * - number: Wait for specified milliseconds
   * - function: Polling function that receives the window object and returns true when done
   * - "networkidle": Waits until network requests settle (basic implementation)
   */
  waitUntil?:
    | number
    | ((window: any) => boolean | Promise<boolean>)
    | "networkidle";

  /**
   * Maximum time to wait in milliseconds. Default: 10000ms.
   */
  timeout?: number;

  /**
   * Callback to mock or inject missing browser APIs (e.g. matchMedia, IntersectionObserver)
   * before the HTML is parsed and scripts are executed.
   */
  beforeParse?: (window: any) => void | Promise<void>;

  /**
   * If true, forwards console.log/error from JSDOM to the Node.js console.
   */
  forwardConsole?: boolean;

  /**
   * If true, removes all <script> tags from the final HTML before returning.
   * Default: true.
   */
  removeScripts?: boolean;
}

/**
 * Hydrates an HTML string or URL using JSDOM and returns the final rendered HTML.
 * Note: Requires 'jsdom' to be installed as a peer dependency.
 */
export async function getHtml(options: HydrateOptions): Promise<string> {
  let jsdomModule: any;
  try {
    jsdomModule = await import("jsdom");
  } catch (e) {
    throw new Error(
      "[Satoru] 'jsdom' is required to use hydrateHtml. Please run: npm install jsdom",
    );
  }

  const { JSDOM, VirtualConsole } = jsdomModule.default || jsdomModule;

  const isUrl = /^https?:\/\//.test(options.src);
  const finalBaseUrl =
    options.baseUrl || (isUrl ? options.src : "http://localhost/");

  const virtualConsole = new VirtualConsole();
  if (options.forwardConsole) {
    virtualConsole.forwardTo(console);
  }

  const jsdomOptions: any = {
    runScripts: "dangerously",
    resources: "usable",
    pretendToBeVisual: true,
    url: finalBaseUrl,
    virtualConsole,
    beforeParse: (window: any) => {
      // Provide basic polyfills often needed by modern frameworks
      if (!window.fetch) {
        window.fetch = async (url: string, opts: any) => {
          const absoluteUrl = new URL(url, finalBaseUrl).href;
          return await globalThis.fetch(absoluteUrl, opts);
        };
      }

      window.matchMedia = window.matchMedia || (() => ({
        matches: false,
        addListener: () => {},
        removeListener: () => {},
        addEventListener: () => {},
        removeEventListener: () => {},
        dispatchEvent: () => false,
      }));

      window.IntersectionObserver = window.IntersectionObserver || class {
        observe() {}
        unobserve() {}
        disconnect() {}
      };

      window.ResizeObserver = window.ResizeObserver || class {
        observe() {}
        unobserve() {}
        disconnect() {}
      };

      if (options.beforeParse) {
        options.beforeParse(window);
      }
    },
  };

  let dom;
  if (isUrl) {
    const { url, ...fromUrlOptions } = jsdomOptions;
    dom = await JSDOM.fromURL(options.src, fromUrlOptions);
  } else {
    dom = new JSDOM(options.src, jsdomOptions);
  }

  const timeoutMs = options.timeout || 10000;
  const startTime = Date.now();

  const checkWaitCondition = async (): Promise<boolean> => {
    if (Date.now() - startTime >= timeoutMs) {
      return true; // Timeout reached
    }

    if (typeof options.waitUntil === "number") {
      return Date.now() - startTime >= options.waitUntil;
    }

    if (options.waitUntil === "networkidle") {
        // Wait at least 2 seconds for heavy sites like Zenn
        return Date.now() - startTime >= 2000;
    }

    if (typeof options.waitUntil === "function") {
      return await options.waitUntil(dom.window);
    }

    // Default: wait a short tick for basic sync scripts
    return Date.now() - startTime >= 50;
  };

  // Polling loop
  while (!(await checkWaitCondition())) {
    await new Promise((resolve) => setTimeout(resolve, 50));
  }

  const removeScripts = options.removeScripts ?? true;
  if (removeScripts) {
    const scripts = dom.window.document.querySelectorAll("script");
    scripts.forEach((s: any) => s.remove());
  }

  const finalHtml = dom.serialize();

  // Cleanup to prevent memory leaks
  if (dom.window) {
    dom.window.close();
  }

  return finalHtml;
}
