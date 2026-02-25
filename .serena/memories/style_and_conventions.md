# Code Style & Conventions

## C++ Guidelines
- **Formatting**: Use `pnpm format` which invokes `clang-format` with the project's `.clang-format` configuration.
- **Naming**: 
  - Functions and variables: `snake_case` (e.g., `api_html_to_svg`).
  - Classes: `PascalCase` (e.g., `SatoruInstance`).
  - Private members often have no prefix, but sometimes context-dependent.
- **Includes**: Grouped by project-local and library-local.

## TypeScript Guidelines
- **Formatting**: Follows standard Prettier/ESLint rules defined in the project.
- **Naming**:
  - Functions/Variables: `camelCase`.
  - Classes/Interfaces: `PascalCase`.
- **Types**: Use strict typing where possible.
