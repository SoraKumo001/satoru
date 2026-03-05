import { toHtml } from "satoru-render/react";
import { render } from "satoru-render";

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
  const html = `

    ${toHtml(
      <html style={{ margin: 0, padding: 0 }}>
        <head>
          <link
            href="https://fonts.googleapis.com/css2?family=Noto+Sans+JP:wght@100..900&display=swap"
            rel="stylesheet"
          />
        </head>
        <body style={{ margin: 0, padding: 0 }}>
          <div
            style={{
              width: "1200px",
              height: "630px",
              display: "flex",
              position: "relative",
              background: "#0a0a0c",
              overflow: "hidden",
            }}
          >
            {/* 背景の装飾的要素 */}
            <div
              style={{
                position: "absolute",
                top: "-150px",
                right: "-150px",
                width: "600px",
                height: "600px",
                borderRadius: "300px",
                background:
                  "radial-gradient(circle, rgba(79, 70, 229, 0.3) 0%, rgba(79, 70, 229, 0) 70%)",
                display: "flex",
              }}
            />
            <div
              style={{
                position: "absolute",
                bottom: "-100px",
                left: "-50px",
                width: "400px",
                height: "400px",
                borderRadius: "200px",
                background:
                  "radial-gradient(circle, rgba(168, 85, 247, 0.2) 0%, rgba(168, 85, 247, 0) 70%)",
                display: "flex",
              }}
            />

            {/* メインコンテナ */}
            <div
              style={{
                display: "flex",
                flexDirection: "row",
                width: "100%",
                height: "100%",
                padding: "60px",
                alignItems: "center",
                justifyContent: "space-between",
                zIndex: 10,
              }}
            >
              {/* 左側: テキストコンテンツ */}
              <div
                style={{
                  display: "flex",
                  flexDirection: "column",
                  width: "60%",
                }}
              >
                <div
                  style={{
                    display: "flex",
                    alignItems: "center",
                    marginBottom: "20px",
                  }}
                >
                  <div
                    style={{
                      width: "40px",
                      height: "4px",
                      background: "#6366f1",
                      marginRight: "15px",
                      borderRadius: "2px",
                    }}
                  />
                  <div
                    style={{
                      fontSize: "24px",
                      fontWeight: 700,
                      color: "#818cf8",
                      letterSpacing: "0.1em",
                      textTransform: "uppercase",
                      display: "flex",
                    }}
                  >
                    Featured Content
                  </div>
                </div>

                <div
                  style={{
                    fontSize: "80px",
                    fontWeight: 900,
                    color: "#ffffff",
                    lineHeight: 1.1,
                    marginBottom: "30px",
                    wordBreak: "break-word",
                    display: "flex",
                  }}
                >
                  {title}
                </div>

                <div
                  style={{
                    fontSize: "32px",
                    fontWeight: 400,
                    color: "#94a3b8",
                    lineHeight: 1.4,
                    display: "flex",
                  }}
                >
                  {subtitle}
                </div>
              </div>

              {/* 右側: 画像カード */}
              <div
                style={{
                  display: "flex",
                  width: "35%",
                  position: "relative",
                  justifyContent: "center",
                  alignItems: "center",
                }}
              >
                <div
                  style={{
                    position: "absolute",
                    width: "420px",
                    height: "420px",
                    borderRadius: "40px",
                    border: "1px solid rgba(255, 255, 255, 0.1)",
                    background: "rgba(255, 255, 255, 0.03)",
                    transform: "rotate(-3deg)",
                    display: "flex",
                  }}
                />
                <div
                  style={{
                    width: "400px",
                    height: "400px",
                    borderRadius: "32px",
                    overflow: "hidden",
                    border: "4px solid rgba(255, 255, 255, 0.1)",
                    display: "flex",
                  }}
                >
                  <img
                    style={{
                      width: "100%",
                      height: "100%",
                      objectFit: "cover",
                    }}
                    src={image}
                    alt=""
                  />
                </div>
              </div>
            </div>

            {/* フッター */}
            <div
              style={{
                position: "absolute",
                bottom: "40px",
                left: "60px",
                display: "flex",
                alignItems: "center",
                zIndex: 20,
              }}
            >
              <div
                style={{
                  padding: "8px 16px",
                  background: "rgba(255, 255, 255, 0.05)",
                  border: "1px solid rgba(255, 255, 255, 0.1)",
                  borderRadius: "10px",
                  fontSize: "18px",
                  color: "#e2e8f0",
                  fontWeight: 500,
                  display: "flex",
                }}
              >
                satoru-cloudflare-ogp
              </div>
            </div>
          </div>
        </body>
      </html>,
    )}
  `;
  console.log(html);
  // Render to PNG with automatic font resolution
  const png = await render({
    value: html,
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
