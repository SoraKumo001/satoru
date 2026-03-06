/** @jsx h */
import { h, toHtml } from "satoru-render/preact";
import { render } from "satoru-render";
import { createCSS } from "satoru-render/tailwind";

const fetch = async (
  request: Request,
  _env: object,
  ctx: ExecutionContext,
): Promise<Response> => {
  const url = new URL(request.url);
  if (url.pathname !== "/") {
    return new Response(null, { status: 404 });
  }
  const subtitle = url.searchParams.get("subtitle") ?? "subtitle";
  const title = url.searchParams.get("title") ?? "Title";
  const image =
    url.searchParams.get("image") ??
    "https://raw.githubusercontent.com/SoraKumo001/cloudflare-ogp/refs/heads/master/sample/image.jpg";
  const cache = await caches.open("satoru-cloudflare-ogp");
  const cacheKey = new Request(url.toString());
  // const cachedResponse = await cache.match(cacheKey);
  // if (cachedResponse) {
  //   return cachedResponse;
  // }

  // Define OGP layout using JSX
  // We include @font-face in a style tag inside the HTML
  const html = toHtml(
    <html className="m-0 p-0">
      <head>
        <link
          href="https://fonts.googleapis.com/css2?family=Noto+Sans+JP:wght@100..900&display=swap"
          rel="stylesheet"
        />
      </head>
      <body className="m-0 p-0">
        <div className="w-[1200px] h-[630px] flex relative bg-[#0a0a0c] overflow-hidden">
          {/* 背景の装飾的要素 */}
          <div className="absolute top-[-150px] right-[-150px] w-[600px] h-[600px] rounded-[300px] bg-[radial-gradient(circle,_rgba(79,70,229,0.3)_0%,_rgba(79,70,229,0)_70%)] flex" />
          <div className="absolute bottom-[-100px] left-[-50px] w-[400px] h-[400px] rounded-[200px] bg-[radial-gradient(circle,_rgba(168,85,247,0.2)_0%,_rgba(168,85,247,0)_70%)] flex" />

          {/* メインコンテナ */}
          <div className="flex flex-row w-full h-full p-[60px] items-center justify-between z-10">
            {/* 左側: テキストコンテンツ */}
            <div className="flex flex-col w-[60%]">
              <div className="flex items-center mb-5">
                <div className="w-10 h-1 bg-[#6366f1] mr-[15px] rounded-sm" />
                <div className="text-2xl font-bold color-[#818cf8] tracking-widest uppercase flex">
                  Featured Content
                </div>
              </div>

              <div className="text-[80px] font-black text-white leading-[1.1] mb-[30px] break-words flex">
                {title}
              </div>

              <div className="text-[32px] font-normal text-[#94a3b8] leading-[1.4] flex">
                {subtitle}
              </div>
            </div>

            {/* 右側: 画像カード */}
            <div className="flex w-[35%] relative justify-center items-center">
              <div className="absolute w-[420px] h-[420px] rounded-[40px] border border-white/10 bg-white/[0.03] rotate-[-3deg] flex" />
              <div className="w-[400px] h-[400px] rounded-[32px] overflow-hidden border-4 border-white/10 flex">
                <img
                  className="w-full h-full object-cover"
                  src={image}
                  alt=""
                />
              </div>
            </div>
          </div>

          {/* フッター */}
          <div className="absolute bottom-10 left-[60px] flex items-center z-20">
            <div className="px-4 py-2 bg-white/[0.05] border border-white/10 rounded-[10px] text-lg text-[#e2e8f0] font-medium flex">
              satoru-cloudflare-ogp
            </div>
          </div>
        </div>
      </body>
    </html>,
  );
  // Render to PNG with automatic font resolution
  const png = await render({
    value: html,
    css: await createCSS(html),
    width: 1200,
    height: 630,
    format: "png",
  });
  const response = new Response(png as BodyInit, {
    headers: {
      "Content-Type": "image/png",
      "Cache-Control": "public, max-age=31536000, immutable",
      date: new Date().toUTCString(),
    },
    cf: {
      cacheEverything: true,
      cacheTtl: 31536000,
    },
  });
  ctx.waitUntil(cache.put(cacheKey, response.clone()));
  return response;
};

export default {
  fetch,
};
