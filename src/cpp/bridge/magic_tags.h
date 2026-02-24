#ifndef SATORU_MAGIC_TAGS_H
#define SATORU_MAGIC_TAGS_H

#include <include/core/SkColor.h>

#include <cstdint>

namespace satoru {

/**
 * SVGポストプロセッサ用のマジックカラー指定
 * Rチャンネルの下位2ビットで種類を判別 (0: MagicTag, 1: MagicTagExtended)
 * Rチャンネルの上位6ビットとBチャンネルでインデックス (計14ビット) を指定
 * Gチャンネルで具体的な種類を指定
 */
enum class MagicTag : uint8_t {
    Shadow = 1,      // ボックスシャドウ
    TextShadow = 2,  // テキストシャドウ
    TextDraw = 3,    // テキスト描画属性（weight/italic等）
    FilterPush = 4,  // フィルタグループ開始
    FilterPop = 5,   // フィルタグループ終了
    LayerPush = 6,   // 不透明度グループ開始
    LayerPop = 7,    // 不透明度グループ終了
    ClipPush = 8,    // クリップ開始
    ClipPop = 9,     // クリップ終了
    GlyphPath = 10,  // グリフパス（defs/use最適化用）
    ClipPathPush = 11,
    ClipPathPop = 12,
};

enum class MagicTagExtended : uint8_t {
    ImageDraw = 0,  // 画像描画
    ConicGradient = 1,
    RadialGradient = 2,
    LinearGradient = 3,
    InlineSvg = 4,
};

/**
 * マジックカラーを生成するユーティリティ
 * 14ビットインデックス規則 (衝突回避のためRの末尾ビットを使用)
 * R: [Index High(6)][Type(2)]  (Type: 0=Magic, 1=Extended, 2,3=Non-Magic)
 * G: [TagValue(8)]
 * B: [Index Low(8)]
 */
inline SkColor make_magic_color(MagicTag tag, int index = 0) {
    uint8_t r = ((index >> 8) & 0x3F) << 2;  // Type 0
    uint8_t g = (uint8_t)tag;
    uint8_t b = (uint8_t)(index & 0xFF);
    return SkColorSetARGB(255, r, g, b);
}

inline SkColor make_magic_color(MagicTagExtended tag, int index = 0) {
    uint8_t r = (((index >> 8) & 0x3F) << 2) | 1;  // Type 1
    uint8_t g = (uint8_t)tag;
    uint8_t b = (uint8_t)(index & 0xFF);
    return SkColorSetARGB(255, r, g, b);
}

/**
 * マジックカラーをデコードする構造体
 */
struct DecodedMagicTag {
    bool is_magic = false;
    bool is_extended = false;
    int tag_value = -1;
    int index = -1;
};

inline DecodedMagicTag decode_magic_color(uint8_t r, uint8_t g, uint8_t b) {
    DecodedMagicTag result;
    uint8_t typeBit = r & 0x03;
    if (typeBit > 1) return result;  // 2,3 はマジックカラーではない

    result.is_magic = true;
    result.is_extended = (typeBit == 1);
    result.tag_value = g;
    result.index = ((r >> 2) << 8) | b;

    return result;
}

}  // namespace satoru

#endif  // SATORU_MAGIC_TAGS_H
