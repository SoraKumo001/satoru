import { Satoru, RequiredResource } from "satoru";

async function init() {
  console.log("Initializing Satoru Engine (Skia + Wasm)...");
  try {
    const satoru = await Satoru.init(undefined, {
      locateFile: (path: string) => {
        if (path.endsWith(".wasm")) return "/satoru.wasm";
        return path;
      }
    });

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
                    #svgContainer { position: relative; }
                </style>
                <div style="padding: 20px; font-family: sans-serif; max-width: 1200px; margin: 0 auto; background: #fafafa; min-height: 100vh;">
                    <h2 style="color: #2196F3; border-bottom: 2px solid #2196F3; padding-bottom: 10px;">Satoru Engine: HTML to Pure SVG</h2>
                    
                    <div style="display: flex; gap: 20px; margin-bottom: 20px;">
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Canvas & Fonts</legend>
                            <label>Width: <input type="number" id="canvasWidth" value="580" style="width: 80px;"></label>
                            <button id="loadFontBtn" style="margin-left: 20px; padding: 5px 10px; cursor: pointer; background: #4CAF50; color: white; border: none; border-radius: 4px;">Auto Font Loading On</button>
                        </fieldset>
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Load Sample Assets</legend>
                            <select id="assetSelect" style="padding: 5px; border-radius: 4px; border: 1px solid #ccc; width: 100%;">
                                <option value="">-- Select Asset --</option>
                                <option value="01-complex-layout.html">01-complex-layout.html</option>
                                <option value="02-typography.html">02-typography.html</option>
                                <option value="03-flexbox-layout.html">03-flexbox-layout.html</option>
                                <option value="04-japanese-rendering.html">04-japanese-rendering.html</option>
                                <option value="05-ui-components.html">05-ui-components.html</option>
                                <option value="06-standard-tags.html">06-standard-tags.html</option>
                                <option value="07-image-embedding.html">07-image-embedding.html</option>
                                <option value="08-box-shadow.html">08-box-shadow.html</option>
                                <option value="09-complex-layout.html">09-complex-layout.html</option>
                                <option value="10-line-style-test.html">10-line-style-test.html</option>
                                <option value="11-gradients.html">11-gradients.html</option>
                                <option value="12-border-radius.html">12-border-radius.html</option>
                            </select>
                        </fieldset>
                    </div>

                    <div style="margin-bottom: 15px;">
                        <h3>HTML Input:</h3>
                        <textarea id="htmlInput" style="width: 100%; height: 150px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box;"></textarea>
                    </div>

                    <button id="convertBtn" style="padding: 15px 30px; font-size: 20px; cursor: pointer; background: #2196F3; color: white; border: none; border-radius: 4px; width: 100%; font-weight: bold; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(33, 150, 243, 0.3);">Generate Vector SVG</button>
                    
                    <div style="display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 20px; margin-bottom: 20px;">
                        <div>
                            <h3>HTML Live Preview (iframe):</h3>
                            <div style="border: 1px solid #ccc; border-radius: 4px; height: 800px; background: white; overflow: hidden; box-sizing: border-box;">
                                <iframe id="htmlPreview" style="width: 100%; height: 100%; border: none;"></iframe>
                            </div>
                        </div>
                        <div>
                            <div style="display: flex; justify-content: space-between; align-items: center;">
                                <h3>SVG Render Preview:</h3>
                                <button id="downloadBtn" style="display: none; background: #FF9800; color: white; border: none; padding: 6px 15px; cursor: pointer; border-radius: 4px;">Download .svg</button>
                            </div>
                            <div id="svgContainer" style="border: 1px solid #ddd; background: #eee; border-radius: 8px; height: 800px; display: flex;  overflow: auto; box-sizing: border-box; justify-content: center;">
                                <div style="color: #999; margin-top: 200px;">Result will appear here</div>
                            </div>
                        </div>
                    </div>

                    <div>
                        <h3>SVG Output Source:</h3>
                        <textarea id="svgSource" style="width: 100%; height: 300px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box; background: #fdfdfd;" readonly></textarea>
                    </div>
                </div>
            `;

      const convertBtn = document.getElementById("convertBtn") as HTMLButtonElement;
      const downloadBtn = document.getElementById("downloadBtn") as HTMLButtonElement;
      const htmlInput = document.getElementById("htmlInput") as HTMLTextAreaElement;
      const htmlPreview = document.getElementById("htmlPreview") as HTMLIFrameElement;
      const svgContainer = document.getElementById("svgContainer") as HTMLDivElement;
      const svgSource = document.getElementById("svgSource") as HTMLTextAreaElement;
      const canvasWidthInput = document.getElementById("canvasWidth") as HTMLInputElement;
      const assetSelect = document.getElementById("assetSelect") as HTMLSelectElement;

      const updatePreview = () => {
        const doc = htmlPreview.contentDocument || htmlPreview.contentWindow?.document;
        if (doc) {
          doc.open();
          doc.write(htmlInput.value);
          doc.close();
        }
      };

      const resourceResolver = async (r: RequiredResource) => {
        console.log(`[Satoru] Resolving ${r.type}: ${r.url}`);
        try {
          const url = (r.url.startsWith("http") || r.url.startsWith("data:")) ? r.url : `../../assets/${r.url}`;
          const isFont = url.match(/\.(woff2?|ttf|otf)$/i);
          
          const resp = await fetch(url);
          if (!resp.ok) return null;

          // Force binary for fonts even if type is 'css'
          if (r.type === "css" && !isFont) {
            return await resp.text();
          } else {
            const buf = await resp.arrayBuffer();
            return new Uint8Array(buf);
          }
        } catch (e) {
          console.error(`Failed to resolve ${r.url}`, e);
          return null;
        }
      };

      const convert = async () => {
        const width = parseInt(canvasWidthInput.value, 10) || 580;
        const html = htmlInput.value;
        if (!html) return;

        convertBtn.disabled = true;
        convertBtn.innerText = "Processing...";

        const loading = document.createElement("div");
        loading.className = "loading-overlay";
        loading.innerHTML = '<div class="spinner"></div>Rendering SVG...';
        svgContainer.style.position = "relative";
        svgContainer.appendChild(loading);

        try {
          const result = await satoru.render(html, width, {
            format: "svg",
            resolveResource: resourceResolver,
          });

          if (typeof result === "string" && result.length > 100) {
            svgContainer.innerHTML = result;
            svgSource.value = result;
            downloadBtn.style.display = "block";
          } else {
            svgContainer.innerHTML = '<div style="color:orange; padding: 20px;">Rendering returned no result.</div>';
            svgSource.value = typeof result === "string" ? result : "";
          }
        } catch (e) {
          console.error("Conversion failed:", e);
          svgContainer.innerHTML = `<div style="color:red; padding: 20px;">Error: ${e}</div>`;
        } finally {
          convertBtn.disabled = false;
          convertBtn.innerText = "Generate Vector SVG";
        }
      };

      convertBtn.addEventListener("click", convert);

      assetSelect.addEventListener("change", async () => {
        const file = assetSelect.value;
        if (!file) return;

        try {
          const resp = await fetch(`../../assets/${file}`);
          const html = await resp.text();
          htmlInput.value = html;
          updatePreview();
          setTimeout(() => convert(), 100);
        } catch (e) {
          console.error(`Failed to load asset: ${file}`, e);
        }
      });

      downloadBtn.addEventListener("click", () => {
        const blob = new Blob([svgSource.value], { type: "image/svg+xml" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = "rendered.svg";
        a.click();
        URL.revokeObjectURL(url);
      });

      htmlInput.addEventListener("input", updatePreview);

      const initialAsset = "01-complex-layout.html";
      try {
        const resp = await fetch(`../../assets/${initialAsset}`);
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
    if (app) app.innerHTML = `<div style="color:red; padding: 50px;">Failed to initialize: ${e}</div>`;
  }
}

init();
