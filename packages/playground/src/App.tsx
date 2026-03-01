import React, { useState, useEffect, useRef, useMemo } from "react";
import {
  createSatoruWorker,
  LogLevel,
  DEFAULT_FONT_MAP,
} from "satoru-render/workers";

const satoru = createSatoruWorker({
  maxParallel: 1,
});

type Params = {
  asset?: string;
  width: number;
  height?: number;
  format: "svg" | "png" | "webp" | "pdf";
  textToPaths: boolean;
  value?: string | null;
};

const App: React.FC = () => {
  const [assetList] = useState<string[]>(() => {
    const assetFiles = import.meta.glob("../../../assets/*.html", {
      query: "?url",
      import: "default",
    });
    return Object.keys(assetFiles).map((path) => path.split("/").pop()!);
  });
  // Initialize state from URL parameters
  const [params, setParams] = useState<Params>(() => {
    const params = new URLSearchParams(window.location.search);
    const a = params.get("asset");
    const w = params.get("width");
    const h = params.get("height");
    const f = params.get("format");
    const t = params.get("textToPaths");
    const v = params.get("value");
    return {
      asset: !v ? a || assetList[0] : undefined,
      width: w ? parseInt(w) : 588,
      height: h ? parseInt(h) : undefined,
      format:
        f && ["svg", "png", "webp", "pdf"].includes(f) ? (f as any) : "svg",
      textToPaths: t !== null ? t === "true" : true,
      value: v,
    };
  });

  const [fontMapJson, setFontMapJson] = useState<string>(() => {
    const params = new URLSearchParams(window.location.search);
    const initialFontMap = params.get("fontMap");
    if (initialFontMap) {
      try {
        const parsed = JSON.parse(initialFontMap);
        return JSON.stringify(parsed, null, 2);
      } catch (e) {
        // ignore
      }
    }
    return JSON.stringify(DEFAULT_FONT_MAP, null, 2);
  });

  const [isRendering, setIsRendering] = useState<boolean>(false);
  const [renderResult, setRenderResult] = useState<string | Uint8Array | null>(
    null,
  );
  const [renderTime, setRenderTime] = useState<number | null>(null);
  const [objectUrl, setObjectUrl] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const iframeRef = useRef<HTMLIFrameElement>(null);
  const latestRenderId = useRef<number>(0);
  const shouldRenderImmediately = useRef<boolean>(false);

  // Initialize Satoru Worker
  useEffect(() => {
    // Handle browser back/forward
    const handlePopState = () => {
      const p = new URLSearchParams(window.location.search);
      const a = p.get("asset");
      const w = p.get("width");
      const h = p.get("height");
      const f = p.get("format");
      const t = p.get("textToPaths");

      setParams({
        asset: a ?? undefined,
        value: p.get("value"),
        width: w ? parseInt(w) : 588,
        height: h ? parseInt(h) : undefined,
        format:
          f && ["svg", "png", "webp", "pdf"].includes(f) ? (f as any) : "svg",
        textToPaths: t !== null ? t === "true" : true,
      });

      if (a && a !== params.asset) {
        loadAsset(a);
      }
    };

    window.addEventListener("popstate", handlePopState);

    return () => {
      satoru.close();
      window.removeEventListener("popstate", handlePopState);
    };
  }, [params]);
  const updateParams = (
    params: Record<string, string | number | boolean | undefined>,
  ) => {
    const url = new URL(location.href);
    const p = url.searchParams;
    Object.entries(params).forEach(([name, value]) => {
      if (value) p.set(name, String(value));
      else p.delete(name);
    });
    if (p.get("asset")) {
      p.delete("value");
    }

    const w = p.get("width");
    const h = p.get("height");
    const f = p.get("format");
    const t = p.get("textToPaths");
    const a = p.get("asset");
    history.replaceState({}, "", url.toString());
    setParams((p) => ({
      width: w ? parseInt(w) : 588,
      height: h ? parseInt(h) : undefined,
      format:
        f && ["svg", "png", "webp", "pdf"].includes(f) ? (f as any) : "svg",
      textToPaths: t !== null ? t === "true" : true,
      asset: a ?? undefined,
      value: params.value !== undefined ? (params.value as string) : p.value,
    }));
  };

  // Auto-run render when specific parameters change
  useEffect(() => {
    const t = setTimeout(handleConvert, 500);
    return () => clearTimeout(t);
  }, [params]);

  // Update iframe preview
  useEffect(() => {
    if (iframeRef.current) {
      const html = params.value ?? "";
      const doc =
        iframeRef.current.contentDocument ||
        iframeRef.current.contentWindow?.document;
      if (doc) {
        doc.open();
        const baseTag = `<base href="${window.location.origin}${window.location.pathname}assets/">`;
        // Hide scrollbars inside iframe
        const styleTag = `<style>body { overflow: hidden; } html { overflow: hidden; }</style>`;
        const htmlWithBase = html.includes("<head>")
          ? html.replace("<head>", `<head>${baseTag}${styleTag}`)
          : baseTag + styleTag + html;
        doc.write(htmlWithBase);
        doc.close();

        // Adjust height to fit content
        const resizeIframe = () => {
          if (iframeRef.current && iframeRef.current.contentWindow) {
            if (params.height) {
              iframeRef.current.style.height = `${params.height}px`;
              return;
            }
            const body = iframeRef.current.contentWindow.document.body;
            const docEl =
              iframeRef.current.contentWindow.document.documentElement;

            if (!body || !docEl) return;

            // Get the maximum height to ensure no scrollbars
            const heightValue = Math.max(
              body.scrollHeight,
              body.offsetHeight,
              docEl.clientHeight,
              docEl.scrollHeight,
              docEl.offsetHeight,
            );
            iframeRef.current.style.height = `${heightValue}px`;
          }
        };

        // Resize immediately and on load (for images)
        resizeIframe();
        iframeRef.current.onload = resizeIframe;
        // Also set a timeout to catch layout shifts
        setTimeout(resizeIframe, 100);
        setTimeout(resizeIframe, 500);
      }
    }
  }, [params]);

  // Handle asset selection (Initial or when assetList is loaded)
  useEffect(() => {
    if (assetList.length === 0 || !params.asset) return;

    const initialAsset =
      assetList.find((v) => v === params.asset) ?? assetList[0];
    loadAsset(initialAsset);
  }, []);
  const loadAsset = async (name: string) => {
    try {
      const resp = await fetch(`assets/${name}`);
      const text = await resp.text();
      shouldRenderImmediately.current = true;
      updateParams({ asset: name, value: text });
    } catch (e) {
      console.error("Failed to load asset", e);
    }
  };
  const property = useRef({ isRender: false });

  const handleConvert = async () => {
    if (property.current.isRender) return;
    console.log("start");
    const currentHtml = params.value;
    if (!satoru || !currentHtml) return;

    const requestId = ++latestRenderId.current;

    setIsRendering(true);
    setError(null);
    setRenderTime(null);

    if (objectUrl) {
      URL.revokeObjectURL(objectUrl);
      setObjectUrl(null);
    }

    const startTime = performance.now();
    try {
      let fontMap = DEFAULT_FONT_MAP;
      try {
        fontMap = JSON.parse(fontMapJson);
      } catch (e) {
        console.warn("Invalid fontMap JSON, using default");
      }

      console.log(`[Satoru] Rendering via Worker (ID: ${requestId})`);
      const { width, height, format, textToPaths } = params;
      const result = await satoru.render({
        value: currentHtml,
        width,
        height,
        format,
        textToPaths,
        fontMap,
        // logLevel: LogLevel.Info,
        onLog: (level, message) => {
          if (requestId !== latestRenderId.current) return;
          const prefix = "[Satoru Worker]";
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
        },
        css: "body { margin: 8px; }",
        baseUrl: `${window.location.origin}${window.location.pathname}assets/`,
        resolveResource: async (resource, defaultResolver) => {
          if (requestId !== latestRenderId.current) return new Uint8Array();
          console.log(
            `[Playground] Resolving: ${resource.url} (${resource.type})`,
          );
          // Open Cache storage
          const cache = await caches.open("satoru-resource-cache");
          const cachedResponse = await cache.match(resource.url);

          if (cachedResponse) {
            console.log(`[Satoru] Cache Hit: ${resource.url}`);
            const buf = await cachedResponse.arrayBuffer();
            return new Uint8Array(buf);
          }

          // Fetch using default resolver (or manual fetch if in worker proxy)
          const data = await defaultResolver(resource);
          if (data?.length)
            await cache.put(resource.url, new Response(data as BodyInit));

          return data;
        },
      });
      property.current.isRender = false;

      if (requestId !== latestRenderId.current) {
        console.log(`[Satoru] Render result discarded (ID: ${requestId})`);
        return;
      }

      const endTime = performance.now();
      setRenderTime(endTime - startTime);
      console.log("[Satoru] Render Complete");
      setRenderResult(result);

      if (result instanceof Uint8Array) {
        const mimeType =
          format === "png"
            ? "image/png"
            : format === "webp"
              ? "image/webp"
              : "application/pdf";
        const blob = new Blob([result.slice()], { type: mimeType });
        setObjectUrl(URL.createObjectURL(blob));
      }
    } catch (e: any) {
      if (requestId !== latestRenderId.current) return;
      console.error("Conversion failed:", e);
      setError(e.message);
    } finally {
      if (requestId === latestRenderId.current) {
        setIsRendering(false);
      }
    }
  };

  const outputSource = useMemo(() => {
    if (!renderResult) return "";
    if (typeof renderResult === "string") return renderResult;

    // Binary to Base64
    let binary = "";
    const len = renderResult.byteLength;
    for (let i = 0; i < len; i++) {
      binary += String.fromCharCode(renderResult[i]);
    }
    return btoa(binary);
  }, [renderResult]);

  const handleDownload = () => {
    if (!renderResult) return;
    const { format } = params;
    const mimeType =
      format === "svg"
        ? "image/svg+xml"
        : format === "png"
          ? "image/png"
          : format === "webp"
            ? "image/webp"
            : "application/pdf";
    const content =
      typeof renderResult === "string" ? renderResult : renderResult.slice();
    const blob = new Blob([content as BlobPart], { type: mimeType });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `rendered.${format}`;
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 100);
  };

  return (
    <div
      style={{
        padding: "20px",
        fontFamily: "sans-serif",
        maxWidth: "1200px",
        margin: "0 auto",
        background: "#fafafa",
        minHeight: "100vh",
      }}
    >
      <header
        style={{
          display: "flex",
          justifyContent: "space-between",
          alignItems: "center",
          borderBottom: "2px solid #2196F3",
          marginBottom: "20px",
          paddingBottom: "10px",
        }}
      >
        <h2 style={{ color: "#2196F3", margin: 0 }}>
          <a href="https://www.npmjs.com/package/satoru-render">
            Satoru Render
          </a>{" "}
          Playground (Multi-threaded)
        </h2>
      </header>

      <div style={{ display: "flex", gap: "20px", marginBottom: "20px" }}>
        <fieldset
          style={{
            flex: 1,
            padding: "15px",
            border: "1px solid #ddd",
            borderRadius: "8px",
            background: "white",
          }}
        >
          <legend style={{ fontWeight: "bold" }}>Rendering Options</legend>
          <div
            style={{ display: "flex", flexDirection: "column", gap: "10px" }}
          >
            <div style={{ display: "flex", gap: "10px", alignItems: "center" }}>
              <label>
                Width
                <div style={{ display: "flex" }}>
                  <input
                    type="number"
                    value={params.width}
                    onChange={(e) => {
                      updateParams({ width: parseInt(e.target.value) || 0 });
                    }}
                    style={{ width: "80px" }}
                  />
                </div>
              </label>

              <label>
                Height
                <div style={{ display: "flex" }}>
                  <input
                    type="number"
                    value={params.height ?? 0}
                    onChange={(e) => {
                      const val = e.target.value;
                      updateParams({
                        height: val === "" ? "auto" : parseInt(val),
                      });
                    }}
                    disabled={!params.height}
                    style={{ width: "80px" }}
                  />
                  <label
                    style={{
                      display: "flex",
                      alignItems: "center",
                      gap: "4px",
                    }}
                  >
                    <input
                      type="checkbox"
                      checked={!params.height}
                      onChange={(e) =>
                        updateParams(
                          e.target.checked
                            ? { height: undefined }
                            : { height: 800 },
                        )
                      }
                    />{" "}
                    Auto
                  </label>
                </div>
              </label>
            </div>
            <label>
              Format:{" "}
              <select
                value={params.format}
                onChange={(e) => updateParams({ format: e.target.value })}
                style={{ padding: "2px 5px", borderRadius: "4px" }}
              >
                <option value="svg">SVG (Vector)</option>
                <option value="png">PNG (Raster)</option>
                <option value="webp">WebP (Raster)</option>
                <option value="pdf">PDF (Document)</option>
              </select>
            </label>
            {params.format === "svg" && (
              <label>
                <input
                  type="checkbox"
                  checked={params.textToPaths}
                  onChange={(e) =>
                    updateParams({
                      textToPaths: e.target.checked || undefined,
                    })
                  }
                />{" "}
                Convert text to paths
              </label>
            )}
          </div>
        </fieldset>

        <fieldset
          style={{
            flex: 1,
            padding: "15px",
            border: "1px solid #ddd",
            borderRadius: "8px",
            background: "white",
          }}
        >
          <legend style={{ fontWeight: "bold" }}>Font Mapping (default)</legend>
          <textarea
            value={fontMapJson}
            onChange={(e) => setFontMapJson(e.target.value)}
            style={{
              width: "100%",
              height: "100px",
              fontFamily: "monospace",
              fontSize: "12px",
              padding: "5px",
              borderRadius: "4px",
              border: "1px solid #ccc",
              boxSizing: "border-box",
            }}
          />
        </fieldset>

        <fieldset
          style={{
            flex: 1,
            padding: "15px",
            border: "1px solid #ddd",
            borderRadius: "8px",
            background: "white",
          }}
        >
          <legend style={{ fontWeight: "bold" }}>Load Sample Assets</legend>
          <select
            value={params.asset}
            onChange={(e) => loadAsset(e.target.value)}
            style={{
              padding: "5px",
              borderRadius: "4px",
              border: "1px solid #ccc",
              width: "100%",
            }}
          >
            <option value="">-- Select Asset --</option>
            {assetList.map((asset) => (
              <option key={asset} value={asset}>
                {asset}
              </option>
            ))}
          </select>
        </fieldset>
      </div>

      <div style={{ marginBottom: "15px" }}>
        <h3>HTML Input:</h3>
        <textarea
          value={params.value ?? undefined}
          onChange={(e) => {
            updateParams({ value: e.target.value, asset: undefined });
          }}
          style={{
            width: "100%",
            height: "150px",
            fontFamily: "monospace",
            padding: "10px",
            borderRadius: "4px",
            border: "1px solid #ccc",
            boxSizing: "border-box",
          }}
        />
      </div>

      <button
        onClick={() => handleConvert()}
        disabled={isRendering}
        style={{
          padding: "15px 30px",
          fontSize: "20px",
          cursor: "pointer",
          background: isRendering ? "#ccc" : "#2196F3",
          color: "white",
          border: "none",
          borderRadius: "4px",
          width: "100%",
          fontWeight: "bold",
          marginBottom: "10px",
          boxShadow: "0 4px 6px rgba(33, 150, 243, 0.3)",
        }}
      >
        {isRendering ? "Processing..." : "Generate Output"}
      </button>

      <div
        style={{
          display: "flex",
          justifyContent: "flex-end",
          fontSize: "14px",
          color: "#666",
          marginBottom: "20px",
          minHeight: "20px",
        }}
      >
        {renderTime !== null && (
          <span>
            Render Time: <strong>{renderTime.toFixed(2)}ms</strong>
          </span>
        )}
      </div>

      {error && (
        <div
          style={{
            padding: "10px",
            background: "#ffebee",
            color: "#c62828",
            borderRadius: "4px",
            marginBottom: "20px",
          }}
        >
          {error}
        </div>
      )}

      <div
        style={{
          display: "grid",
          gridTemplateColumns: "repeat(2, minmax(0, 1fr))",
          gap: "20px",
          marginBottom: "20px",
        }}
      >
        <div>
          <h3>HTML Live Preview (iframe):</h3>
          <div
            style={{
              border: "1px solid #ccc",
              borderRadius: "4px",
              height: "800px",
              background: "white",
              overflowY: "auto",
              overflowX: "auto",
              boxSizing: "border-box",
            }}
          >
            <iframe
              ref={iframeRef}
              title="preview"
              style={{
                width: `${params.width}px`,
                minHeight: "100%",
                border: "none",
                display: "block",
              }}
            />
          </div>
        </div>

        <div>
          <div
            style={{
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center",
            }}
          >
            <h3>{params.format.toUpperCase()} Render Preview:</h3>
            {renderResult && (
              <button
                onClick={handleDownload}
                style={{
                  background: "#FF9800",
                  color: "white",
                  border: "none",
                  padding: "6px 15px",
                  cursor: "pointer",
                  borderRadius: "4px",
                }}
              >
                Download
              </button>
            )}
          </div>
          <div
            style={{
              border: "1px solid #ddd",
              background: "white",
              height: "800px",
              display: "flex",
              overflow: "auto",
              boxSizing: "border-box",
              position: "relative",
            }}
          >
            {isRendering && (
              <div
                style={{
                  position: "absolute",
                  top: 0,
                  left: 0,
                  right: 0,
                  bottom: 0,
                  background: "rgba(255, 255, 255, 0.7)",
                  display: "flex",
                  justifyContent: "center",
                  alignItems: "center",
                  zIndex: 10,
                  fontWeight: "bold",
                  color: "#2196F3",
                  backdropFilter: "blur(2px)",
                }}
              >
                Rendering {params.format.toUpperCase()}...
              </div>
            )}
            {!renderResult && !isRendering && (
              <div
                style={{
                  color: "#999",
                  marginTop: "200px",
                  textAlign: "center",
                  width: "100%",
                }}
              >
                Result will appear here
              </div>
            )}
            {renderResult && params.format === "svg" && (
              <div
                dangerouslySetInnerHTML={{ __html: renderResult as string }}
              />
            )}
            {objectUrl &&
              (params.format === "png" || params.format === "webp") && (
                <div>
                  <img
                    src={objectUrl}
                    alt="render result"
                    style={{
                      boxShadow: "0 2px 10px rgba(0,0,0,0.1)",
                    }}
                  />
                </div>
              )}
            {objectUrl && params.format === "pdf" && (
              <embed
                src={objectUrl}
                type="application/pdf"
                style={{ width: "100%", height: "100%", border: "none" }}
              />
            )}
          </div>
        </div>
      </div>

      <div>
        <h3>Output Source {params.format !== "svg" ? "(Base64)" : ""}:</h3>
        <textarea
          value={outputSource}
          readOnly
          style={{
            width: "100%",
            height: "300px",
            fontFamily: "monospace",
            padding: "10px",
            borderRadius: "4px",
            border: "1px solid #ccc",
            boxSizing: "border-box",
            background: "#fdfdfd",
          }}
        />
      </div>
    </div>
  );
};

export default App;
