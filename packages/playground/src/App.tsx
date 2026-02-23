import React, { useState, useEffect, useRef, useMemo } from "react";
import { DEFAULT_FONT_MAP } from "satoru";
import { createSatoruWorker, LogLevel } from "satoru/workers";

const satoru = createSatoruWorker({
  maxParallel: 1,
});

const App: React.FC = () => {
  const [html, setHtml] = useState<string>("");
  const [width, setWidth] = useState<number>(588);
  const [format, setFormat] = useState<"svg" | "png" | "webp" | "pdf">("svg");
  const [textToPaths, setTextToPaths] = useState<boolean>(true);
  const [assetList, setAssetList] = useState<string[]>([]);
  const [selectedAsset, setSelectedAsset] = useState<string>("");
  const [fontMapJson, setFontMapJson] = useState<string>(
    JSON.stringify(DEFAULT_FONT_MAP, null, 2),
  );
  const [isRendering, setIsRendering] = useState<boolean>(false);
  const [renderResult, setRenderResult] = useState<string | Uint8Array | null>(
    null,
  );
  const [renderTime, setRenderTime] = useState<number | null>(null);
  const [objectUrl, setObjectUrl] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const iframeRef = useRef<HTMLIFrameElement>(null);

  // Initialize Satoru Worker
  useEffect(() => {
    // Get asset list
    const assetFiles = import.meta.glob("../../../assets/*.html", {
      query: "?url",
      import: "default",
    });
    setAssetList(Object.keys(assetFiles).map((path) => path.split("/").pop()!));

    const params = new URLSearchParams(window.location.search);
    const initialWidth = params.get("width");
    if (initialWidth) setWidth(parseInt(initialWidth));

    const initialFormat = params.get("format");
    if (
      initialFormat &&
      ["svg", "png", "webp", "pdf"].includes(initialFormat)
    ) {
      setFormat(initialFormat as any);
    }

    const initialTextToPaths = params.get("textToPaths");
    if (initialTextToPaths !== null) {
      setTextToPaths(initialTextToPaths === "true");
    }

    const initialFontMap = params.get("fontMap");
    if (initialFontMap) {
      try {
        const parsed = JSON.parse(initialFontMap);
        setFontMapJson(JSON.stringify(parsed, null, 2));
      } catch (e) {
        // ignore
      }
    }

    // Handle browser back/forward
    const handlePopState = () => {
      const p = new URLSearchParams(window.location.search);
      const asset = p.get("asset");
      const w = p.get("width");
      const f = p.get("format");
      const t = p.get("textToPaths");

      if (w) setWidth(parseInt(w));
      else setWidth(588); // Default

      if (f && ["svg", "png", "webp", "pdf"].includes(f)) {
        setFormat(f as any);
      } else {
        setFormat("svg"); // Default
      }

      if (t !== null) {
        setTextToPaths(t === "true");
      } else {
        setTextToPaths(true); // Default
      }

      if (asset && asset !== selectedAsset) {
        loadAsset(asset);
      }
    };

    window.addEventListener("popstate", handlePopState);

    return () => {
      satoru.close();
      window.removeEventListener("popstate", handlePopState);
    };
  }, []);

  // Update URL when parameters change
  useEffect(() => {
    const url = new URL(window.location.href);
    let changed = false;

    const syncParam = (key: string, value: string) => {
      if (url.searchParams.get(key) !== value) {
        url.searchParams.set(key, value);
        changed = true;
      }
    };

    syncParam("width", width.toString());
    syncParam("format", format);

    if (format === "svg") {
      syncParam("textToPaths", textToPaths.toString());
    } else if (url.searchParams.has("textToPaths")) {
      url.searchParams.delete("textToPaths");
      changed = true;
    }

    if (selectedAsset) syncParam("asset", selectedAsset);

    try {
      const currentMap = JSON.parse(fontMapJson);
      if (JSON.stringify(currentMap) !== JSON.stringify(DEFAULT_FONT_MAP)) {
        syncParam("fontMap", JSON.stringify(currentMap));
      } else {
        url.searchParams.delete("fontMap");
        changed = true;
      }
    } catch (e) {
      // ignore invalid JSON
    }

    if (changed) {
      window.history.replaceState({}, "", url.toString());
    }

    // Auto-generate when parameters change (if HTML is loaded)
    if (html) {
      handleConvert(html);
    }
  }, [width, format, textToPaths, selectedAsset]);

  // Update iframe preview
  useEffect(() => {
    if (iframeRef.current) {
      const doc =
        iframeRef.current.contentDocument ||
        iframeRef.current.contentWindow?.document;
      if (doc) {
        doc.open();
        const baseTag = `<base href="${window.location.origin}${window.location.pathname}assets/">`;
        const htmlWithBase = html.includes("<head>")
          ? html.replace("<head>", `<head>${baseTag}`)
          : baseTag + html;
        doc.write(htmlWithBase);
        doc.close();
      }
    }
  }, [html]);

  // Handle asset selection (Initial or when assetList is loaded)
  useEffect(() => {
    if (assetList.length === 0) return;

    const params = new URLSearchParams(window.location.search);
    const initialAsset = params.get("asset") || "01-layout.html";

    if (assetList.includes(initialAsset)) {
      if (selectedAsset !== initialAsset) {
        loadAsset(initialAsset);
      }
    } else if (!selectedAsset) {
      loadAsset(assetList[0]);
    }
  }, [assetList]);

  const loadAsset = async (name: string) => {
    try {
      const resp = await fetch(`assets/${name}`);
      const text = await resp.text();
      setHtml(text);
      setSelectedAsset(name);
    } catch (e) {
      console.error("Failed to load asset", e);
    }
  };

  const handleConvert = async (overrideHtml?: string) => {
    const currentHtml = overrideHtml !== undefined ? overrideHtml : html;
    if (!satoru || !currentHtml) return;

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

      console.log("[Satoru] Rendering via Worker");
      const result = await satoru.render({
        value: currentHtml,
        width,
        format,
        textToPaths,
        fontMap,
        logLevel: LogLevel.Info,
        onLog: (level, message) => {
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
          await cache.put(resource.url, new Response(data as BodyInit));

          return data;
        },
      });

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
      console.error("Conversion failed:", e);
      setError(e.message);
    } finally {
      setIsRendering(false);
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
          Satoru Engine Playground (Multi-threaded)
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
            <label>
              Width:{" "}
              <input
                type="number"
                value={width}
                onChange={(e) => {
                  setWidth(parseInt(e.target.value) || 0);
                }}
                style={{ width: "80px" }}
              />
            </label>
            <label>
              Format:{" "}
              <select
                value={format}
                onChange={(e) =>
                  setFormat(e.target.value as "svg" | "png" | "webp" | "pdf")
                }
                style={{ padding: "2px 5px", borderRadius: "4px" }}
              >
                <option value="svg">SVG (Vector)</option>
                <option value="png">PNG (Raster)</option>
                <option value="webp">WebP (Raster)</option>
                <option value="pdf">PDF (Document)</option>
              </select>
            </label>
            {format === "svg" && (
              <label>
                <input
                  type="checkbox"
                  checked={textToPaths}
                  onChange={(e) => setTextToPaths(e.target.checked)}
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
          <legend style={{ fontWeight: "bold" }}>Font Mapping (JSON)</legend>
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
            value={selectedAsset}
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
          value={html}
          onChange={(e) => setHtml(e.target.value)}
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
              overflowY: "hidden",
              overflowX: "auto",
              boxSizing: "border-box",
            }}
          >
            <iframe
              ref={iframeRef}
              title="preview"
              style={{
                width: `${width}px`,
                height: "100%",
                border: "none",
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
            <h3>{format.toUpperCase()} Render Preview:</h3>
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
                Rendering {format.toUpperCase()}...
              </div>
            )}
            {!renderResult && !isRendering && (
              <div style={{ color: "#999", marginTop: "200px" }}>
                Result will appear here
              </div>
            )}
            {renderResult && format === "svg" && (
              <div
                dangerouslySetInnerHTML={{ __html: renderResult as string }}
              />
            )}
            {objectUrl && (format === "png" || format === "webp") && (
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
            {objectUrl && format === "pdf" && (
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
        <h3>Output Source {format !== "svg" ? "(Base64)" : ""}:</h3>
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
