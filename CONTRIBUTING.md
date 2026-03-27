# Contributing

Thanks for your interest in contributing to Mac Cleaner TUI.

## Ground Rules
- Keep safety-first behavior as the top priority.
- Do not weaken risk protections (`never`, `review`, access denied).
- Keep keyboard UX consistent and predictable.

## Development Setup
1. Ensure macOS with `clang++`, `make`, and `ncurses` available.
2. Build locally:
   ```bash
   make
   ```
3. Run:
   ```bash
   ./mcleaner
   ```
4. Optional dry-run:
   ```bash
   ./mcleaner --dry-run
   ```

## Coding Guidelines
- Use C++20.
- Prefer clear, explicit naming over clever abstractions.
- Keep new features modular (`Cleaner` logic vs `Tui` rendering/interaction).
- Preserve ASCII source unless a file already requires Unicode.

## Safety Checklist for Changes
- [ ] No new destructive path behavior outside expected scope.
- [ ] Protected/default directories remain protected unless explicitly intended.
- [ ] Access-denied behavior is still explicit and user-friendly.
- [ ] `--dry-run` continues to avoid real deletion.

## Pull Request Guidance
- Explain the user impact and safety impact.
- Include test steps (exact keys/commands).
- Provide before/after screenshots or terminal captures for TUI changes if possible.

## Release and Homebrew Notes
- Keep versioning aligned with release tags (`vX.Y.Z`).
- Verify `--version` output before publishing.
- Update `packaging/homebrew/mcleaner.rb` with real `homepage`, `url`, and `sha256`.

## Commit Message Style (recommended)
- Use concise imperative subject lines.
- Include why the change exists, not just what changed.
