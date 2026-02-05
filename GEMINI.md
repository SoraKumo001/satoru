# GEMINI.md

## System Context & Operational Guidelines

This document defines the operational rules for Gemini when interacting with the local file system and shell environment.

### 1. Shell Environment: Windows PowerShell

You are operating in a **Windows PowerShell** environment (likely v5.1).

- **Constraint:** The standard PowerShell environment does not support the `&&` or `||` operators for command chaining.
- **Instruction:** You **MUST** use the semicolon `;` separator for sequential command execution.
- ❌ **Incorrect:** `mkdir test_dir && cd test_dir`
- ✅ **Correct:** `mkdir test_dir ; cd test_dir`

- **Alternative:** If strict error handling is needed (stop on failure), execute commands one by one or use `if ($?) { ... }`.

### 2. File Editing Protocol (mcp-text-editor)

To prevent `Hash Mismatch` errors when using `edit_file` or `write_file`, strictly adhere to the following sequence:

- **Rule 1: Read Before Write**
- ALWAYS execute `read_file` on the target file immediately before attempting an edit.
- Do not rely on your context window for file content; it may be outdated or hallucinated (especially whitespace).

- **Rule 2: Byte-Perfect `old_text**`
- When using `edit_file` (search & replace), the `old_text` field must be a **byte-for-byte exact match** of the file content.
- **Whitespace:** Do NOT change indentation (tabs vs spaces) or trim trailing spaces.
- **Line Endings:** Respect the existing line endings (CRLF vs LF). Do not normalize them in the `old_text`.

- **Rule 3: Atomic Edits**
- Prefer replacing smaller, unique blocks of code (e.g., a single function signature or a specific logic block) rather than large chunks. This reduces the risk of hash collisions.

### 3. Troubleshooting

- If you encounter a `Hash Mismatch` error:

1. Stop the current chain.
2. Re-read the file using `read_file`.
3. Verify the exact content of the lines you intend to change.
4. Retry the edit with the newly confirmed content.
