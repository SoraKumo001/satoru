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

## 1.4 Serena Tool Constraints

- **execute_shell_command**: Use of `execute_shell_command` is strictly prohibited. Use `run_shell_command` instead.
- **serena\_\_read_file**: Use of `serena__read_file` is strictly prohibited. Use `read_text_file` or other alternative tools instead.
- **replace_content**: Use of `replace_content` is strictly prohibited. Use `edit_file` or other alternative tools instead.
- **find_symbol**: Use of `find_symbol` is strictly prohibited. Use `get_symbols_overview` and `read_text_file` with line numbers or other alternative tools instead.
