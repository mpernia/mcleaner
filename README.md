# Mac Cleaner TUI

Terminal-based cleaner for macOS with a commander-like interface, risk-aware targeting, and safe defaults.

## Features
- Single-panel ncurses UI with keyboard-first navigation.
- Column-based tables for targets, preview, and results.
- Risk model: `safe`, `review`, `never`, `deny`.
- Hidden directory scan and risk filter shortcuts.
- Parallel cleaning workflow with live incremental result updates.
- Footer progress bar synchronized with processed directories.
- `--dry-run` mode for safe simulation.
- Logs and report output in `~/Library/Logs`.

## Requirements
- macOS
- C++20 compiler (clang++)
- ncurses
- make

## Build
```bash
make
```

## Run
```bash
./mcleaner
```

### Dry-run mode
```bash
./mcleaner --dry-run
```

### Help and version
```bash
./mcleaner --help
./mcleaner --version
```

## Key Shortcuts
- `Up/Down`: move
- `Space` / `1`: toggle current safe row
- `2`: select all safe rows
- `3`: deselect all
- `4`: preview table
- `5` / `Enter`: clean selected
- `6`: rescan
- `7`: scan hidden directories
- `8`: confirm-mark current hidden `review` row
- `/`: set risk filter (`all`, `safe`, `review`, `never`)
- `9`: help
- `q` / `0` / `F10`: quit

## Permissions
Some paths (for example `.Trash` or parts of `Library`) may require Full Disk Access for your terminal emulator.

If you see access-denied rows:
1. Open `System Settings` -> `Privacy & Security` -> `Full Disk Access`.
2. Enable your terminal app (Terminal/iTerm).
3. Restart the terminal and rescan.

## Logs and Reports
- Text log: `~/Library/Logs/mcleaner.log`
- Last JSON report: `~/Library/Logs/mcleaner-last-report.json`

## Homebrew Packaging
This repository includes a formula template at `packaging/homebrew/mcleaner.rb`.

Typical release flow:
1. Create and publish a GitHub release tag (for example `v0.1.0`).
2. Download the source tarball and compute `sha256`.
3. Update formula `url`, `sha256`, and `version` if needed.
4. Publish/update the formula in your Homebrew tap.

The Makefile supports install targets used by package managers:
```bash
make install PREFIX=/usr/local
make uninstall PREFIX=/usr/local
```

## License
MIT. See `LICENSE`.

