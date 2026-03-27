# Product Requirements Document (PRD)

## Product Name
Mac Cleaner TUI

## Objective
Provide a terminal-first cleaner for macOS that helps users identify and remove temporary, cache, and user-generated cleanup candidates safely.

## Problem Statement
macOS users often accumulate temporary files, caches, logs, and stale folders over time. Existing GUI tools may be too heavy, opaque, or not terminal-friendly. Users need a transparent, keyboard-driven tool with strong safety controls.

## Target Users
- Developers and power users who work in Terminal/iTerm.
- Users who want a CCleaner/BleachBit-like workflow in a TUI.

## Goals
- Fast keyboard-driven experience.
- Clear risk classification (`safe`, `review`, `never`, `deny`).
- Safe defaults with explicit confirmation for risky operations.
- Preview and cleaning results in column-based tables.

## Non-Goals
- System-wide deep cleaning requiring privileged daemons.
- Automatic bypass of macOS privacy permissions.
- GUI desktop application.

## Functional Requirements
1. Discover and display cleanup targets in a single-panel TUI.
2. Include base targets (Trash, Caches, Logs, Temp) and user home directories.
3. Support hidden directory scan (`7`) and risk-based filtering (`/all`, `/safe`, `/review`, `/never`).
4. Allow marking/unmarking safe targets and controlled promotion of hidden review targets via `8` confirmation flow.
5. Execute clean operation with progress and live incremental result updates.
6. Support `--dry-run` mode (no deletion).
7. Persist operational logs and JSON report under `~/Library/Logs`.

## Safety Requirements
- Never clean paths outside user-safe scope.
- Keep protected/default user folders as non-selectable when classified `never`.
- Require explicit confirmation for clean action.
- Require explicit confirmation for `review` hidden target activation.
- Detect and communicate access-denied cases (TCC/Full Disk Access).

## UX Requirements
- Commander-style action bar with keyboard shortcuts.
- Consistent table layouts for list, preview, and results.
- Footer progress bar during cleaning operations.
- Modal dialogs for warning/error flows.

## Technical Requirements
- Language: C++20.
- UI: ncurses.
- Build: Makefile.
- Platform: macOS.

## Success Metrics
- User can complete scan -> select -> clean flow via keyboard only.
- Dry-run and live run produce understandable, structured results.
- No accidental deletion of `never` targets under default behavior.

## Future Improvements
- Persist user preferences across sessions.
- Optional JSON schema versioning for reports.
- Improved permission diagnostics and guided setup flow.
