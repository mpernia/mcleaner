#pragma once

#include "Cleaner.h"

#include <cstdint>
#include <future>
#include <string>
#include <vector>

class Tui {
public:
    explicit Tui(bool dryRun = false);
    int run();

private:
    Cleaner cleaner_;
    bool dryRun_{false};

    static bool confirmClean();
    static bool confirmReviewSelection(const CleaningItem& item);
    static void showAccessDeniedDialog(const std::vector<std::string>& deniedPaths);
    static void drawProgress(std::size_t done, std::size_t total, const std::string& label);
    static void showPagedLines(
        const std::string& title,
        const std::string& summary,
        const std::vector<std::string>& lines,
        const std::string& footerLeft = "",
        const std::string& footerRight = ""
    );
    static void showPreviewTable(
        const std::vector<CleaningItem>& items,
        const std::vector<Cleaner::ItemPreview>& previews,
        std::size_t totalEntries,
        std::uintmax_t totalBytes
    );
    static void showCleanResultTable(
        const std::vector<Cleaner::CleanResult>& results,
        std::size_t totalEntriesRemoved,
        bool dryRun
    );
    void requestPreviewRefresh();
    void startPreviewJobIfNeeded();
    void collectPreviewJobIfReady();
    bool cacheMatchesCurrentItems() const;
    void selectAll(bool selected);
    void appendLog(const std::vector<std::string>& lines) const;
    void appendCleanLog(std::size_t removed, const std::vector<std::string>& lines) const;
    void appendPreviewLog(std::size_t entries, std::uintmax_t bytes, const std::vector<std::string>& lines) const;
    void writeJsonReport(std::size_t removed, const std::vector<std::string>& lines) const;
    void toggleItem(int index);

    struct PreviewJobResult {
        std::vector<CleaningItem> items;
        std::vector<Cleaner::ItemPreview> previews;
    };

    std::future<PreviewJobResult> previewFuture_;
    bool previewJobRunning_{false};
    bool previewRefreshRequested_{true};
    std::vector<CleaningItem> cachedPreviewItems_;
    std::vector<Cleaner::ItemPreview> cachedPreviews_;
};
