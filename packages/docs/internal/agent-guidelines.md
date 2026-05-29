---
title: Agent Guidelines
---

# Agent Guidelines

This internal note was removed from the public documentation sidebar because it describes project-specific agent behavior rather than user-facing Satoru usage.

## Shell Environment: Windows PowerShell

- Use `;` for sequential execution. Do not use `&&` or `||`.
- Do not execute multiple shell commands in parallel.

## Communication and Troubleshooting

- Report current status concisely in the user's language before and during each step.
- If a file synchronization issue occurs, re-read the file before applying updates.

## Git Usage

- Do not run `git add`, `commit`, `push`, or similar write operations unless the user explicitly asks for them.
- Use `status`, `diff`, and `log` for read-only context gathering.
