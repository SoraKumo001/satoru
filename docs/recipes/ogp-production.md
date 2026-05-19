# Production OGP Generation Recipe

This recipe demonstrates how to use Satoru for a production-grade OGP (Open Graph Protocol) image generation service. It combines **caching**, **rendering limits**, and **diagnostics** to ensure a safe, fast, and observable system.

## Key Features

1.  **Memory Caching**: Avoids redundant network requests for shared assets like fonts and branding images.
2.  **Strict Limits**: Prevents SSRF, oversized inputs, and runaway render durations.
3.  **Observability**: Uses diagnostics to track resource failures and performance.

## The Recipe (Node.js / Cloudflare Workers)

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

## Why this is Production-Ready

- **Security**: Blocking `http:` and `localhost` prevents basic SSRF attacks where a malicious user might try to make your server fetch internal metadata or files.
- **Stability**: The `timeoutMs` ensures that a complex CSS input won't hang your worker/server indefinitely.
- **Cost**: The `MemoryResourceCache` significantly reduces egress traffic and latency by keeping expensive Google Fonts or high-res branding images in memory.
- **Debuggability**: If an OGP image looks broken (e.g., missing font), the `onDiagnostics` callback tells you exactly which URL failed and why, without needing to reproduce it locally.
