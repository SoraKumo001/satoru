---
sidebar_position: 7
title: Diagnostics
---

# Diagnostics

```typescript
await render({
  value: html,
  width: 1200,
  height: 630,
  format: "png",
  diagnostics: true,
  onDiagnostics: (report) => {
    console.log(report.timings);
    console.log(report.resources);
    console.log(report.errors);
  },
});
```

`RenderDiagnostics` には `format`、`width`、`height`、`mediaType`、`timings`、`resources`、`fonts`、`warnings`、`errors` が含まれます。`errors[].code` には `LIMIT_TIMEOUT`、`LIMIT_RESOURCE_SIZE`、`LIMIT_TOTAL_SIZE`、`LIMIT_RESOURCE_COUNT`、`LIMIT_PROTOCOL_BLOCKED`、`LIMIT_HOST_BLOCKED` などが入ります。
