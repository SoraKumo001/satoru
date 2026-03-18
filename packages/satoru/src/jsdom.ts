export interface HydrateOptions {
  /**
   * Target URL or raw HTML string to render.
   */
  urlOrHtml: string;

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
  waitUntil?: number | ((window: any) => boolean | Promise<boolean>) | "networkidle";

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
export async function hydrateHtml(options: HydrateOptions): Promise<string> {
  let jsdomModule: any;
  try {
    jsdomModule = await import("jsdom");
  } catch (e) {
    throw new Error(
      "[Satoru] 'jsdom' is required to use hydrateHtml. Please run: npm install jsdom",
    );
  }

  const { JSDOM, ResourceLoader, VirtualConsole } = jsdomModule;

  const isUrl = /^https?:\/\//.test(options.urlOrHtml);
  const finalBaseUrl = options.baseUrl || (isUrl ? options.urlOrHtml : "http://localhost/");

  let activeRequests = 0;

  // Custom resource loader to track network idle state
  class TrackingResourceLoader extends ResourceLoader {
    constructor(opts?: any) {
      super(opts);
    }
    fetch(url: string, fetchOptions: any) {
      activeRequests++;
      const promise = super.fetch(url, fetchOptions);
      if (promise && promise.then) {
        promise
          .then(() => {
            activeRequests--;
          })
          .catch(() => {
            activeRequests--;
          });
      } else {
        activeRequests--;
      }
      return promise;
    }
  }

  const virtualConsole = new VirtualConsole();
  if (options.forwardConsole) {
    virtualConsole.sendTo(console);
  }

  const jsdomOptions: any = {
    runScripts: "dangerously",
    resources: options.waitUntil === "networkidle" ? new TrackingResourceLoader({ strictSSL: false }) : "usable",
    pretendToBeVisual: true,
    url: finalBaseUrl,
    virtualConsole,
    beforeParse: (window: any) => {
      // Provide basic polyfills often needed by modern frameworks if not present
      if (!window.fetch) {
        window.fetch = async (url: string, opts: any) => {
          const absoluteUrl = new URL(url, finalBaseUrl).href;
          activeRequests++;
          try {
            return await globalThis.fetch(absoluteUrl, opts);
          } finally {
            activeRequests--;
          }
        };
      }
      if (options.beforeParse) {
        options.beforeParse(window);
      }
    },
  };

  let dom;
  if (isUrl) {
    dom = await JSDOM.fromURL(options.urlOrHtml, jsdomOptions);
  } else {
    dom = new JSDOM(options.urlOrHtml, jsdomOptions);
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
      return activeRequests === 0;
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
