---
sidebar_position: 4
title: Resources
---

# Resources

## ResourceResolver

`resolveResource` hooks resource resolution for fonts, CSS, and images. Use it for custom fetch behavior, authentication headers, caching, and SSRF protection.

```typescript
import { render } from "satoru-render/single";

const png = await render({
  value: html,
  width: 1200,
  height: 630,
  format: "png",
  resolveResource: async (resource, defaultResolver) => {
    if (resource.url.startsWith("https://assets.example.com/")) {
      const response = await fetch(resource.url, {
        headers: { Authorization: `Bearer ${ASSET_TOKEN}` },
      });
      return new Uint8Array(await response.arrayBuffer());
    }

    return defaultResolver(resource);
  },
});
```

`resource` includes `type` (`"font" | "css" | "image"`), `url`, and `name`. Font providers can return `ResolvedFontResult` when a CSS response references multiple font files.

## Resource Caches

```typescript
import { render } from "satoru-render/single";
import { MemoryResourceCache } from "satoru-render/resources";

const resourceCache = new MemoryResourceCache({
  maxEntries: 1000,
  maxBytes: 128 * 1024 * 1024,
  ttl: 24 * 60 * 60 * 1000,
});

await render({
  value: html,
  width: 1200,
  height: 630,
  format: "png",
  resolveResource: resourceCache.wrap(),
});
```

| API | Description |
| --- | --- |
| `new MemoryResourceCache(options)` | In-memory cache that works in Node.js, browsers, and workers. |
| `cache.wrap(resolver?)` | Returns a cache-backed `ResourceResolver`. |
| `cache.clear()` | Clears the cache. |
| `cache.getStats()` | Returns `{ entries, totalBytes }`. |
| `new CacheStorageResourceCache(cacheName?)` | CacheStorage-backed cache for browsers and Cloudflare Workers. It does not cache compound font results. |
| `composeResourceResolvers(...resolvers)` | Composes multiple resolvers in order. |
