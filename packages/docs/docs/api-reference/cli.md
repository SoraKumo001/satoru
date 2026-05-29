---
sidebar_position: 6
title: CLI
---

# CLI

`satoru-render` package は CLI も提供します。local HTML file または URL を入力として受け取り、SVG、PNG、WebP、PDF を生成できます。

```bash
# local HTML を PNG に変換
npx satoru-render input.html -o output.png

# URL を PDF に変換
npx satoru-render https://example.com -o site.pdf -w 1280 -f pdf

# 出力拡張子から format を推定
npx satoru-render input.html -o output.webp

# JSDOM hydration を無効化
npx satoru-render https://example.com --no-jsdom -o example.png

# 診断 report を JSON に出力
npx satoru-render input.html -o output.png --json-report report.json
```

既定では `width` は `800`、`format` は `png`、JSDOM hydration は有効です。`-f` / `--format` を指定しない場合は、`--output` の拡張子が `svg`、`png`、`webp`、`pdf` のいずれかなら、その拡張子から format を推定します。

## CLI Options

| Option | 説明 |
| --- | --- |
| `<input-file-or-url>` | 入力 HTML file または URL。必須です。 |
| `-o, --output <path>` | 出力 file path。省略時は input 名または `output` に format 拡張子を付けます。 |
| `-w, --width <number>` | viewport width。既定は `800`。 |
| `-h, --height <number>` | viewport height。省略時は content height から自動計算。 |
| `-f, --format <format>` | 出力形式。`svg`、`png`、`webp`、`pdf`。 |
| `--json-report <path>` | diagnostics report を JSON file に書き出します。 |
| `--no-jsdom` | JSDOM hydration を無効化し、file 内容または URL を直接 render します。 |
| `--media <type>` | CSS media type。`screen` または `print`。 |
| `--timeout <ms>` | render timeout。 |
| `--max-resource-bytes <bytes>` | 1 resource あたりの最大 byte 数。 |
| `--max-total-resource-bytes <bytes>` | 全 resource 合計の最大 byte 数。 |
| `--max-resource-count <count>` | 読み込む resource 数の上限。 |
| `--allowed-protocols <list>` | 許可する protocol の comma-separated list。例: `https:,data:`。 |
| `--allowed-hosts <list>` | 許可する hostname の comma-separated list。 |
| `--blocked-hosts <list>` | 拒否する hostname の comma-separated list。 |
| `--verbose` | 詳細 log を stderr に出力します。 |
| `--help` | help を表示します。 |

## CLI と JSDOM

CLI は既定で JSDOM hydration を試みます。URL や script を含む local HTML を入力した場合、hydration 後の DOM を Satoru に渡します。`jsdom` が使えない場合や hydration に失敗した場合は、`--verbose` で理由を確認できます。静的 HTML をそのまま render したい場合は `--no-jsdom` を指定してください。

```bash
npx satoru-render ./page.html --no-jsdom -o page.svg
```

## CLI で安全制限をかける

外部 URL や user-generated HTML を扱う場合は、resource 制限を付けることを推奨します。

```bash
npx satoru-render https://example.com \
  -o output.png \
  --timeout 5000 \
  --allowed-protocols https: \
  --blocked-hosts localhost,127.0.0.1 \
  --max-resource-count 20 \
  --max-total-resource-bytes 20971520
```
