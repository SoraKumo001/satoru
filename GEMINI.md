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

- **Project Overview**: [docs/overview.md](docs/overview.md)
- **Technical Specifications & Architecture**: [docs/architecture.md](docs/architecture.md)
- **Development & Verification Workflow**: [docs/workflow.md](docs/workflow.md)
