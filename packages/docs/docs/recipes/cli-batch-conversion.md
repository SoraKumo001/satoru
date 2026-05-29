---
sidebar_position: 4
title: CLI 一括変換
---

# CLI で HTML や URL を一括変換する

`satoru-render` には CLI が含まれています。HTML file や URL を PNG、WebP、PDF、SVG に変換できます。

## 単発変換

```bash
npx satoru-render ./input.html -o output.png -w 1200
npx satoru-render https://example.com -o page.pdf -w 1280 -f pdf
npx satoru-render ./input.html -o output.webp -f webp --verbose
```

URL や HTML file は、default で JSDOM hydration を通します。静的 HTML をそのまま処理したい場合は `--no-jsdom` を指定します。

```bash
npx satoru-render ./static.html -o static.png --no-jsdom
```

## 複数 file を変換する

PowerShell:

```powershell
Get-ChildItem .\pages -Filter *.html | ForEach-Object {
  npx satoru-render $_.FullName -o ".\out\$($_.BaseName).png" -w 1200 --no-jsdom
}
```

Bash:

```bash
mkdir -p out
for file in pages/*.html; do
  name="$(basename "$file" .html)"
  npx satoru-render "$file" -o "out/$name.png" -w 1200 --no-jsdom
done
```

## Diagnostics と安全制限

外部 URL を変換する job では、timeout と resource limit を付けます。`--json-report` を使うと、失敗した resource や diagnostics を file に残せます。

```bash
npx satoru-render https://example.com \
  -o example.png \
  -w 1200 \
  --timeout 5000 \
  --max-resource-count 50 \
  --max-resource-bytes 5000000 \
  --max-total-resource-bytes 20000000 \
  --allowed-protocols https: \
  --json-report example.report.json
```

## よく使う option

| Option | 用途 |
| --- | --- |
| `-o, --output` | 出力 file path。 |
| `-w, --width` / `-h, --height` | layout viewport の幅と高さ。 |
| `-f, --format` | `svg`, `png`, `webp`, `pdf` を指定。 |
| `--no-jsdom` | JSDOM hydration を無効化。 |
| `--media print` | `@media print` を有効化。 |
| `--json-report` | diagnostics report を JSON で保存。 |
| `--timeout` | render timeout。 |
| `--allowed-protocols` | 許可する protocol の comma-separated list。 |
| `--allowed-hosts` / `--blocked-hosts` | host の allow/block list。 |
