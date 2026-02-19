#ifndef SATORU_MAGIC_TAGS_H
#define SATORU_MAGIC_TAGS_H

#include <cstdint>

namespace satoru {

/**
 * SVGポストプロセッサ用のマジックカラー指定
 * Rチャンネルは基本 0 (特殊要素は 1)
 * Gチャンネルで種類を判別
 * Bチャンネルでインデックスを指定
 */
enum class MagicTag : uint8_t {
    Shadow = 1,      // r=0, g=1: ボックスシャドウ
    TextShadow = 2,  // r=0, g=2: テキストシャドウ
    TextDraw = 3,    // r=0, g=3: テキスト描画属性（weight/italic等）
    FilterPush = 4,  // r=0, g=4: フィルタグループ開始
    FilterPop = 5,   // r=0, g=5: フィルタグループ終了
    LayerPush = 6,   // r=0, g=6: 不透明度グループ開始
    LayerPop = 7,    // r=0, g=7: 不透明度グループ終了
};

enum class MagicTagExtended : uint8_t {
    ImageDraw = 0,       // r=1, g=0: 画像描画
    ConicGradient = 1,   // r=1, g=1
    RadialGradient = 2,  // r=1, g=2
    LinearGradient = 3,  // r=1, g=3
    InlineSvg = 4,       // r=1, g=4
};

}  // namespace satoru

#endif  // SATORU_MAGIC_TAGS_H
