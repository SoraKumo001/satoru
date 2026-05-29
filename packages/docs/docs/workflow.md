---
sidebar_position: 4
title: 開発ワークフロー
---

# 開発と検証ワークフロー

## 4.1 ビルドコマンド

- `pnpm wasm:configure`: Ninja を使って CMake を構成します。
- `pnpm wasm:build`: C++ を Wasm にコンパイルします。本番ビルドでは Thin LTO と `-O3` を使用します。
- `pnpm build`: monorepo 全体をビルドします。

## 4.2 検証手順

1. **ビルド確認**: Wasm コンパイルが成功することを確認します。
2. **視覚確認**: `pnpm --filter visual-test convert-assets` を実行し、`temp/` の出力を確認します。
3. **ログ分析**: `--verbose` を使い、layout pass や cache hit/miss を確認します。

## 4.3 リリース検証と公開ワークフロー

公開時の安全性と API の一貫性を保つため、リリース前に次の手順を実行します。

1. **ローカルリリース検証**: 自動リリースチェックを実行します。
   ```bash
   pnpm release:check
   ```
   この script は次の項目を確認します。
   - `packages/satoru/package.json` と `packages/satoru/CHANGELOG.md` の version 整合性。
   - clean build (`pnpm build`)。
   - export された entrypoint と README、LICENSE、WASM binary、worker bundle などの存在。
   - 視覚回帰テスト。
   - package dry-run pack (`pnpm pack --dry-run`)。

2. **互換性エビデンス文書**:
   視覚互換性 matrix の文書が更新されていることを確認します。CI は未コミット差分の有無でこれを検証します。ローカルでは次のコマンドで再生成できます。
   ```bash
   pnpm docs:compatibility
   ```
   リリース PR を作成する前に、`docs/compatibility.md` の更新を必ずコミットします。

3. **Worker Pool のライフサイクル管理**:
   高スループットなレンダリングで `createSatoruWorker` を使う場合は、次の lifecycle control を理解しておきます。
   - `waitAll()`: dispatch 済みの render job がすべて完了したときに resolve する promise を返します。script 終了を完了まで待つ用途に使います。
   - `close()`: すべての worker thread/process を即座に終了します。memory leak や process の残留を防ぐため、cleanup 時に必ず呼びます。
   - `reset()`: 現在の worker を終了し、新しい worker pool を起動します。job timeout が起きた場合、native 実行の hang から復旧するため自動的に呼ばれます。
