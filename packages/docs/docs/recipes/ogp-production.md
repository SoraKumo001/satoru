---
sidebar_position: 1
title: 本番 OGP 生成
---

# 本番 OGP 生成 recipe

この recipe は、本番向けの OGP (Open Graph Protocol) 画像生成 service で Satoru を使う方法を示します。**caching**、**rendering limits**、**diagnostics** を組み合わせ、安全で高速かつ観測可能な system を構成します。

## 主なポイント

1.  **Memory Caching**: font や branding image などの共有 asset に対する重複 network request を避けます。
2.  **Strict Limits**: SSRF、過大な input、長時間化する render を防ぎます。
3.  **Observability**: diagnostics を使い、resource failure と performance を追跡します。

## Recipe (Node.js / Cloudflare Workers)

```typescript
import { render } from "satoru-render/single";
import { MemoryResourceCache } from "satoru-render/resources";

// 1. Initialize a long-lived cache
const resourceCache = new MemoryResourceCache({
  maxEntries: 1000,
  maxBytes: 128 * 1024 * 1024, // 128MB
  ttl: 24 * 60 * 60 * 1000,    // 24 hours
});

export async function generateOGP(html: string, url: string) {
  return await render({
    value: html,
    width: 1200,
    height: 630,
    format: "png",
    
    // 2. Wrap the resolver with caching
    resolveResource: resourceCache.wrap(),
    
    // 3. Set strict safety limits
    limits: {
      timeoutMs: 5000,           // 5s total timeout
      maxResourceBytes: 5 * 1024 * 1024, // 5MB per image/font
      maxTotalResourceBytes: 20 * 1024 * 1024, // 20MB total
      maxResourceCount: 15,      // No more than 15 resources
      allowedProtocols: ["https:"],
      blockedHosts: ["localhost", "127.0.0.1"],
    },
    
    // 4. Enable diagnostics for monitoring
    diagnostics: true,
    onDiagnostics: (report) => {
      // Log failed resources to your tracking system (Sentry, Datadog, etc.)
      const failures = report.resources.filter(r => r.status === "failed");
      if (failures.length > 0) {
        console.warn(`[OGP] Render had ${failures.length} resource failures for ${url}`);
      }
      
      // Monitor performance
      if (report.timings.wasmRender > 1000) {
        console.warn(`[OGP] Slow render detected: ${report.timings.wasmRender}ms`);
      }
    }
  });
}
```

## 本番向けである理由

- **Security**: `http:` と `localhost` を block することで、悪意ある user が server に内部 metadata や file を取得させる基本的な SSRF 攻撃を防ぎます。
- **Stability**: `timeoutMs` により、複雑な CSS input が worker/server を無期限に hang させることを防ぎます。
- **Cost**: `MemoryResourceCache` は高価な Google Fonts や高解像度 branding image を memory に保持し、egress traffic と latency を大きく減らします。
- **Debuggability**: OGP image が壊れて見える場合 (font missing など)、`onDiagnostics` callback から、どの URL がなぜ失敗したかを local 再現なしで確認できます。
