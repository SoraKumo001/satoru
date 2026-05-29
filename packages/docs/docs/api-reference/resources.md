---
sidebar_position: 4
title: Resources
---

# Resources

## ResourceResolver

`resolveResource` は font、CSS、image の解決処理を hook します。独自 fetch、認証 header、cache、SSRF 対策などに使います。

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

`resource` には `type` (`"font" | "css" | "image"`)、`url`、`name` が含まれます。font provider のように CSS と複数 font file を返す場合は `ResolvedFontResult` を返せます。

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

| API | 説明 |
| --- | --- |
| `new MemoryResourceCache(options)` | Node.js、browser、worker で使える in-memory cache。 |
| `cache.wrap(resolver?)` | cache 付き `ResourceResolver` を返します。 |
| `cache.clear()` | cache を空にします。 |
| `cache.getStats()` | `{ entries, totalBytes }` を返します。 |
| `new CacheStorageResourceCache(cacheName?)` | browser / Cloudflare Workers の CacheStorage を使う cache。font の複合 result は cache しません。 |
| `composeResourceResolvers(...resolvers)` | 複数 resolver を順に合成します。 |
