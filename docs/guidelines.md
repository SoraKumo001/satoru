# Operational Guidelines (Agent Specific)

This section defines the core rules for an agent's behavior within this project.

## 1.1 Shell Environment: Windows PowerShell

- **Command Chaining**: Use `;` for sequential execution (do NOT use `&&` or `||`).
- **Constraint**: Do not execute multiple shell commands in parallel.

## 1.2 Communication & Troubleshooting

- **Progress Reporting**: Always report your current status in the user's language concisely before and during each step.
- **Hash Mismatch**: If encountered, immediately re-read the file with `get_text_file_contents` to synchronize. If the issue persists, overwrite the entire file using `write_file`.

## 1.3 Git Usage

- **No Write Operations**: Do not perform `git add`, `commit`, `push`, etc., unless explicitly and specifically instructed by the user.
- **Read-Only Access**: `status`, `diff`, and `log` are permitted and recommended for gathering context.
