# Changelog

All notable changes to this project will be documented in this file.

## v1.0.13 - 2026-05-19

- **Diagnostics & Tooling**:
  - [Feature] Stabilize `RenderDiagnostics` schema (version 1) for reliable automated analysis.
  - [Feature] Add Inspection Workspace to Playground with Resources, Fonts, Timings, and Logs tabs.
  - [Improvement] Make CLI `--json-report` output deterministic by sorting resources and fonts.
  - [Improvement] Add regression tests for diagnostics and resource resolution.

## v1.0.12 - 2026-05-18

- **Performance**:
  - [Optimization] Add performance optimizations for rendering speed.

## v1.0.11 - 2026-05-17

- **Rendering Engine & Text Layout**:
  - [Feature] Implement text layout measurement engine with vertical writing mode and profiling support.
  - [Feature] Implement core interfaces and font resolution logic, including Google Fonts resolution.
- **Core API & Infrastructure**:
  - [Feature] Implement core Satoru module interface, resource resolution, and C++ API bindings.
  - [Improvement] Initialize and adjust `satoru-render` package configuration and exports.

## v1.0.10 - 2026-04-26

- **Rendering Capabilities**:
  - [Feature] Implement PNG, PDF, and SVG rendering engines with Skia-based text and layout management.
- **Configuration & Build**:
  - [Improvement] Initialize `package.json` with multi-environment exports.

## v1.0.9 - 2026-04-25

- **Rendering Engine**:
  - [Feature] Add `backdrop-filter` support via Skia.

## v1.0.7 - 2026-04-24

- **Rendering Engine**:
  - [Feature] Implement `backdrop-filter` in Skia container.
  - [Feature] Add initial font rendering infrastructure.
  - [Feature] Implement PDF and PNG rendering engines using Skia and litehtml integration.

## v1.0.5 - 2026-04-23

- **Core Capabilities**:
  - [Feature] Implement multi-format rendering supporting PNG, WebP, PDF, and SVG engines.

## v1.0.4 - 2026-04-22

- **Configuration & Build**:
  - [Improvement] Initialize `package.json` for `satoru-render`.

## v1.0.3 - 2026-04-22

- **Rendering Engine**:
  - [Feature] Implement core rendering engine including litehtml integration and showcase page scaffolding.

## v1.0.2 - 2026-04-21

- **Rendering Engine**:
  - [Feature] Implement C++ rendering engine with bridge support and add initial assets.
  - [Feature] Add WebP renderer support and expand core TypeScript interfaces.

## v0.0.7 - 2026-03-03

- **CSS Support & Rendering**:
  - [Feature] Add support for CSS gradients in the `border-image` property.
  - [Feature] Implement full SVG mask rendering, including images and gradients.

## v0.0.6 - 2026-03-03

- **CSS Support**:
  - [Feature] Implement `object-fit` CSS property for images (reflected in rendering and visual tests).

## v0.0.5 - 2026-03-03

- **General**:
  - [Improvement] Version bump and cleanup of image rendering debug output.

## v0.0.4 - 2026-02-27

- **Asset Management**:
  - [Feature] Allow direct asset linking via URL parameters.
  - [Optimization] Improve asset loading logic.

## v0.0.3 - 2026-02-26

- **Code Quality**:
  - [Refactor] Format text layout and rendering code for improved readability.

## v0.0.2 - 2026-02-25

- **Performance & Build**:
  - [Optimization] Optimize WASM builds using Thin LTO and `-O3`.
  - [Improvement] Improve height handling in core API.

## v0.0.1 - 2026-02-23

- **Architecture & Ecosystem**:
  - [Feature] Major structural changes including entry point setup for multiple environments (Node, Workers, Deno, etc.).
  - [Feature] Add Cloudflare OGP generator package and Deno OGP service.
  - [Feature] Implement automatic font resolution with configurable font map and Google Fonts integration.
  - [Improvement] Rename package from `satoru` to `satoru-render`.

## v1.0.0 - 2026-02-06

- **Initial Setup & Monorepo**:
  - [Refactor] Refactor to pnpm monorepo structure.
  - [Feature] Create initial `@satoru/core` WASM package and add Cloudflare Workers (workerd) support.
  - [Feature] Add visual regression testing suite and comprehensive HTML/CSS test assets.
