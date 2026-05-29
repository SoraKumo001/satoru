---
sidebar_position: 2
title: アーキテクチャ
---

# 技術仕様とアーキテクチャ

## 3.1 ディレクトリ構成

- `src/cpp/api`: Emscripten API 実装 (`satoru_api.cpp`)。
- `src/cpp/bridge`: モジュール間で共有する型定義と定数 (`bridge_types.h`, `magic_tags.h`)。
- `src/cpp/core`: レイアウト/レンダリングのコア、リソース管理、マスター CSS。
- `src/cpp/core/text`: モジュール化されたテキスト処理スタック。
  - `UnicodeService`: Unicode 処理 (utf8proc, libunibreak, BiDi) を集約。改行解析や BiDi レベルに **LRU キャッシュ**を使用します。
  - `TextLayout`: 高レベルのテキストレイアウト。統合された `TextAnalysis` パスと、シェイピング/計測結果の **LRU キャッシュ**を使用します。
  - `TextRenderer`: Skia ベースの描画、装飾、タグ付け。
  - `TextBatcher`: 同じスタイルの連続テキスト run を単一の `SkTextBlob` にまとめます。
- `src/cpp/renderers`: PNG、SVG、PDF、WebP 向けの renderer。
- `src/cpp/utils`: Skia ユーティリティ、LRU キャッシュ実装 (`lru_cache.h`)、ログマクロ。
- `src/cpp/libs`: 外部ライブラリ (`litehtml`, `skia`)。

## 3.2 レイアウトとレンダリングパイプライン

- **統合レイアウトパイプライン (O(N) 最適化)**: `measure()` と `place()` の pass を分離しています。
  - **キャッシュ機構**: `render_item` に `containing_block_context` と計測結果をキャッシュし、線形 $O(N)$ の計算量を維持します。
- **W3C Flexbox エンジン**: base sizing、main/cross axis resolution、finalization を含む多段解決。
- **W3C Grid エンジン**: template tracks、item placement、alignment による多段解決パイプライン。
- **Container Queries**: `container-type` (size/inline-size) と `container-name` を含む `@container` ルールをサポート。
- **高度な CSS Shapes と Filters**:
  - **clip-path**: `circle`、`ellipse`、`inset`、`polygon`、Skia path operation による `path()` をサポート。
  - **backdrop-filter**: `saveLayer` と Skia image filter (`blur()` など) で実装。
  - **Gradients**: focal point と spread method を含む `radial-gradient` の拡張サポート。
- **座標系と writing mode**:
  - **論理座標**: レイアウトと高レベル処理で使用。`inline` 軸と `block` 軸を含みます。
  - **物理座標**: Skia のネイティブ座標系 (x: 左から右、y: 上から下)。
  - **WritingModeContext**: 論理空間と物理空間を相互変換するコアユーティリティ (`src/cpp/core/logical_geometry.h`)。
  - **統合**: `litehtml::render_item` は margin、padding、border などの box model プロパティに論理メトリクスを使います。`TextRenderer` は writing mode に依存しない glyph 配置と装飾描画にこれを利用します。
- **SVG Pipeline v2**:
  - **高性能ストリームパーサー**: FSM ベースの parser により描画コマンド stream を効率的に処理。
  - **構造メタデータ**: 後処理のため、SVG 出力に CSS class と ID を保持。
  - **スタイル復元**: "Magic Colors" で style を encode し、regex で復元。
- **バイナリパイプライン**: `SkSurface` (PNG/WebP) または `SkPDFDocument` (PDF) への最適化された直接描画。

## 3.3 CSS エンジン (litehtml カスタマイズ)

- **Cascade と Specificity**: W3C の cascade order に準拠。
- **動的 length**: `calc()`、`min()`、`max()`、`clamp()` を再帰評価でサポート。`var()` と `env()` にも対応。
- **Logical Properties**: margin、padding、border、size の `*-inline-*` / `*-block-*` プロパティをネイティブサポート。
- **Modern CSS Functions と Colors**:
  - **Color**: `oklch()`、`oklab()`、`color-mix()`、`light-dark()`、relative color syntax。
  - **Masking**: Skia の composite layer による `mask` と `-webkit-mask` サポート。
- **国際化テキスト**:
  - **BiDi**: `SkUnicode` と内部 BiDi level 解決による双方向テキストサポート。
  - **Overflow**: inline-level overflow control を含む line box 内の `text-overflow: ellipsis`。
- **Decoration**: `wavy` underline を含む高度な text decoration。

## 3.4 リソース管理とグローバルキャッシュ

- **Font Caching**: URL と FNV-1a hash の 2 段 lookup により `SkTypeface` をグローバル管理。
- **Automatic Resolution**: Google Fonts provider 連携と、動的 CSS font rule の自動解析。
