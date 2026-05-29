# GEMINI.md

This file serves as the entry point for agent instructions and project documentation.

## 🛠 Operational Guidelines (Critical)

- **Shell**: Use Windows PowerShell. Chain commands with `;` (do NOT use `&&`).
- **Execution**: Execute shell commands sequentially, one by one. Do NOT run multiple commands in parallel.
- **Git**: Read-only access by default (`status`, `diff`, `log`). No `add`/`commit`/`push` unless directed.
- **Reporting**: Always report status in the user's language.
- **Debugging**: Create and use a minimal JavaScript reproduction script to verify behaviors and fix bugs.

Full guidelines: [docs/guidelines.md](docs/guidelines.md)

---

## 📚 Project Documentation

- **Project Overview**: [packages/docs/docs/overview.md](packages/docs/docs/overview.md)
- **Technical Specifications & Architecture**: [packages/docs/docs/architecture.md](packages/docs/docs/architecture.md)
- **Development & Verification Workflow**: [packages/docs/docs/workflow.md](packages/docs/docs/workflow.md)
