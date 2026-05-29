# Satoru Docs

This package contains the Docusaurus documentation site for Satoru.

## Installation

```bash
pnpm install
```

## Local Development

```bash
pnpm docs:dev
```

This starts the local Docusaurus development server. Most content and style changes are reflected without restarting the server.

## Build

```bash
pnpm docs:build
```

This generates the static site into `packages/docs/build`.

## Preview Production Output

After building, preview the generated site with:

```bash
pnpm --filter satoru-docs serve
```

## Content

- Japanese source documents live in `docs/`.
- English translations live in `i18n/en/docusaurus-plugin-content-docs/current/`.
- Shared UI translations live under `i18n/en/`.

Run `pnpm --filter satoru-docs typecheck` and `pnpm docs:build` before publishing documentation changes.
