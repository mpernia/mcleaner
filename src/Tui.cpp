#include "Tui.h"

#include <ncurses.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {
enum ColorPairId {
    COLOR_HEADER = 1,
    COLOR_SAFE = 2,
    COLOR_SKIP = 3,
    COLOR_STATUS = 4,
    COLOR_SELECTED = 5,
    COLOR_BAR_BG = 6,
    COLOR_BAR_KEY = 7,
    COLOR_MODAL_ERROR = 8,
    COLOR_MODAL_WARNING = 9,
    COLOR_HIDDEN = 10,
    COLOR_STATUS_OK = 11,
    COLOR_STATUS_WARN = 12,
    COLOR_STATUS_ERR = 13,
};

void initColors() {
    if (!has_colors()) {
        return;
    }

    start_color();
    use_default_colors();
    init_pair(COLOR_HEADER, COLOR_CYAN, -1);
    init_pair(COLOR_SAFE, COLOR_GREEN, -1);
    init_pair(COLOR_SKIP, COLOR_YELLOW, -1);
    init_pair(COLOR_STATUS, COLOR_BLACK, COLOR_CYAN);
    init_pair(COLOR_SELECTED, COLOR_WHITE, -1);
    init_pair(COLOR_BAR_BG, COLOR_WHITE, COLOR_BLUE);
    init_pair(COLOR_BAR_KEY, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_MODAL_ERROR, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_MODAL_WARNING, COLOR_BLACK, COLOR_YELLOW);
    init_pair(COLOR_HIDDEN, COLOR_CYAN, -1);
    init_pair(COLOR_STATUS_OK, COLOR_GREEN, -1);
    init_pair(COLOR_STATUS_WARN, COLOR_YELLOW, -1);
    init_pair(COLOR_STATUS_ERR, COLOR_RED, -1);
}

void drawCentered(int row, const std::string& text) {
    const int cols = COLS;
    const int x = std::max(0, (cols - static_cast<int>(text.size())) / 2);
    mvprintw(row, x, "%s", text.c_str());
}

void drawBottomBar() {
    const int y = LINES - 1;
    const bool colors = has_colors();

    if (colors) {
        attron(COLOR_PAIR(COLOR_BAR_BG));
    } else {
        attron(A_REVERSE);
    }
    mvhline(y, 0, ' ', COLS);

    struct BottomAction {
        std::string key;
        std::string label;
        std::string compactLabel;
    };
    const std::vector<BottomAction> actions = {
        {"1", "Toggle", "Tog"},
        {"2", "All", "All"},
        {"3", "None", "None"},
        {"4", "Preview", "Prev"},
        {"5", "Clean", "Clean"},
        {"6", "Rescan", "Scan"},
        {"7", "Hidden", "Hid"},
        {"8", "RiskMark", "Risk"},
        {"9", "Help", "Help"},
        {"10", "Quit", "Quit"},
    };

    enum class LabelMode {
        Full,
        Compact,
        KeyOnly,
    };

    auto neededWidth = [&actions](LabelMode mode) {
        int width = 0;
        for (const auto& action : actions) {
            const int keyBoxWidth = std::max(3, static_cast<int>(action.key.size()) + 2);
            width += keyBoxWidth;
            if (mode != LabelMode::KeyOnly) {
                const std::string& label = (mode == LabelMode::Compact) ? action.compactLabel : action.label;
                width += 1 + static_cast<int>(label.size());
            }
            width += 2;
        }
        return width;
    };

    LabelMode mode = LabelMode::Full;
    if (neededWidth(LabelMode::Full) >= COLS) {
        mode = LabelMode::Compact;
    }
    if (neededWidth(LabelMode::Compact) >= COLS) {
        mode = LabelMode::KeyOnly;
    }

    int x = 0;
    for (const auto& action : actions) {
        const std::string& key = action.key;
        const std::string& label = (mode == LabelMode::Compact) ? action.compactLabel : action.label;
        const int keyBoxWidth = std::max(3, static_cast<int>(key.size()) + 2);
        const int labelWidth = (mode == LabelMode::KeyOnly) ? 0 : static_cast<int>(label.size());
        const int segmentWidth = keyBoxWidth + ((mode == LabelMode::KeyOnly) ? 0 : (1 + labelWidth)) + 2;

        if (x + segmentWidth >= COLS) {
            break;
        }

        if (colors) {
            attron(COLOR_PAIR(COLOR_BAR_KEY) | A_BOLD);
        } else {
            attron(A_BOLD | A_REVERSE);
        }
        mvhline(y, x, ' ', keyBoxWidth);
        const int keyX = x + std::max(0, (keyBoxWidth - static_cast<int>(key.size())) / 2);
        mvprintw(y, keyX, "%s", key.c_str());
        if (colors) {
            attroff(COLOR_PAIR(COLOR_BAR_KEY) | A_BOLD);
            attron(COLOR_PAIR(COLOR_BAR_BG));
        } else {
            attroff(A_BOLD | A_REVERSE);
            attron(A_REVERSE);
        }

        if (mode != LabelMode::KeyOnly) {
            mvprintw(y, x + keyBoxWidth + 1, "%s", label.c_str());
        }
        x += segmentWidth;
    }

    if (colors) {
        attroff(COLOR_PAIR(COLOR_BAR_BG));
    } else {
        attroff(A_REVERSE);
    }
}

std::string fitText(const std::string& text, int width);

void drawStatusBar(const std::string& text) {
    mvhline(LINES - 2, 0, ' ', COLS);
    if (has_colors()) {
        attron(COLOR_PAIR(COLOR_STATUS));
    } else {
        attron(A_REVERSE);
    }

    mvhline(LINES - 2, 0, ' ', COLS);
    mvprintw(LINES - 2, 1, "%s", fitText(text, std::max(1, COLS - 2)).c_str());

    if (has_colors()) {
        attroff(COLOR_PAIR(COLOR_STATUS));
    } else {
        attroff(A_REVERSE);
    }
}

void drawFooterProgress(std::size_t done, std::size_t total) {
    const std::size_t safeTotal = std::max<std::size_t>(1, total);
    const std::size_t safeDone = std::min(done, safeTotal);
    const int y = LINES - 1;
    const int barX = 2;
    const int barWidth = std::max(10, COLS - 24);
    const int filled = static_cast<int>((static_cast<double>(safeDone) / static_cast<double>(safeTotal)) * static_cast<double>(barWidth));

    mvhline(y, 0, ' ', COLS);
    mvprintw(y, 1, "%3zu%%", static_cast<std::size_t>((safeDone * 100) / safeTotal));
    mvprintw(y, barX + 5 + barWidth, "%zu/%zu", safeDone, safeTotal);
    mvprintw(y, barX, "[");
    for (int i = 0; i < barWidth; ++i) {
        addch(i < filled ? '#' : '-');
    }
    addch(']');
}

void drawLiveCleanResults(
    const std::vector<Cleaner::CleanResult>& results,
    std::size_t totalEntriesRemoved,
    bool dryRun,
    std::size_t done,
    std::size_t total
) {
    clear();
    drawCentered(1, dryRun ? "Cleaning result (running dry-run)" : "Cleaning result (running)");
    mvprintw(3, 2, "%s%zu", dryRun ? "Dry-run entries removed: " : "Total entries removed: ", totalEntriesRemoved);

    const int panelLeft = 2;
    const int panelWidth = std::max(20, COLS - 4);
    const int nameW = 14;
    const int riskW = 6;
    const int statusW = 10;
    const int entriesW = 8;
    const int fixedColumns = nameW + riskW + statusW + entriesW + 4;
    const int pathW = std::max(8, panelWidth - fixedColumns);

    if (has_colors()) {
        attron(COLOR_PAIR(COLOR_HEADER));
    }
    mvprintw(5, panelLeft, "%-*s %-*s %-*s %*s %-*s", nameW, "NAME", riskW, "RISK", statusW, "STATUS", entriesW, "ENT", pathW, "PATH");
    mvhline(6, panelLeft, '-', std::max(1, panelWidth));
    if (has_colors()) {
        attroff(COLOR_PAIR(COLOR_HEADER));
    }

    const int maxRows = std::max(1, LINES - 10);
    const std::size_t start = results.size() > static_cast<std::size_t>(maxRows) ? results.size() - static_cast<std::size_t>(maxRows) : 0;
    int row = 7;
    for (std::size_t i = start; i < results.size() && row < LINES - 2; ++i) {
        const auto& r = results[i];
        const std::string entries = r.entries > 0 ? std::to_string(r.entries) : "-";
        mvprintw(
            row,
            panelLeft,
            "%-*s %-*s %-*s %*s %-*s",
            nameW,
            fitText(r.name, nameW).c_str(),
            riskW,
            fitText(r.risk, riskW).c_str(),
            statusW,
            fitText(r.status, statusW).c_str(),
            entriesW,
            entries.c_str(),
            pathW,
            fitText(r.path, pathW).c_str()
        );
        ++row;
    }

    mvprintw(LINES - 2, 2, "Processing... results update in real time");
    drawFooterProgress(done, total);
    refresh();
}

std::string formatBytes(std::uintmax_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }

    char buffer[64] = {0};
    std::snprintf(buffer, sizeof(buffer), "%.2f %s", value, units[unitIndex]);
    return std::string(buffer);
}

std::string timestampNow() {
    const std::time_t now = std::time(nullptr);
    char buffer[32] = {0};
    const std::tm* local = std::localtime(&now);
    if (!local) {
        return "unknown-time";
    }
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local);
    return std::string(buffer);
}

std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (const char ch : input) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

std::string fitText(const std::string& text, int width) {
    if (width <= 0) {
        return "";
    }
    if (static_cast<int>(text.size()) <= width) {
        return text;
    }
    if (width <= 3) {
        return text.substr(0, static_cast<std::size_t>(width));
    }
    return text.substr(0, static_cast<std::size_t>(width - 3)) + "...";
}

struct TableLayout {
    int panelLeft{2};
    int panelWidth{20};
    int selW{3};
    int nameW{12};
    int riskW{6};
    int entriesW{10};
    int sizeW{10};
    int pathW{8};
    bool compact{false};
};

TableLayout buildLayout() {
    TableLayout layout;
    layout.panelWidth = std::max(20, COLS - 4);
    layout.compact = COLS < 100;
    if (layout.compact) {
        layout.entriesW = 8;
        layout.sizeW = 0;
    }

    const int fixedColumns = layout.selW + layout.nameW + layout.riskW + layout.entriesW + layout.sizeW + 5;
    layout.pathW = std::max(8, layout.panelWidth - fixedColumns);
    return layout;
}

void drawTableHeader(const TableLayout& layout, int row) {
    if (has_colors()) {
        attron(COLOR_PAIR(COLOR_HEADER));
    }
    if (layout.compact) {
        mvprintw(
            row,
            layout.panelLeft,
            "%-*s %-*s %-*s %*s %-*s",
            layout.selW,
            "SEL",
            layout.nameW,
            "NAME",
            layout.riskW,
            "RISK",
            layout.entriesW,
            "ENT",
            layout.pathW,
            "PATH"
        );
    } else {
        mvprintw(
            row,
            layout.panelLeft,
            "%-*s %-*s %-*s %*s %*s %-*s",
            layout.selW,
            "SEL",
            layout.nameW,
            "NAME",
            layout.riskW,
            "RISK",
            layout.entriesW,
            "ENTRIES",
            layout.sizeW,
            "SIZE",
            layout.pathW,
            "PATH"
        );
    }
    mvhline(row + 1, layout.panelLeft, '-', std::max(1, layout.panelWidth));
    if (has_colors()) {
        attroff(COLOR_PAIR(COLOR_HEADER));
    }
}

void drawTableRow(
    int row,
    const TableLayout& layout,
    const std::string& mark,
    const std::string& name,
    const std::string& risk,
    const std::string& entries,
    const std::string& size,
    const std::string& path
) {
    if (layout.compact) {
        mvprintw(
            row,
            layout.panelLeft,
            "%-*s %-*s %-*s %*s %-*s",
            layout.selW,
            mark.c_str(),
            layout.nameW,
            fitText(name, layout.nameW).c_str(),
            layout.riskW,
            fitText(risk, layout.riskW).c_str(),
            layout.entriesW,
            fitText(entries, layout.entriesW).c_str(),
            layout.pathW,
            fitText(path, layout.pathW).c_str()
        );
    } else {
        mvprintw(
            row,
            layout.panelLeft,
            "%-*s %-*s %-*s %*s %*s %-*s",
            layout.selW,
            mark.c_str(),
            layout.nameW,
            fitText(name, layout.nameW).c_str(),
            layout.riskW,
            fitText(risk, layout.riskW).c_str(),
            layout.entriesW,
            fitText(entries, layout.entriesW).c_str(),
            layout.sizeW,
            fitText(size, layout.sizeW).c_str(),
            layout.pathW,
            fitText(path, layout.pathW).c_str()
        );
    }
}

void buildPreviewSummary(
    const std::vector<CleaningItem>& items,
    const std::vector<Cleaner::ItemPreview>& previews,
    std::size_t& totalEntries,
    std::uintmax_t& totalBytes,
    std::vector<std::string>* lines
) {
    totalEntries = 0;
    totalBytes = 0;
    if (lines) {
        lines->clear();
    }

    const std::size_t limit = std::min(items.size(), previews.size());
    for (std::size_t i = 0; i < limit; ++i) {
        const auto& item = items[i];
        const auto& preview = previews[i];
        if (!item.selected) {
            continue;
        }

        const std::string risk = !preview.accessible ? "deny" : item.risk;
        if (!preview.exists) {
            if (lines) {
                lines->push_back(item.name + " [" + risk + "]: missing");
            }
            continue;
        }
        if (!preview.accessible) {
            if (lines) {
                lines->push_back(item.name + " [" + risk + "]: access denied");
            }
            continue;
        }

        totalEntries += preview.entries;
        totalBytes += preview.bytes;

        if (lines) {
            lines->push_back(
                item.name +
                " [" + risk + "]: " +
                std::to_string(preview.entries) +
                " entries, " +
                std::to_string(preview.bytes) +
                " bytes"
            );
        }
    }
}
}

Tui::Tui(bool dryRun) : dryRun_(dryRun) {}

void Tui::requestPreviewRefresh() {
    previewRefreshRequested_ = true;
}

bool Tui::cacheMatchesCurrentItems() const {
    if (cachedPreviewItems_.size() != cleaner_.items().size() || cachedPreviews_.size() != cleaner_.items().size()) {
        return false;
    }

    for (std::size_t i = 0; i < cleaner_.items().size(); ++i) {
        if (cachedPreviewItems_[i].path != cleaner_.items()[i].path) {
            return false;
        }
    }
    return true;
}

void Tui::startPreviewJobIfNeeded() {
    if (previewJobRunning_ || !previewRefreshRequested_) {
        return;
    }

    const std::vector<CleaningItem> snapshot = cleaner_.items();
    previewRefreshRequested_ = false;
    previewJobRunning_ = true;

    previewFuture_ = std::async(std::launch::async, [this, snapshot]() {
        PreviewJobResult result;
        result.items = snapshot;
        result.previews = cleaner_.previewItemsFor(snapshot);
        return result;
    });
}

void Tui::collectPreviewJobIfReady() {
    if (!previewJobRunning_ || !previewFuture_.valid()) {
        return;
    }

    if (previewFuture_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    auto result = previewFuture_.get();
    cachedPreviewItems_ = std::move(result.items);
    cachedPreviews_ = std::move(result.previews);
    previewJobRunning_ = false;
}

int Tui::run() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    initColors();

    cleaner_.scan();

    bool running = true;
    int current = 0;
    int listOffset = 0;
    bool accessDialogShown = false;
    std::string riskFilter = "all";

    while (running) {
        collectPreviewJobIfReady();
        startPreviewJobIfNeeded();

        clear();
        drawCentered(1, "Mac Cleaner TUI");
        if (dryRun_) {
            drawCentered(2, "DRY-RUN mode (no deletion)   Arrows: move   Space: toggle   Enter: clean   q: quit");
        } else {
            drawCentered(2, "Arrows: move   Space: toggle   Enter: clean   q: quit");
        }

        std::vector<std::string> previewLines;
        std::uintmax_t previewBytes = 0;
        std::size_t previewEntries = 0;

        std::vector<Cleaner::ItemPreview> itemPreviews(cleaner_.items().size());
        const bool previewReady = cacheMatchesCurrentItems();
        if (previewReady) {
            itemPreviews = cachedPreviews_;
            buildPreviewSummary(cleaner_.items(), itemPreviews, previewEntries, previewBytes, &previewLines);
            mvprintw(3, 2, "Selected preview: %zu entries, %s", previewEntries, formatBytes(previewBytes).c_str());
        } else {
            mvprintw(3, 2, "Selected preview: scanning... (async)");
        }

        std::size_t safeSelected = 0;
        std::size_t skippedSelected = 0;
        std::size_t deniedSelected = 0;
        std::vector<std::string> deniedPaths;
        for (std::size_t i = 0; i < cleaner_.items().size(); ++i) {
            const auto& item = cleaner_.items()[i];
            if (!item.selected) {
                continue;
            }
            const auto& preview = itemPreviews[i];
            if (!preview.accessible) {
                ++deniedSelected;
                deniedPaths.push_back(item.path);
            } else if (item.risk == "safe") {
                ++safeSelected;
            } else {
                ++skippedSelected;
            }
        }
        mvprintw(4, 2, "Risk summary: safe=%zu  skipped=%zu  denied=%zu", safeSelected, skippedSelected, deniedSelected);

        const TableLayout layout = buildLayout();
        const int panelTop = 5;
        const int panelBottom = std::max(panelTop + 6, LINES - 3);
        const int panelLeft = 1;
        const int panelRight = std::max(panelLeft + 10, COLS - 2);

        mvaddch(panelTop, panelLeft, ACS_ULCORNER);
        mvhline(panelTop, panelLeft + 1, ACS_HLINE, std::max(1, panelRight - panelLeft - 1));
        mvaddch(panelTop, panelRight, ACS_URCORNER);
        for (int row = panelTop + 1; row < panelBottom; ++row) {
            mvaddch(row, panelLeft, ACS_VLINE);
            mvaddch(row, panelRight, ACS_VLINE);
        }
        mvaddch(panelBottom, panelLeft, ACS_LLCORNER);
        mvhline(panelBottom, panelLeft + 1, ACS_HLINE, std::max(1, panelRight - panelLeft - 1));
        mvaddch(panelBottom, panelRight, ACS_LRCORNER);
        mvprintw(panelTop, panelLeft + 3, " Cleaner Targets ");

        drawTableHeader(layout, panelTop + 1);

        std::vector<int> filteredIndexes;
        filteredIndexes.reserve(cleaner_.items().size());
        for (std::size_t i = 0; i < cleaner_.items().size(); ++i) {
            const auto& item = cleaner_.items()[i];
            if (riskFilter == "all" || item.risk == riskFilter) {
                filteredIndexes.push_back(static_cast<int>(i));
            }
        }

        const int visibleRows = std::max(1, panelBottom - (panelTop + 3));
        const int itemCount = static_cast<int>(filteredIndexes.size());

        if (itemCount <= 0) {
            current = 0;
            listOffset = 0;
        } else {
            current = std::clamp(current, 0, itemCount - 1);
            listOffset = std::clamp(listOffset, 0, std::max(0, itemCount - visibleRows));
            if (current < listOffset) {
                listOffset = current;
            }
            if (current >= listOffset + visibleRows) {
                listOffset = current - visibleRows + 1;
            }
        }

        int row = panelTop + 3;
        for (int i = listOffset; i < itemCount; ++i) {
            if (row >= panelBottom) {
                break;
            }
            const int sourceIndex = filteredIndexes[static_cast<std::size_t>(i)];
            const auto& item = cleaner_.items()[static_cast<std::size_t>(sourceIndex)];
            const auto& preview = itemPreviews[static_cast<std::size_t>(sourceIndex)];
            const std::string mark = item.selectable ? (item.selected ? "[x]" : "[ ]") : "[-]";
            const std::string risk = !preview.accessible ? "deny" : item.risk;
            const std::string entries = (!preview.exists || !preview.accessible) ? "-" : std::to_string(preview.entries);
            const std::string size = (!preview.exists || !preview.accessible) ? "-" : formatBytes(preview.bytes);

            if (i == current) {
                attron(A_REVERSE | A_BOLD);
            } else if (!item.selectable) {
                if (has_colors() && item.hidden) {
                    attron(COLOR_PAIR(COLOR_HIDDEN));
                }
                attron(A_DIM);
            } else if (has_colors()) {
                if (item.hidden) {
                    attron(COLOR_PAIR(COLOR_HIDDEN));
                } else if (risk == "safe") {
                    attron(COLOR_PAIR(COLOR_SAFE));
                } else {
                    attron(COLOR_PAIR(COLOR_SKIP));
                }
            } else if (!item.selected) {
                attron(A_DIM);
            }

            drawTableRow(row++, layout, mark, item.name, risk, entries, size, item.path);

            if (i == current) {
                attroff(A_REVERSE | A_BOLD);
            } else if (!item.selectable) {
                attroff(A_DIM);
                if (has_colors() && item.hidden) {
                    attroff(COLOR_PAIR(COLOR_HIDDEN));
                }
            } else if (has_colors()) {
                if (item.hidden) {
                    attroff(COLOR_PAIR(COLOR_HIDDEN));
                } else if (risk == "safe") {
                    attroff(COLOR_PAIR(COLOR_SAFE));
                } else {
                    attroff(COLOR_PAIR(COLOR_SKIP));
                }
            } else if (!item.selected) {
                attroff(A_DIM);
            }
        }

        if (itemCount > 0) {
            const int activeSourceIndex = filteredIndexes[static_cast<std::size_t>(current)];
            const auto& activeItem = cleaner_.items()[static_cast<std::size_t>(activeSourceIndex)];
            const auto& activePreview = itemPreviews[static_cast<std::size_t>(activeSourceIndex)];
            const std::string activeRisk = !activePreview.accessible ? "deny" : activeItem.risk;
            const std::string activeEntries = (!activePreview.exists || !activePreview.accessible) ? "-" : std::to_string(activePreview.entries);
            const std::string activeSize = (!activePreview.exists || !activePreview.accessible) ? "-" : formatBytes(activePreview.bytes);
            drawStatusBar(
                "Item=" + activeItem.name +
                "  Filter=" + riskFilter +
                "  Risk=" + activeRisk +
                "  Selectable=" + std::string(activeItem.selectable ? "yes" : "no") +
                "  Entries=" + activeEntries +
                "  Size=" + activeSize +
                "  Path=" + activeItem.path
            );
        } else {
            drawStatusBar("No cleanup categories available for filter=" + riskFilter + " (use /all, /safe, /review, /never)");
        }

        drawBottomBar();

        refresh();

        if (deniedSelected > 0 && !accessDialogShown) {
            showAccessDeniedDialog(deniedPaths);
            accessDialogShown = true;
        }
        if (deniedSelected == 0) {
            accessDialogShown = false;
        }

        const auto showHelp = []() {
            const std::vector<std::string> lines = {
                "NAME",
                "    mcleaner - interactive cache/log cleaner for macOS user paths",
                "",
                "SYNOPSIS",
                "    mcleaner",
                "    mcleaner --dry-run",
                "",
                "DESCRIPTION",
                "    Single-panel commander-style interface for selecting cleanup targets.",
                "    The cleaner only acts on safe paths under the current HOME directory.",
                "",
                "COMMANDS",
                "    1, Space       Toggle current row",
                "    2              Select all rows",
                "    3              Deselect all rows",
                "    4              Open preview table",
                "    5, Enter       Clean selected rows (with confirmation)",
                "    6              Rescan default targets",
                "    7              Scan hidden home directories",
                "    8              Confirm-mark current review row",
                "    /risk          Filter rows by risk (all/safe/review/never)",
                "    9              Open this help screen",
                "    0, q, F10      Quit application",
                "",
                "NAVIGATION",
                "    Up, Down       Move current row",
                "    PgUp, PgDn     Scroll one page",
                "    Home, End      Jump to first/last row",
                "    Ctrl+U, Ctrl+D Scroll half page up/down",
                "",
                "FILES",
                "    ~/Library/Logs/mcleaner.log",
                "    ~/Library/Logs/mcleaner-last-report.json",
                "",
                "NOTES",
                "    Unsafe paths are skipped and reported in logs/results.",
                "    Preview and cleanup outputs are paginated for narrow terminals."
            };
            showPagedLines("MAC CLEANER TUI", "General Commands Manual", lines);
        };

        const auto runPreview = [this, &previewLines]() {
            if (!cacheMatchesCurrentItems()) {
                const std::vector<std::string> lines = {
                    "Preview is still loading in background.",
                    "Please wait a moment and try again.",
                };
                showPagedLines("Preview loading", "Asynchronous scan in progress", lines);
                return;
            }

            std::size_t entries = 0;
            std::uintmax_t bytes = 0;
            buildPreviewSummary(cleaner_.items(), cachedPreviews_, entries, bytes, &previewLines);
            showPreviewTable(cleaner_.items(), cachedPreviews_, entries, bytes);
            appendPreviewLog(entries, bytes, previewLines);
        };

        const auto runClean = [this]() {
            if (!confirmClean()) {
                return;
            }
            std::vector<CleaningItem> selectedItems;
            for (const auto& item : cleaner_.items()) {
                if (item.selected) {
                    selectedItems.push_back(item);
                }
            }

            if (selectedItems.empty()) {
                const std::vector<std::string> lines = {"No selected rows to process."};
                showPagedLines("Cleaning result", "Nothing to do", lines);
                return;
            }

            using TaskResult = std::pair<Cleaner::CleanResult, std::size_t>;
            std::vector<std::future<TaskResult>> futures;
            futures.reserve(selectedItems.size());
            for (const auto& item : selectedItems) {
                futures.push_back(std::async(std::launch::async, [this, item]() {
                    const Cleaner::CleanResult result = cleaner_.cleanItem(item, dryRun_);
                    const std::size_t removed = (result.status == "removed") ? result.entries : 0;
                    return TaskResult{result, removed};
                }));
            }

            std::vector<bool> finished(futures.size(), false);
            std::vector<Cleaner::CleanResult> cleanResults;
            cleanResults.reserve(futures.size());
            std::size_t completed = 0;
            std::size_t removed = 0;

            while (completed < futures.size()) {
                for (std::size_t i = 0; i < futures.size(); ++i) {
                    if (finished[i]) {
                        continue;
                    }
                    if (futures[i].wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                        TaskResult r = futures[i].get();
                        finished[i] = true;
                        ++completed;
                        removed += r.second;
                        cleanResults.push_back(std::move(r.first));
                    }
                }

                drawLiveCleanResults(cleanResults, removed, dryRun_, completed, futures.size());
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
            }

            std::vector<std::string> messages;
            messages.reserve(cleanResults.size());
            for (const auto& row : cleanResults) {
                messages.push_back(row.name + ": " + row.detail);
            }

            requestPreviewRefresh();
            showCleanResultTable(cleanResults, removed, dryRun_);
            appendCleanLog(removed, messages);
            writeJsonReport(removed, messages);
        };

        const auto runHiddenScan = [this]() {
            std::vector<std::string> messages;
            const std::size_t added = cleaner_.scanHiddenCandidates(messages);
            requestPreviewRefresh();
            showPagedLines(
                "HiddenScan result",
                "Added candidates: " + std::to_string(added),
                messages
            );
        };

        const auto runRiskFilterPrompt = [&riskFilter, &current, &listOffset]() {
            echo();
            curs_set(1);

            char input[32] = {0};
            mvhline(LINES - 2, 0, ' ', COLS);
            mvprintw(LINES - 2, 1, "Filter risk (/all, /safe, /review, /never): /");
            move(LINES - 2, 47);
            getnstr(input, 16);

            noecho();
            curs_set(0);

            std::string value(input);
            if (value == "all" || value == "safe" || value == "review" || value == "never") {
                riskFilter = value;
                current = 0;
                listOffset = 0;
            }
        };

        const int ch = getch();
        switch (ch) {
            case 'q':
            case '0':
            case KEY_F(10):
                running = false;
                break;
            case 'r':
            case '6':
            case KEY_F(6):
                cleaner_.scan();
                requestPreviewRefresh();
                break;
            case '7':
            case KEY_F(7):
                runHiddenScan();
                break;
            case '8':
            case KEY_F(8):
                if (itemCount > 0) {
                    const int sourceIndex = filteredIndexes[static_cast<std::size_t>(current)];
                    auto& item = cleaner_.items()[static_cast<std::size_t>(sourceIndex)];
                    if (item.hidden && item.risk == "review" && confirmReviewSelection(item)) {
                        item.selectable = true;
                        item.selected = true;
                        requestPreviewRefresh();
                    }
                }
                break;
            case '/':
                runRiskFilterPrompt();
                break;
            case 'a':
            case '2':
            case KEY_F(2):
                selectAll(true);
                break;
            case 'n':
            case '3':
            case KEY_F(3):
                selectAll(false);
                break;
            case 'p':
            case '4':
            case KEY_F(4):
                runPreview();
                break;
            case 'c':
            case '5':
            case KEY_F(5):
            case 10:
            case 13:
                runClean();
                break;
            case 'h':
            case '9':
            case KEY_F(9):
                showHelp();
                break;
            case KEY_UP:
                if (itemCount > 0) {
                    current = std::max(0, current - 1);
                }
                break;
            case KEY_DOWN:
                if (itemCount > 0) {
                    current = std::min(current + 1, itemCount - 1);
                }
                break;
            case KEY_HOME:
                if (itemCount > 0) {
                    current = 0;
                }
                break;
            case KEY_END:
                if (itemCount > 0) {
                    current = itemCount - 1;
                }
                break;
            case KEY_PPAGE:
                if (itemCount > 0) {
                    current = std::max(0, current - visibleRows);
                }
                break;
            case KEY_NPAGE:
                if (itemCount > 0) {
                    current = std::min(itemCount - 1, current + visibleRows);
                }
                break;
            case 21:  // Ctrl+U
                if (itemCount > 0) {
                    const int half = std::max(1, visibleRows / 2);
                    current = std::max(0, current - half);
                }
                break;
            case 4:  // Ctrl+D
                if (itemCount > 0) {
                    const int half = std::max(1, visibleRows / 2);
                    current = std::min(itemCount - 1, current + half);
                }
                break;
            case ' ':
            case '1':
            case KEY_F(1):
                if (itemCount > 0) {
                    toggleItem(filteredIndexes[static_cast<std::size_t>(current)]);
                }
                break;
            default: break;
        }
    }

    if (previewFuture_.valid()) {
        previewFuture_.wait();
    }

    endwin();
    return 0;
}

bool Tui::confirmClean() {
    clear();
    drawCentered(1, "Confirm clean");
    mvprintw(3, 2, "This action removes files from selected locations.");
    mvprintw(4, 2, "Press Enter to continue or ESC to cancel.");
    refresh();
    const int ch = getch();
    return ch == 10 || ch == 13 || ch == KEY_ENTER;
}

bool Tui::confirmReviewSelection(const CleaningItem& item) {
    const int width = std::min(std::max(42, COLS - 10), 76);
    const int height = 11;
    const int startY = std::max(1, (LINES - height) / 2);
    const int startX = std::max(2, (COLS - width) / 2);

    WINDOW* win = newwin(height, width, startY, startX);
    if (!win) {
        return false;
    }

    keypad(win, TRUE);
    if (has_colors()) {
        wbkgd(win, COLOR_PAIR(COLOR_MODAL_WARNING));
    }
    werase(win);
    box(win, 0, 0);
    wattron(win, A_BOLD);
    mvwprintw(win, 1, 3, "Risk Confirmation");
    wattroff(win, A_BOLD);

    mvwprintw(win, 3, 2, "This row has risk=review and is not selected by default.");
    mvwprintw(win, 4, 2, "If you confirm, it will be marked for deletion.");
    mvwprintw(win, 6, 2, "Name: %s", fitText(item.name, width - 10).c_str());
    mvwprintw(win, 7, 2, "Path: %s", fitText(item.path, width - 10).c_str());
    mvwprintw(win, 9, 2, "Press Enter to confirm or ESC to cancel.");

    wrefresh(win);
    const int ch = wgetch(win);
    delwin(win);
    return ch == 10 || ch == 13 || ch == KEY_ENTER;
}

void Tui::showAccessDeniedDialog(const std::vector<std::string>& deniedPaths) {
    const int width = std::min(std::max(36, COLS - 8), 66);
    const int height = std::min(LINES - 4, 12 + static_cast<int>(std::min<std::size_t>(3, deniedPaths.size())));
    const int startY = std::max(1, (LINES - height) / 2);
    const int startX = std::max(2, (COLS - width) / 2);

    WINDOW* win = newwin(height, width, startY, startX);
    if (!win) {
        return;
    }

    keypad(win, TRUE);
    if (has_colors()) {
        wbkgd(win, COLOR_PAIR(COLOR_MODAL_ERROR));
    }
    werase(win);
    box(win, 0, 0);

    wattron(win, A_BOLD);
    mvwprintw(win, 1, 3, "Access Error");
    wattroff(win, A_BOLD);

    mvwprintw(win, 3, 2, "Cannot access one or more directories.");
    mvwprintw(win, 4, 2, "Grant Full Disk Access to your terminal emulator.");
    mvwprintw(win, 5, 2, "Settings -> Privacy & Security -> Full Disk Access");

    int row = 7;
    for (std::size_t i = 0; i < deniedPaths.size() && i < 3; ++i) {
        const std::string path = fitText(deniedPaths[i], width - 6);
        mvwprintw(win, row++, 2, "- %s", path.c_str());
    }

    mvwprintw(win, height - 2, 2, "Press ESC to continue...");
    wrefresh(win);

    while (true) {
        const int ch = wgetch(win);
        if (ch == 27) {
            break;
        }
    }

    delwin(win);
}

void Tui::drawProgress(std::size_t done, std::size_t total, const std::string& label) {
    clear();
    drawCentered(1, "Cleaning in progress");

    const int barWidth = std::max(10, COLS - 20);
    const std::size_t clampedTotal = std::max<std::size_t>(1, total);
    const std::size_t clampedDone = std::min(done, clampedTotal);
    const int filled = static_cast<int>((static_cast<double>(clampedDone) / static_cast<double>(clampedTotal)) * static_cast<double>(barWidth));

    mvprintw(3, 2, "Current: %s", label.c_str());
    mvprintw(4, 2, "Progress: %zu/%zu", clampedDone, clampedTotal);
    mvprintw(6, 2, "[");
    for (int i = 0; i < barWidth; ++i) {
        addch(i < filled ? '#' : '-');
    }
    addch(']');
    refresh();
}

void Tui::showPagedLines(
    const std::string& title,
    const std::string& summary,
    const std::vector<std::string>& lines,
    const std::string& footerLeft,
    const std::string& footerRight
) {
    const std::size_t pageSize = static_cast<std::size_t>(std::max(1, LINES - 8));
    const std::size_t pageCount = std::max<std::size_t>(1, (lines.size() + pageSize - 1) / pageSize);
    std::size_t page = 0;

    while (true) {
        clear();
        drawCentered(1, title);
        mvprintw(3, 2, "%s", summary.c_str());

        if (lines.empty()) {
            mvprintw(5, 2, "(no items)");
        } else {
            const std::size_t begin = page * pageSize;
            const std::size_t end = std::min(begin + pageSize, lines.size());
            int row = 5;
            for (std::size_t i = begin; i < end && row < LINES - 2; ++i) {
                mvprintw(row++, 2, "%s", lines[i].c_str());
            }
        }

        mvprintw(
            LINES - 2,
            2,
            "Page %zu/%zu  n:next  p:prev  q/Enter:back",
            page + 1,
            pageCount
        );

        if (!footerLeft.empty() || !footerRight.empty()) {
            mvhline(LINES - 1, 0, ' ', COLS);
            mvprintw(LINES - 1, 1, "%s", fitText(footerLeft, std::max(1, COLS - 2)).c_str());
            const int rightLen = static_cast<int>(footerRight.size());
            const int rightX = std::max(1, COLS - rightLen - 1);
            mvprintw(LINES - 1, rightX, "%s", footerRight.c_str());
        }
        refresh();

        const int ch = getch();
        if (ch == 'q' || ch == 10 || ch == 13 || ch == 27) {
            break;
        }
        if ((ch == 'n' || ch == KEY_RIGHT || ch == KEY_DOWN) && page + 1 < pageCount) {
            ++page;
        }
        if ((ch == 'p' || ch == KEY_LEFT || ch == KEY_UP) && page > 0) {
            --page;
        }
    }
}

void Tui::showPreviewTable(
    const std::vector<CleaningItem>& items,
    const std::vector<Cleaner::ItemPreview>& previews,
    std::size_t totalEntries,
    std::uintmax_t totalBytes
) {
    std::vector<std::size_t> visibleIndexes;
    for (std::size_t i = 0; i < items.size() && i < previews.size(); ++i) {
        visibleIndexes.push_back(i);
    }

    const std::size_t pageSize = static_cast<std::size_t>(std::max(1, LINES - 11));
    const std::size_t pageCount = std::max<std::size_t>(1, (visibleIndexes.size() + pageSize - 1) / pageSize);
    std::size_t page = 0;

    while (true) {
        clear();
        drawCentered(1, "Preview overview");
        mvprintw(3, 2, "Total: %zu entries, %s", totalEntries, formatBytes(totalBytes).c_str());

        const TableLayout layout = buildLayout();
        drawTableHeader(layout, 5);

        const std::size_t begin = page * pageSize;
        const std::size_t end = std::min(begin + pageSize, visibleIndexes.size());

        int row = 7;
        for (std::size_t pos = begin; pos < end && row < LINES - 2; ++pos) {
            const std::size_t idx = visibleIndexes[pos];
            const auto& item = items[idx];
            const auto& preview = previews[idx];

            const std::string mark = item.selectable ? (item.selected ? "[x]" : "[ ]") : "[-]";
            const std::string risk = !preview.accessible ? "deny" : item.risk;
            const std::string entries = (!preview.exists || !preview.accessible) ? "-" : std::to_string(preview.entries);
            const std::string size = (!preview.exists || !preview.accessible) ? "-" : formatBytes(preview.bytes);
            drawTableRow(row++, layout, mark, item.name, risk, entries, size, item.path);
        }

        if (visibleIndexes.empty()) {
            mvprintw(8, 2, "No rows.");
        }

        mvprintw(
            LINES - 2,
            2,
            "Page %zu/%zu  n:next  p:prev  q/Enter:back",
            page + 1,
            pageCount
        );
        refresh();

        const int ch = getch();
        if (ch == 'q' || ch == 10 || ch == 13 || ch == 27) {
            break;
        }
        if ((ch == 'n' || ch == KEY_RIGHT || ch == KEY_DOWN) && page + 1 < pageCount) {
            ++page;
        }
        if ((ch == 'p' || ch == KEY_LEFT || ch == KEY_UP) && page > 0) {
            --page;
        }
    }
}

void Tui::showCleanResultTable(
    const std::vector<Cleaner::CleanResult>& results,
    std::size_t totalEntriesRemoved,
    bool dryRun
) {
    const std::size_t pageSize = static_cast<std::size_t>(std::max(1, LINES - 11));
    const std::size_t pageCount = std::max<std::size_t>(1, (results.size() + pageSize - 1) / pageSize);
    std::size_t page = 0;

    while (true) {
        clear();
        drawCentered(1, dryRun ? "Cleaning result (dry-run)" : "Cleaning result");
        mvprintw(3, 2, "%s%zu", dryRun ? "Dry-run entries removed: " : "Total entries removed: ", totalEntriesRemoved);

        const int panelLeft = 2;
        const int panelWidth = std::max(20, COLS - 4);
        const int nameW = 14;
        const int riskW = 6;
        const int statusW = 10;
        const int entriesW = 8;
        const int fixedColumns = nameW + riskW + statusW + entriesW + 4;
        const int pathW = std::max(8, panelWidth - fixedColumns);

        if (has_colors()) {
            attron(COLOR_PAIR(COLOR_HEADER));
        }
        mvprintw(5, panelLeft, "%-*s %-*s %-*s %*s %-*s", nameW, "NAME", riskW, "RISK", statusW, "STATUS", entriesW, "ENT", pathW, "PATH");
        mvhline(6, panelLeft, '-', std::max(1, panelWidth));
        if (has_colors()) {
            attroff(COLOR_PAIR(COLOR_HEADER));
        }

        const std::size_t begin = page * pageSize;
        const std::size_t end = std::min(begin + pageSize, results.size());
        int row = 7;
        for (std::size_t i = begin; i < end && row < LINES - 2; ++i) {
            const auto& r = results[i];
            const std::string entries = r.entries > 0 ? std::to_string(r.entries) : "-";

            if (has_colors()) {
                if (r.risk == "safe") {
                    attron(COLOR_PAIR(COLOR_SAFE));
                } else if (r.risk == "review" || r.risk == "never") {
                    attron(COLOR_PAIR(COLOR_HIDDEN));
                }
            }

            mvprintw(
                row++,
                panelLeft,
                "%-*s %-*s %-*s %*s %-*s",
                nameW,
                fitText(r.name, nameW).c_str(),
                riskW,
                fitText(r.risk, riskW).c_str(),
                statusW,
                fitText(r.status, statusW).c_str(),
                entriesW,
                entries.c_str(),
                pathW,
                fitText(r.path, pathW).c_str()
            );

            if (has_colors()) {
                int statusColor = 0;
                if (r.status == "removed") {
                    statusColor = COLOR_STATUS_OK;
                } else if (r.status == "dry-run" || r.status == "skipped" || r.status == "missing") {
                    statusColor = COLOR_STATUS_WARN;
                } else if (r.status == "error") {
                    statusColor = COLOR_STATUS_ERR;
                }

                if (statusColor != 0) {
                    const int statusX = panelLeft + nameW + 1 + riskW + 1;
                    attron(COLOR_PAIR(statusColor) | A_BOLD);
                    mvprintw(row - 1, statusX, "%-*s", statusW, fitText(r.status, statusW).c_str());
                    attroff(COLOR_PAIR(statusColor) | A_BOLD);
                }
            }

            if (has_colors()) {
                if (r.risk == "safe") {
                    attroff(COLOR_PAIR(COLOR_SAFE));
                } else if (r.risk == "review" || r.risk == "never") {
                    attroff(COLOR_PAIR(COLOR_HIDDEN));
                }
            }
        }

        if (results.empty()) {
            mvprintw(8, 2, "No selected rows were processed.");
        }

        mvprintw(LINES - 2, 2, "Page %zu/%zu  n:next  p:prev  q/Enter:back", page + 1, pageCount);
        refresh();

        const int ch = getch();
        if (ch == 'q' || ch == 10 || ch == 13 || ch == 27) {
            break;
        }
        if ((ch == 'n' || ch == KEY_RIGHT || ch == KEY_DOWN) && page + 1 < pageCount) {
            ++page;
        }
        if ((ch == 'p' || ch == KEY_LEFT || ch == KEY_UP) && page > 0) {
            --page;
        }
    }
}

void Tui::selectAll(bool selected) {
    for (auto& item : cleaner_.items()) {
        if (!item.selectable) {
            item.selected = false;
            continue;
        }
        item.selected = selected;
    }
}

void Tui::appendLog(const std::vector<std::string>& lines) const {
    const char* home = std::getenv("HOME");
    if (!home) {
        return;
    }

    const std::filesystem::path logPath = std::filesystem::path(home) / "Library" / "Logs" / "mcleaner.log";
    std::error_code ec;
    std::filesystem::create_directories(logPath.parent_path(), ec);

    std::ofstream out(logPath.string(), std::ios::app);
    if (!out.is_open()) {
        return;
    }

    for (const auto& line : lines) {
        out << line << '\n';
    }
    out << '\n';
}

void Tui::appendCleanLog(std::size_t removed, const std::vector<std::string>& lines) const {
    std::vector<std::string> output;
    output.reserve(lines.size() + 3);
    output.push_back("[" + timestampNow() + "] clean");
    output.push_back("total entries removed: " + std::to_string(removed));
    for (const auto& line : lines) {
        output.push_back("- " + line);
    }
    appendLog(output);
}

void Tui::appendPreviewLog(std::size_t entries, std::uintmax_t bytes, const std::vector<std::string>& lines) const {
    std::vector<std::string> output;
    output.reserve(lines.size() + 3);
    output.push_back("[" + timestampNow() + "] preview");
    output.push_back("total preview: " + std::to_string(entries) + " entries, " + formatBytes(bytes));
    for (const auto& line : lines) {
        output.push_back("- " + line);
    }
    appendLog(output);
}

void Tui::writeJsonReport(std::size_t removed, const std::vector<std::string>& lines) const {
    const char* home = std::getenv("HOME");
    if (!home) {
        return;
    }

    const std::filesystem::path reportPath = std::filesystem::path(home) / "Library" / "Logs" / "mcleaner-last-report.json";
    std::error_code ec;
    std::filesystem::create_directories(reportPath.parent_path(), ec);

    std::ofstream out(reportPath.string(), std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    out << "{\n";
    out << "  \"timestamp\": \"" << jsonEscape(timestampNow()) << "\",\n";
    out << "  \"total_entries_removed\": " << removed << ",\n";
    out << "  \"messages\": [\n";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out << "    \"" << jsonEscape(lines[i]) << "\"";
        if (i + 1 < lines.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void Tui::toggleItem(int index) {
    if (index < 0 || index >= static_cast<int>(cleaner_.items().size())) return;
    if (!cleaner_.items()[index].selectable) return;
    cleaner_.items()[index].selected = !cleaner_.items()[index].selected;
}
