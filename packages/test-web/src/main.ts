import { Satoru, createSatoruModule } from "satoru";

async function init() {
  console.log("Initializing Satoru Engine (Skia + Wasm)...");
  try {
    const satoru = new Satoru(createSatoruModule);
    await satoru.init({
      locateFile: (path: string) => {
        if (path.endsWith(".wasm")) {
          return "satoru.wasm";
        }
        return path;
      },
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
                            <button id="loadFontBtn" style="margin-left: 20px; padding: 5px 10px; cursor: pointer; background: #4CAF50; color: white; border: none; border-radius: 4px;">Loading Fonts...</button>
                        </fieldset>
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Load Sample Assets</legend>
                            <select id="assetSelect" style="padding: 5px; border-radius: 4px; border: 1px solid #ccc; width: 100%;">
                                <option value="">-- Select Asset --</option>
                                <option value="01-basic-styling.html">01-basic-styling.html</option>
                                <option value="02-typography.html">02-typography.html</option>
                                <option value="03-flexbox-layout.html">03-flexbox-layout.html</option>
                                <option value="04-japanese-rendering.html">04-japanese-rendering.html</option>
                                <option value="05-ui-components.html">05-ui-components.html</option>
                                <option value="06-complex-layout.html">06-complex-layout.html</option>
                                <option value="07-image-embedding.html">07-image-embedding.html</option>
                                <option value="08-box-shadow.html">08-box-shadow.html</option>
                                <option value="09-complex-layout.html">09-complex-layout.html</option>
                                <option value="11-gradients.html">11-gradients.html</option>
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
                            <div id="svgContainer" style="border: 1px solid #ddd; background: #eee; border-radius: 8px; height: 800px; display: flex;  overflow: auto; box-sizing: border-box;">
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

      const convertBtn = document.getElementById(
        "convertBtn",
      ) as HTMLButtonElement;
      const loadFontBtn = document.getElementById("loadFontBtn");
      const downloadBtn = document.getElementById("downloadBtn");
      const htmlInput = document.getElementById(
        "htmlInput",
      ) as HTMLTextAreaElement;
      const htmlPreview = document.getElementById(
        "htmlPreview",
      ) as HTMLIFrameElement;
      const svgContainer = document.getElementById(
        "svgContainer",
      ) as HTMLDivElement;
      const svgSource = document.getElementById(
        "svgSource",
      ) as HTMLTextAreaElement;
      const canvasWidthInput = document.getElementById(
        "canvasWidth",
      ) as HTMLInputElement;
      const assetSelect = document.getElementById(
        "assetSelect",
      ) as HTMLSelectElement;

      const updatePreview = () => {
        const doc =
          htmlPreview.contentDocument || htmlPreview.contentWindow?.document;
        if (doc) {
          doc.open();
          doc.write(htmlInput.value);
          doc.close();
        }
      };

      const preloadImages = async (html: string) => {
        satoru.clearImages();
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");
        const images = Array.from(doc.querySelectorAll("img"));

        const bgMatches = html.matchAll(
          /background-image:\s*url\(['"]?(data:image\/[^'"]+)['\"]?\)/g,
        );
        const dataUrls = new Set<string>();
        for (const img of images) {
          if (img.src.startsWith("data:")) dataUrls.add(img.src);
        }
        for (const match of bgMatches) {
          dataUrls.add(match[1]);
        }

        await Promise.all(
          Array.from(dataUrls).map(async (url) => {
            return new Promise<void>((resolve) => {
              const img = new Image();
              img.onload = () => {
                satoru.loadImage(url, url, img.width, img.height);
                resolve();
              };
              img.onerror = () => resolve();
              img.src = url;
            });
          }),
        );
      };

      const performConversion = async () => {
        const htmlStr = htmlInput.value;
        const width = parseInt(canvasWidthInput.value) || 580;

        const loadingOverlay = document.createElement("div");
        loadingOverlay.className = "loading-overlay";
        loadingOverlay.innerHTML = '<div class="spinner"></div> Converting...';
        svgContainer.appendChild(loadingOverlay);
        convertBtn.disabled = true;
        convertBtn.style.opacity = "0.7";

        await preloadImages(htmlStr);
        await new Promise((resolve) => setTimeout(resolve, 50));

        try {
          let svgResult = satoru.toSvg(htmlStr, width, 0);

          svgResult = svgResult.replace(
            "<svg",
            '<svg style="overflow:visible"',
          );

          svgContainer.innerHTML = svgResult;
          svgContainer.style.background = "#fff";
          if (svgSource) svgSource.value = svgResult;
          if (downloadBtn) downloadBtn.style.display = "block";
        } catch (err) {
          console.error("Error during Wasm call:", err);
          svgContainer.innerHTML =
            '<div style="color: red; padding: 20px;">Conversion Failed</div>';
        } finally {
          convertBtn.disabled = false;
          convertBtn.style.opacity = "1.0";
        }
      };

      const ROBOTO_400 =
        "https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2";
      const ROBOTO_700 =
        "https://fonts.gstatic.com/s/roboto/v30/KFOlCnqEu92Fr1MmWUlfBBc4.woff2";
      const NOTO_400 =
        "https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-400-normal.woff2";
      const NOTO_700 =
        "https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-700-normal.woff2";

      const loadFont = async (name: string, url: string) => {
        const resp = await fetch(url);
        const buffer = await resp.arrayBuffer();
        satoru.loadFont(name, new Uint8Array(buffer));
      };

      const loadAllFonts = async () => {
        await Promise.all([
          loadFont("Roboto", ROBOTO_400),
          loadFont("Roboto", ROBOTO_700),
          loadFont("Noto Sans JP", NOTO_400),
          loadFont("Noto Sans JP", NOTO_700),
        ]);
      };

      (async () => {
        try {
          if (loadFontBtn) {
            loadFontBtn.setAttribute("disabled", "true");
            loadFontBtn.innerText = "Loading Fonts...";
          }
          await loadAllFonts();
          if (loadFontBtn) loadFontBtn.innerText = "Fonts Loaded \u2713";
          performConversion();
        } catch (e) {
          console.error("Failed to load default fonts:", e);
          if (loadFontBtn) {
            loadFontBtn.innerText = "Font Load Failed";
            loadFontBtn.removeAttribute("disabled");
          }
        }
      })();

      htmlInput.value = `
<div style="padding: 40px; background-color: #e3f2fd; border: 5px solid #2196f3; border-radius: 24px;">
  <h1 style="color: #0d47a1; font-family: 'Roboto'; font-size: 48px; text-align: center; margin-bottom: 10px; font-weight: 700;">Outline Fonts in Wasm</h1>
  <p style="font-size: 22px; color: #1565c0; line-height: 1.5; font-family: 'Roboto'; font-weight: 400;">
    This SVG is rendered using <b>Skia's SVG Canvas</b>. 
    The text is precisely measured and positioned using <b>FreeType</b> metrics.
  </p>\n  <p style="font-size: 20px; color: #0d47a1; font-family: 'Noto Sans JP'; margin-top: 20px; font-weight: 700;">
    日本語の太字（Bold）も表示可能です。
  </p>
  <div style="text-align: center; margin-top: 20px;">
    <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAALUlEQVRYR+3QQREAAAzCQJ9/aaYpAtpAn7Z6AgICAgICAgICAgICAgICAgICAj8WpAEBArZunQAAAABJRU5ErkJggg==" style="width: 64px; height: 64px; border: 2px solid white; border-radius: 8px;" />
  </div>
</div>`;
      updatePreview();

      htmlInput.addEventListener("input", updatePreview);
      assetSelect?.addEventListener("change", async () => {
        const asset = assetSelect.value;
        if (!asset) return;
        try {
          const resp = await fetch(`./assets/${asset}`);
          const text = await resp.text();
          htmlInput.value = text;
          updatePreview();
          performConversion();
        } catch (e) {
          console.error("Error loading asset:", e);
        }
      });

      loadFontBtn?.addEventListener("click", async () => {
        loadFontBtn.innerText = "Loading...";
        loadFontBtn.setAttribute("disabled", "true");
        try {
          await loadAllFonts();
          loadFontBtn.innerText = "Fonts Loaded \u2713";
          performConversion();
        } catch (e) {
          loadFontBtn.innerText = "Load Failed";
          loadFontBtn.removeAttribute("disabled");
        }
      });

      convertBtn?.addEventListener("click", performConversion);
      downloadBtn?.addEventListener("click", () => {
        const blob = new Blob([svgSource.value], { type: "image/svg+xml" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = "satoru_render.svg";
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
      });
    }
  } catch (e) {
    console.error("Failed to load Wasm module:", e);
  }
}

init();
