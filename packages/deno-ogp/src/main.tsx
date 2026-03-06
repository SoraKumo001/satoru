/** @jsx h */
import { h, toHtml } from "satoru-render/preact";
import { createCSS } from "satoru-render/tailwind";
import { render } from "satoru-render";

Deno.serve(async (request) => {
  const url = new URL(request.url);
  if (url.pathname !== "/") {
    return new Response(null, { status: 404 });
  }

  const name = url.searchParams.get("name") ?? "Name";
  const title = url.searchParams.get("title") ?? "Title";
  const image = url.searchParams.get("image");

  const isDev =
    Deno.env.get("DENO_ENV") === "development" || url.hostname === "localhost";
  const cache = await caches.open("satoru-ogp");
  const cacheKey = new Request(url.toString());

  if (!isDev) {
    const cachedResponse = await cache.match(cacheKey);
    if (cachedResponse) return cachedResponse;
  }

  const html = toHtml(
    <html>
      <head>
        <link
          href="https://fonts.googleapis.com/css2?family=Noto+Sans+JP:wght@100..900&display=swap"
          rel="stylesheet"
        />
      </head>
      <body
        style={{
          margin: 16,
          borderRadius: 16,
          boxSizing: "border-box",
          overflow: "hidden",
        }}
      >
        <div
          style={{
            border: "solid 16px",
            borderImage:
              "linear-gradient(to right, #6666aa 0%, #333366 20%, #222233 100%) 2",
            boxSizing: "border-box",
            width: "100%",
            height: "100%",
            position: "relative",
            display: "flex",
            flexDirection: "column",
            alignItems: "center",
            backgroundImage:
              "linear-gradient(to bottom right, #333355, #666688)",
            backgroundSize: "16px 16px, 100% 100%",
            color: "white",
            overflow: "hidden",
          }}
        >
          {image && (
            <img
              style={{
                borderRadius: "100%",
                padding: "24px",
                opacity: 0.7,
                position: "absolute",
              }}
              width={480}
              height={480}
              src={image}
              alt=""
            />
          )}
          <div>
            <div
              style={{
                fontSize: "80px",
                fontWeight: "bold",
                textShadow: "0 8px 16px rgba(0,0,0,0.6)",
                filter: "drop-shadow(0 4px 8px rgba(0,0,0,0.8))",
                backdropFilter: "blur(5px)",
                zIndex: 1,
                display: "-webkit-box",
                WebkitBoxOrient: "vertical",
                lineClamp: 3,
                overflow: "hidden",
                margin: 32,
                borderRadius: 32,
                padding: 16,
              }}
            >
              {title}
            </div>
          </div>

          <div
            style={{
              position: "absolute",
              bottom: "40px",
              right: "40px",
              fontSize: "24px",
              fontWeight: "bold",
              padding: "12px 24px",
              border: "1px solid rgba(255,255,255,0.2)",
              filter: "drop-shadow(0 4px 8px rgba(0,0,0,0.8))",
              borderRadius: "12px",
              backdropFilter: "blur(4px)",
              color: "#EE6688",
              zIndex: 1,
            }}
          >
            {name}
          </div>
        </div>
      </body>
    </html>,
  );
  // Render to PNG with automatic font resolution
  const png = await render({
    value: html,
    width: 1200,
    height: 630,
    css: await createCSS(html),
    format: "png",
  });
  const response = new Response(png as BodyInit, {
    headers: {
      "Content-Type": "image/png",
      "Cache-Control": isDev
        ? "no-cache"
        : "public, max-age=31536000, immutable",
      date: new Date().toUTCString(),
    },
  });
  if (!isDev) {
    await cache.put(cacheKey, response.clone());
  }
  return response;
});
