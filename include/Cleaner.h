#pragma once

#include "CleaningItem.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class Cleaner {
public:
    struct CleanResult {
        std::string name;
        std::string path;
        std::string risk;
        std::string status;
        std::size_t entries{0};
        std::string detail;
    };

    struct ItemPreview {
        std::size_t entries{0};
        std::uintmax_t bytes{0};
        bool exists{false};
        bool accessible{true};
        bool safe{false};
    };

    Cleaner();

    const std::vector<CleaningItem>& items() const;
    std::vector<CleaningItem>& items();

    bool isSafePath(const std::string& path) const;
    std::size_t scan();
    std::size_t scanHiddenCandidates(std::vector<std::string>& messages);
    std::size_t scanHomeDirectoryCandidates(std::vector<std::string>& messages);
    std::vector<ItemPreview> previewItemsFor(const std::vector<CleaningItem>& items) const;
    std::vector<ItemPreview> previewItems() const;
    std::size_t previewSelected(std::vector<std::string>& messages, std::uintmax_t& totalBytes) const;
    CleanResult cleanItem(const CleaningItem& item, bool dryRun = false) const;
    std::size_t cleanSelected(
        std::vector<std::string>& messages,
        bool dryRun = false,
        const std::function<void(std::size_t, std::size_t, const CleaningItem&)>& onProgress = {},
        std::vector<CleanResult>* results = nullptr
    );

private:
    std::vector<CleaningItem> items_;

    static std::vector<CleaningItem> defaultItems();
    static std::size_t removePathContents(const std::string& path, std::string& message);
};
