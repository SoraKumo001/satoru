import { Satoru, RequiredResource } from "satoru";

async function init() {
  console.log("Initializing Satoru Engine (Skia + Wasm)...");
  try {
    const satoru = await Satoru.init();

    const app = document.getElementById("app");
    if (app) {
      app.innerHTML = `
                <style>
                    .loading-overlay {
                        position: absolute;
                        top: 0; left: 0; right: 0; bottom: 0;
                        background: rgba(255, 255, 255, 0.7);
                        display: flex;
                        justify-content: center;
                        align-items: center;
                        z-index: 10;
                        border-radius: 8px;
                        font-weight: bold;
                        color: #2196F3;
                        backdrop-filter: blur(2px);
                    }
                    .spinner {
                        width: 40px;
                        height: 40px;
                        border: 4px solid #f3f3f3;
                        border-top: 4px solid #2196F3;
                        border-radius: 50%;
                        animation: spin 1s linear infinite;
                        margin-right: 15px;
                    }
                    @keyframes spin {
                        0% { transform: rotate(0deg); }
                        100% { transform: rotate(360deg); }
                    }
                    #renderContainer { position: relative; }
                    .preview-frame {
                        width: 100%;
                        height: 100%;
                        border: none;
                        background: white;
                    }
                </style>
                <div style="padding: 20px; font-family: sans-serif; max-width: 1200px; margin: 0 auto; background: #fafafa; min-height: 100vh;">
                    <h2 style="color: #2196F3; border-bottom: 2px solid #2196F3; padding-bottom: 10px;">Satoru Engine: HTML to Vector/Raster</h2>
                    
                    <div style="display: flex; gap: 20px; margin-bottom: 20px;">
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Rendering Options</legend>
                            <div style="display: flex; flex-direction: column; gap: 10px;">
                                <label>Width: <input type="number" id="canvasWidth" value="588" style="width: 80px;"></label>
                                <label>Format: 
                                    <select id="formatSelect" style="padding: 2px 5px; border-radius: 4px;">
                                        <option value="svg" selected>SVG (Vector)</option>
                                        <option value="png">PNG (Raster)</option>
                                        <option value="pdf">PDF (Document)</option>
                                    </select>
                                </label>
                            </div>
                        </fieldset>
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Load Sample Assets</legend>
                            <select id="assetSelect" style="padding: 5px; border-radius: 4px; border: 1px solid #ccc; width: 100%;">
                                <option value="">-- Select Asset --</option>
                            </select>
                        </fieldset>
                    </div>

                    <div style="margin-bottom: 15px;">
                        <h3>HTML Input:</h3>
                        <textarea id="htmlInput" style="width: 100%; height: 150px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box;"></textarea>
                    </div>

                    <button id="convertBtn" style="padding: 15px 30px; font-size: 20px; cursor: pointer; background: #2196F3; color: white; border: none; border-radius: 4px; width: 100%; font-weight: bold; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(33, 150, 243, 0.3);">Generate Output</button>
                    
                    <div style="display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 20px; margin-bottom: 20px;">
                        <div>
                            <h3>HTML Live Preview (iframe):</h3>
                            <div style="border: 1px solid #ccc; border-radius: 4px; height: 800px; background: white; overflow: hidden; box-sizing: border-box;">
                                <iframe id="htmlPreview" style="width: 100%; height: 100%; border: none;"></iframe>
                            </div>
                        </div>
                        <div>
                            <div style="display: flex; justify-content: space-between; align-items: center;">
                                <h3 id="previewTitle">Output Render Preview:</h3>
                                <button id="downloadBtn" style="display: none; background: #FF9800; color: white; border: none; padding: 6px 15px; cursor: pointer; border-radius: 4px;">Download</button>
                            </div>
                            <div id="renderContainer" style="border: 1px solid #ddd; background: white; border-radius: 8px; height: 800px; display: flex;  overflow: auto; box-sizing: border-box; justify-content: center;">
                                <div style="color: #999; margin-top: 200px;">Result will appear here</div>
                            </div>
                        </div>
                    </div>

                    <div id="sourceSection">
                        <h3 id="sourceTitle">Output Source:</h3>
                        <textarea id="outputSource" style="width: 100%; height: 300px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box; background: #fdfdfd;" readonly></textarea>
                    </div>
                </div>
            `;

      const convertBtn = document.getElementById(
        "convertBtn",
      ) as HTMLButtonElement;
      const downloadBtn = document.getElementById(
        "downloadBtn",
      ) as HTMLButtonElement;
      const htmlInput = document.getElementById(
        "htmlInput",
      ) as HTMLTextAreaElement;
      const htmlPreview = document.getElementById(
        "htmlPreview",
      ) as HTMLIFrameElement;
      const renderContainer = document.getElementById(
        "renderContainer",
      ) as HTMLDivElement;
      const outputSource = document.getElementById(
        "outputSource",
      ) as HTMLTextAreaElement;
      const canvasWidthInput = document.getElementById(
        "canvasWidth",
      ) as HTMLInputElement;
      const formatSelect = document.getElementById(
        "formatSelect",
      ) as HTMLSelectElement;
      const assetSelect = document.getElementById(
        "assetSelect",
      ) as HTMLSelectElement;
      const previewTitle = document.getElementById(
        "previewTitle",
      ) as HTMLHeadingElement;
      const sourceTitle = document.getElementById(
        "sourceTitle",
      ) as HTMLHeadingElement;

      // Automatically populate asset dropdown
      const assetFiles = import.meta.glob("../../../assets/*.html", {
        query: "?raw",
        import: "default",
      });
      Object.keys(assetFiles).forEach((path) => {
        const fileName = path.split("/").pop()!;
        const option = new Option(fileName, fileName);
        assetSelect.add(option);
      });

      let currentObjectURL: string | null = null;
      let lastResult: string | Uint8Array | null = null;

      const updatePreview = () => {
        const doc =
          htmlPreview.contentDocument || htmlPreview.contentWindow?.document;
        if (doc) {
          doc.open();
          const baseTag = `<base href="${window.location.origin}${window.location.pathname}assets/">`;
          const html = htmlInput.value;
          const htmlWithBase = html.includes("<head>")
            ? html.replace("<head>", `<head>${baseTag}`)
            : baseTag + html;
          doc.write(htmlWithBase);
          doc.close();
        }
      };

      const resourceResolver = async (r: RequiredResource) => {
        console.log(`[Satoru] Resolving ${r.type}: ${r.url}`);
        try {
          const url =
            r.url.startsWith("http") || r.url.startsWith("data:")
              ? r.url
              : `assets/${r.url}`;

          const resp = await fetch(url);
          if (!resp.ok) return null;

          const buf = await resp.arrayBuffer();
          return new Uint8Array(buf);
        } catch (e) {
          console.error(`Failed to resolve ${r.url}`, e);
          return null;
        }
      };

      const convert = async () => {
        const width = parseInt(canvasWidthInput.value, 10) || 588;
        const html = htmlInput.value;
        const format = formatSelect.value as "svg" | "png" | "pdf";
        if (!html) return;

        convertBtn.disabled = true;
        convertBtn.innerText = "Processing...";

        if (currentObjectURL) {
          URL.revokeObjectURL(currentObjectURL);
          currentObjectURL = null;
        }

        const loading = document.createElement("div");
        loading.className = "loading-overlay";
        loading.innerHTML = `<div class="spinner"></div>Rendering ${format.toUpperCase()}...`;
        renderContainer.style.position = "relative";
        renderContainer.appendChild(loading);

        try {
          const result = await satoru.render(html, width, {
            format,
            resolveResource: resourceResolver,
          });
          lastResult = result;

          previewTitle.innerText = `${format.toUpperCase()} Render Preview:`;
          sourceTitle.innerText = `${format.toUpperCase()} Output Source${format !== "svg" ? " (Base64)" : ""}:`;

          if (result) {
            if (format === "svg" && typeof result === "string") {
              renderContainer.innerHTML = result;
              outputSource.value = result;
            } else if (result instanceof Uint8Array) {
              const mimeType =
                format === "png" ? "image/png" : "application/pdf";
              // Use slice() to ensure we have a standard Uint8Array on a standard ArrayBuffer
              const resultCopy = result.slice();
              const blob = new Blob([resultCopy], { type: mimeType });
              currentObjectURL = URL.createObjectURL(blob);

              if (format === "png") {
                renderContainer.innerHTML = `<img src="${currentObjectURL}" style="max-width: 100%; height: auto; box-shadow: 0 2px 10px rgba(0,0,0,0.1);">`;
              } else {
                renderContainer.innerHTML = `<embed src="${currentObjectURL}" type="application/pdf" class="preview-frame">`;
              }

              // Show Base64 in source for binary formats
              let binary = "";
              const len = result.byteLength;
              for (let i = 0; i < len; i++) {
                binary += String.fromCharCode(result[i]);
              }
              outputSource.value = btoa(binary);
            }
            downloadBtn.style.display = "block";
            downloadBtn.innerText = `Download .${format}`;
          } else {
            renderContainer.innerHTML =
              '<div style="color:orange; padding: 20px;">Rendering returned no result.</div>';
            outputSource.value = "";
          }
        } catch (e) {
          console.error("Conversion failed:", e);
          renderContainer.innerHTML = `<div style="color:red; padding: 20px;">Error: ${e}</div>`;
        } finally {
          convertBtn.disabled = false;
          convertBtn.innerText = "Generate Output";
        }
      };

      convertBtn.addEventListener("click", convert);
      formatSelect.addEventListener("change", convert);

      assetSelect.addEventListener("change", async () => {
        const file = assetSelect.value;
        if (!file) return;

        const url = new URL(window.location.href);
        url.searchParams.set("asset", file);
        window.history.replaceState({}, "", url.toString());

        try {
          const resp = await fetch(`assets/${file}`);
          const html = await resp.text();
          htmlInput.value = html;
          updatePreview();
          setTimeout(() => convert(), 100);
        } catch (e) {
          console.error(`Failed to load asset: ${file}`, e);
        }
      });

      downloadBtn.addEventListener("click", () => {
        if (!lastResult) return;
        const format = formatSelect.value;
        const mimeType =
          format === "svg"
            ? "image/svg+xml"
            : format === "png"
              ? "image/png"
              : "application/pdf";

        // Ensure standard Uint8Array for Blob
        const content =
          typeof lastResult === "string" ? lastResult : lastResult.slice();
        const blob = new Blob([content as BlobPart], { type: mimeType });

        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = `rendered.${format}`;
        a.click();
        setTimeout(() => URL.revokeObjectURL(url), 100);
      });

      htmlInput.addEventListener("input", updatePreview);

      const initialAsset =
        new URLSearchParams(window.location.search).get("asset") ||
        "01-layout.html";
      try {
        const resp = await fetch(`assets/${initialAsset}`);
        const html = await resp.text();
        htmlInput.value = html;
        assetSelect.value = initialAsset;
        updatePreview();
        setTimeout(() => convert(), 1000);
      } catch (e) {
        console.warn("Could not load initial asset", e);
      }
    }
  } catch (e) {
    console.error("Failed to initialize Satoru Engine:", e);
    const app = document.getElementById("app");
    if (app)
      app.innerHTML = `<div style="color:red; padding: 50px;">Failed to initialize: ${e}</div>`;
  }
}

init();
