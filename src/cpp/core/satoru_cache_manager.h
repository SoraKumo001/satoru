#ifndef SATORU_CACHE_MANAGER_H
#define SATORU_CACHE_MANAGER_H

#include <string>
#include <vector>

#include "core/text/text_types.h"
#include "utils/lru_cache.h"

namespace satoru {

/**
 * プロジェクト全体のLRUキャッシュを一括管理するクラス
 */
class SatoruCacheManager {
   public:
    SatoruCacheManager() : shapingCache(1000), measureCache(2000), lineBreakCache(1000) {}

    /**
     * 全てのキャッシュをクリアする
     */
    void clearAll() {
        shapingCache.clear();
        measureCache.clear();
        lineBreakCache.clear();
    }

    // テキスト整形キャッシュ (キー: ShapingKey, 値: ShapedResult)
    LruCache<ShapingKey, ShapedResult, ShapingKeyHash> shapingCache;

    // テキスト計測キャッシュ (キー: MeasureKey, 値: MeasureResult)
    LruCache<MeasureKey, MeasureResult, MeasureKeyHash> measureCache;

    // 改行位置解析キャッシュ (キー: std::string, 値: 改行位置フラグ列)
    LruCache<std::string, std::vector<char>> lineBreakCache;
};

}  // namespace satoru

#endif  // SATORU_CACHE_MANAGER_H
