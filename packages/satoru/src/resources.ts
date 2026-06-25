import { RequiredResource, ResourceResolver, ResolvedFontResult } from "./core.js";

export interface ResourceCacheEntry {
  data: Uint8Array | ResolvedFontResult;
  bytes: number;
  created: number;
  lastHit: number;
}

export interface ResourceCacheOptions {
  /** Maximum number of entries to keep in cache. Default: 100 */
  maxEntries?: number;
  /** Maximum total bytes to keep in cache. Default: 64MB */
  maxBytes?: number;
  /** Time to live in milliseconds. Default: 0 (no TTL) */
  ttl?: number;
}

/**
 * In-memory resource cache that works in all environments.
 */
export class MemoryResourceCache {
  private cache = new Map<string, ResourceCacheEntry>();
  private totalBytes = 0;
  private options: Required<ResourceCacheOptions>;

  constructor(options: ResourceCacheOptions = {}) {
    this.options = {
      maxEntries: options.maxEntries ?? 100,
      maxBytes: options.maxBytes ?? 64 * 1024 * 1024,
      ttl: options.ttl ?? 0,
    };
  }

  async get(url: string): Promise<Uint8Array | ResolvedFontResult | null> {
    const entry = this.cache.get(url);
    if (!entry) return null;

    if (this.options.ttl && Date.now() - entry.created > this.options.ttl) {
      this.delete(url);
      return null;
    }

    entry.lastHit = Date.now();
    return entry.data;
  }

  async set(url: string, data: Uint8Array | ResolvedFontResult): Promise<void> {
    const bytes = this.calculateBytes(data);
    
    if (this.options.maxBytes && bytes > this.options.maxBytes) return;
    
    this.evict(bytes);

    this.cache.set(url, {
      data,
      bytes,
      created: Date.now(),
      lastHit: Date.now(),
    });
    this.totalBytes += bytes;
  }

  private delete(url: string) {
    const entry = this.cache.get(url);
    if (entry) {
      this.totalBytes -= entry.bytes;
      this.cache.delete(url);
    }
  }

  private evict(incomingBytes: number) {
    if (this.cache.size >= this.options.maxEntries || (this.options.maxBytes && this.totalBytes + incomingBytes > this.options.maxBytes)) {
      const sorted = Array.from(this.cache.entries()).sort((a, b) => a[1].lastHit - b[1].lastHit);
      for (const [url, entry] of sorted) {
        this.delete(url);
        if (this.cache.size < this.options.maxEntries && (!this.options.maxBytes || this.totalBytes + incomingBytes <= this.options.maxBytes)) {
          break;
        }
      }
    }
  }

  private calculateBytes(data: Uint8Array | ResolvedFontResult): number {
    if (data instanceof Uint8Array) return data.byteLength;
    let sum = data.css.byteLength;
    for (const f of data.fonts) sum += f?.data?.byteLength ?? 0;
    return sum;
  }

  /**
   * Returns a ResourceResolver that wraps another resolver (or the default one) with this cache.
   */
  wrap(resolver?: ResourceResolver): ResourceResolver {
    return async (resource, defaultResolver) => {
      const cached = await this.get(resource.url);
      if (cached) return cached;

      const result = await (resolver ? resolver(resource, defaultResolver) : defaultResolver(resource));
      if (result) {
        // Normalize to Uint8Array if it's an ArrayBufferView but not ResolvedFontResult
        let dataToCache: Uint8Array | ResolvedFontResult;
        if (typeof (result as any).css !== 'undefined') {
          dataToCache = result as ResolvedFontResult;
        } else {
          const view = result as ArrayBufferView;
          dataToCache = new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
        }
        
        await this.set(resource.url, dataToCache);
      }
      return result;
    };
  }

  /**
   * Clear the cache.
   */
  clear() {
    this.cache.clear();
    this.totalBytes = 0;
  }

  /**
   * Get cache statistics.
   */
  getStats() {
    return {
      entries: this.cache.size,
      totalBytes: this.totalBytes,
    };
  }
}

/**
 * Composes multiple resource resolvers into a single one.
 * They are executed in order; each one can choose to call the next one in the chain.
 */
export function composeResourceResolvers(...resolvers: ResourceResolver[]): ResourceResolver {
  return (resource, defaultResolver) => {
    const run = (i: number): Promise<Uint8Array | ArrayBufferView | ResolvedFontResult | null> => {
      const resolver = resolvers[i];
      if (resolver) {
        return resolver(resource, (r) => run(i + 1) as any);
      }
      return defaultResolver(resource);
    };
    return run(0);
  };
}

/**
 * CacheStorage adapter for browser and Cloudflare Workers.
 */
export class CacheStorageResourceCache {
  constructor(private cacheName: string = "satoru-resources") {}

  private async getCache() {
    return await caches.open(this.cacheName);
  }

  async get(url: string): Promise<Uint8Array | null> {
    try {
      const cache = await this.getCache();
      const response = await cache.match(url);
      if (response) {
        return new Uint8Array(await response.arrayBuffer());
      }
    } catch (e) {
      console.warn("CacheStorage error:", e);
    }
    return null;
  }

  async set(url: string, data: Uint8Array): Promise<void> {
    try {
      const cache = await this.getCache();
      await cache.put(url, new Response(data as any, {
        headers: { "Content-Type": "application/octet-stream" }
      }));
    } catch (e) {
      console.warn("CacheStorage error:", e);
    }
  }

  wrap(resolver?: ResourceResolver): ResourceResolver {
    return async (resource, defaultResolver) => {
      // Note: CacheStorage only supports Uint8Array easily, not ResolvedFontResult
      if (resource.type === "font") {
         // Fonts are complex because they might return multiple files. 
         // For now we don't cache ResolvedFontResult in CacheStorage.
         return resolver ? resolver(resource, defaultResolver) : defaultResolver(resource);
      }

      const cached = await this.get(resource.url);
      if (cached) return cached;

      const result = await (resolver ? resolver(resource, defaultResolver) : defaultResolver(resource));
      if (result && result instanceof Uint8Array) {
        await this.set(resource.url, result);
      }
      return result;
    };
  }
}
