import { Satoru, RequiredResource } from "satoru";

export interface Env {}

// Cache for resource data to avoid re-fetching on every request
const resourceCache = new Map<string, Uint8Array>();

/**
 * サンプルHTML。Satoruの機能（日本語、グラデーション、シャドウ、角丸、Flexbox、CSSクラス）を示す完全なHTML構造。
 */
const SAMPLE_HTML = `<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <style>
    @font-face {
      font-family: 'Noto Sans JP';
      src: url('https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-400-normal.woff2');
    }
    .card-container {
      font-family: 'Noto Sans JP', sans-serif;
      padding: 40px;
      background: #f0f2f5;
      display: flex;
      justify-content: center;
    }
    .card {
      width: 400px;
      background: white;
      border-radius: 16px;
      overflow: hidden;
      box-shadow: 0 10px 25px rgba(0,0,0,0.1);
      border: 1px solid #eee;
    }
    .card-header {
      height: 120px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .card-logo {
      color: white;
      margin: 0;
      font-size: 28px;
      letter-spacing: 2px;
    }
    .card-body {
      padding: 24px;
    }
    .meta-row {
      display: flex;
      align-items: center;
      margin-bottom: 16px;
    }
    .badge {
      background: #eef2ff;
      color: #4f46e5;
      padding: 4px 12px;
      border-radius: 20px;
      font-size: 12px;
      font-weight: bold;
      margin-right: 10px;
    }
    .date {
      color: #666;
      font-size: 12px;
    }
    .title {
      margin: 0 0 12px 0;
      color: #1a1a1a;
      font-size: 20px;
    }
    .description {
      margin: 0 0 20px 0;
      color: #4a4a4a;
      line-height: 1.6;
      font-size: 14px;
    }
    .highlight-blue {
      color: #4f46e5;
      font-weight: bold;
    }
    .highlight-yellow {
      background: #fff3bf;
      padding: 0 2px;
    }
    .highlight-red {
      color: #ff4b2b;
      font-weight: bold;
    }
    .underline-purple {
      text-decoration: underline;
      text-decoration-color: #764ba2;
      text-decoration-thickness: 2px;
    }
    .card-footer {
      border-top: 1px solid #eee;
      padding-top: 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .color-dots {
      display: flex;
      gap: 8px;
    }
    .dot {
      width: 24px;
      height: 24px;
      border-radius: 50%;
    }
    .dot-red { background: #ff4b2b; }
    .dot-pink { background: #ff416c; }
    .dot-dark { background: #2b5876; }
    .link {
      color: #4f46e5;
      font-weight: bold;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <div class="card-container">
    <div class="card">
      <div class="card-header">
        <h2 class="card-logo">SATORU</h2>
      </div>
      <div class="card-body">
        <div class="meta-row">
          <span class="badge">New Feature</span>
          <span class="date">2026.02.06</span>
        </div>
        <h3 class="title">高性能HTMLレンダリング</h3>
        <p class="description">
          Satoruは<span class="highlight-blue">WebAssembly</span>上で動作する<span class="highlight-yellow">超軽量・高速</span>なHTMLレンダラーです。<span class="highlight-red">Skia</span>を使用した高品質な描画により、サーバーサイドでの<span class="underline-purple">画像生成</span>を容易にします。
        </p>
        <div class="card-footer">
          <div class="color-dots">
            <div class="dot dot-red"></div>
            <div class="dot dot-pink"></div>
            <div class="dot dot-dark"></div>
          </div>
          <div class="link">詳細を見る →</div>
        </div>
      </div>
    </div>
  </div>
</body>
</html>`;

/**
 * メインの入力フォームUIを生成
 */
function renderIndex(sampleHtml: string) {
  return `
<!DOCTYPE html>
<html>
  <head>
    <title>Satoru HTML to PNG/SVG</title>
    <style>
      body { font-family: sans-serif; max-width: 800px; margin: 2rem auto; padding: 0 1rem; line-height: 1.5; }
      textarea { width: 100%; height: 500px; margin: 1rem 0; font-family: monospace; padding: 0.5rem; background: #f9f9f9; }
      .controls { display: flex; gap: 1rem; align-items: center; flex-wrap: wrap; }
      button { padding: 0.5rem 1rem; cursor: pointer; background: #0070f3; color: white; border: none; border-radius: 4px; font-weight: bold; }
      button:hover { background: #0051ad; }
      input[type="number"] { padding: 0.4rem; border: 1px solid #ccc; border-radius: 4px; width: 80px; }
      select { padding: 0.4rem; border: 1px solid #ccc; border-radius: 4px; }
    </style>
  </head>
  <body>
    <h1>Satoru Renderer</h1>
    <p>HTMLを入力してPNGまたはSVGとしてレンダリングします。</p>
    <form action="/" method="GET" target="_blank">
      <textarea name="html" placeholder="<html><body><h1>Hello World</h1></body></html>">${sampleHtml}</textarea>
      <div class="controls">
        <label>Width: <input type="number" name="width" value="800"></label>
        <label>Format: 
          <select name="format">
            <option value="png">PNG</option>
            <option value="svg">SVG</option>
          </select>
        </label>
        <button type="submit">レンダリング実行（別タブ）</button>
      </div>
    </form>
  </body>
</html>
`;
}

export default {
  async fetch(
    request: Request,
    env: Env,
    ctx: ExecutionContext,
  ): Promise<Response> {
    const url = new URL(request.url);

    // GETリクエストでパラメータがない場合は入力フォームを表示
    if (request.method === "GET" && !url.searchParams.has("html")) {
      return new Response(renderIndex(SAMPLE_HTML), {
        headers: { "Content-Type": "text/html;charset=UTF-8" },
      });
    }

    // GETパラメータ（またはPOST）からHTMLとWidth, Formatを取得
    let html = url.searchParams.get("html") || "";
    let width = parseInt(url.searchParams.get("width") || "800");
    let format = (url.searchParams.get("format") || "png") as "png" | "svg";

    if (!html && request.method === "POST") {
      const formData = await request.formData();
      html = formData.get("html")?.toString() || "";
      width = parseInt(formData.get("width")?.toString() || "800");
      format = (formData.get("format")?.toString() || "png") as "png" | "svg";
    }

    try {
      const satoru = await Satoru.init();

      // 動的リソース（フォント、画像等）の解決
      const result = await satoru.render({
        value: html,
        width,
        format,
        resolveResource: async (resource: RequiredResource) => {
          // キャッシュを確認
          if (resourceCache.has(resource.url)) {
            console.log(`Cache hit: ${resource.url}`);
            return resourceCache.get(resource.url)!;
          }

          console.log(`Fetching resource: ${resource.url} (${resource.type})`);
          try {
            const response = await fetch(resource.url);
            if (!response.ok) {
              console.error(`Failed to fetch resource: ${response.statusText}`);
              return null;
            }
            const data = new Uint8Array(await response.arrayBuffer());
            resourceCache.set(resource.url, data);
            return data;
          } catch (e) {
            console.error(`Error fetching resource ${resource.url}:`, e);
            return null;
          }
        },
      });

      if (!result) {
        throw new Error("Rendering failed");
      }

      const contentType = format === "svg" ? "image/svg+xml" : "image/png";
      return new Response(result as BodyInit, {
        headers: {
          "Content-Type": contentType,
          "Cache-Control": "public, max-age=3600",
        },
      });
    } catch (e: any) {
      return new Response(e.stack || e.toString(), { status: 500 });
    }
  },
};
