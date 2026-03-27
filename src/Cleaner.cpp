#include "Cleaner.h"

#include <filesystem>
#include <cstdlib>
#include <system_error>

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {
bool startsWithPath(const fs::path& fullPath, const fs::path& prefixPath) {
    auto fullIt = fullPath.begin();
    auto prefixIt = prefixPath.begin();
    for (; prefixIt != prefixPath.end(); ++prefixIt, ++fullIt) {
        if (fullIt == fullPath.end() || *fullIt != *prefixIt) {
            return false;
        }
    }
    return true;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string classifyHiddenPath(const fs::path& relativePath) {
    const std::string rel = toLower(relativePath.generic_string());

    static const std::unordered_set<std::string> neverRoots = {
        ".ssh",
        ".gnupg",
        ".aws",
        ".azure",
        ".kube",
        ".config",
        ".docker",
        ".local",
    };

    static const std::unordered_set<std::string> safeRoots = {
        ".cache",
        ".npm",
        ".yarn",
        ".pnpm-store",
        ".gradle",
        ".m2",
        ".cargo",
        ".ivy2",
    };

    const std::string root = toLower(relativePath.begin() == relativePath.end() ? "" : (*relativePath.begin()).string());
    if (neverRoots.contains(root)) {
        return "never";
    }
    if (safeRoots.contains(root)) {
        return "safe";
    }

    if (rel.find("cache") != std::string::npos || rel.find("tmp") != std::string::npos) {
        return "safe";
    }
    return "review";
}

std::string hiddenDisplayName(const fs::path& relativePath) {
    const std::string base = relativePath.filename().string();
    if (base.empty()) {
        return ".hidden";
    }
    return base;
}

std::string classifyHomePath(const fs::path& namePath) {
    const std::string name = toLower(namePath.filename().string());

    static const std::unordered_set<std::string> safeNames = {
        "cache",
        "caches",
        "tmp",
        "temp",
        "logs",
    };

    static const std::unordered_set<std::string> defaultSystemNames = {
        "library",
        "applications",
        "desktop",
        "documents",
        "downloads",
        "movies",
        "music",
        "pictures",
        "public",
        "sites",
    };

    if (safeNames.contains(name)) {
        return "safe";
    }
    if (defaultSystemNames.contains(name)) {
        return "never";
    }
    return "safe";
}

Cleaner::ItemPreview previewPath(const fs::path& path, bool safe) {
    Cleaner::ItemPreview result;
    result.safe = safe;

    std::error_code ec;
    const bool pathExists = fs::exists(path, ec);
    if (ec) {
        result.exists = true;
        result.accessible = false;
        return result;
    }

    if (!pathExists) {
        return result;
    }

    result.exists = true;

    if (fs::is_regular_file(path, ec)) {
        result.entries = 1;
        result.bytes = fs::file_size(path, ec);
        if (ec) {
            result.bytes = 0;
        }
        return result;
    }

    if (!fs::is_directory(path, ec)) {
        if (ec) {
            result.accessible = false;
        }
        return result;
    }

    fs::directory_iterator rootIt(path, ec);
    if (ec) {
        result.accessible = false;
        return result;
    }

    fs::recursive_directory_iterator it(
        path,
        fs::directory_options::skip_permission_denied,
        ec
    );
    fs::recursive_directory_iterator end;
    while (!ec && it != end) {
        const auto& entry = *it;
        ++result.entries;
        if (entry.is_regular_file(ec)) {
            const auto size = entry.file_size(ec);
            if (!ec) {
                result.bytes += size;
            }
        }
        ec.clear();
        it.increment(ec);
    }

    if (ec) {
        result.accessible = false;
    }

    return result;
}
}

Cleaner::Cleaner() : items_(defaultItems()) {}

const std::vector<CleaningItem>& Cleaner::items() const { return items_; }

std::vector<CleaningItem>& Cleaner::items() { return items_; }

bool Cleaner::isSafePath(const std::string& path) const {
    const char* home = std::getenv("HOME");
    if (!home || path.empty()) {
        return false;
    }

    std::error_code ec;
    const fs::path homePath = fs::path(home).lexically_normal();
    fs::path candidatePath = fs::path(path).lexically_normal();

    const fs::path canonicalHome = fs::weakly_canonical(homePath, ec);
    if (!ec) {
        ec.clear();
        const fs::path canonicalCandidate = fs::weakly_canonical(candidatePath, ec);
        if (!ec) {
            return startsWithPath(canonicalCandidate, canonicalHome);
        }
    }

    return startsWithPath(candidatePath, homePath);
}

std::vector<CleaningItem> Cleaner::defaultItems() {
    const char* home = std::getenv("HOME");
    std::string homeDir = home ? home : "";

    return {
        {"Trash", homeDir + "/.Trash", "safe", false, true, true},
        {"Caches", homeDir + "/Library/Caches", "safe", false, true, true},
        {"Logs", homeDir + "/Library/Logs", "safe", false, true, true},
        {"Temp", homeDir + "/.tmp", "safe", false, true, true},
    };
}

std::size_t Cleaner::scan() {
    items_ = defaultItems();
    std::vector<std::string> ignoredMessages;
    scanHomeDirectoryCandidates(ignoredMessages);
    return items_.size();
}

std::size_t Cleaner::scanHiddenCandidates(std::vector<std::string>& messages) {
    messages.clear();

    const char* home = std::getenv("HOME");
    if (!home) {
        messages.push_back("HiddenScan: HOME not available.");
        return 0;
    }

    const fs::path homePath = fs::path(home);
    std::error_code ec;
    fs::directory_iterator it(homePath, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        messages.push_back("HiddenScan: cannot read home directory.");
        return 0;
    }

    std::unordered_set<std::string> existing;
    for (const auto& item : items_) {
        existing.insert(fs::path(item.path).lexically_normal().string());
    }

    std::size_t added = 0;
    std::size_t safeAdded = 0;
    std::size_t reviewAdded = 0;
    std::size_t neverAdded = 0;

    for (const auto& entry : it) {
        if (ec) {
            ec.clear();
            continue;
        }

        const fs::path candidate = entry.path();
        const std::string name = candidate.filename().string();
        if (name.empty() || name[0] != '.') {
            continue;
        }
        if (!entry.is_directory(ec)) {
            ec.clear();
            continue;
        }

        const std::string normalized = candidate.lexically_normal().string();
        if (existing.contains(normalized)) {
            continue;
        }

        const fs::path relative = fs::relative(candidate, homePath, ec);
        ec.clear();
        const std::string risk = classifyHiddenPath(relative.empty() ? candidate.filename() : relative);
        const bool selectable = (risk == "safe");

        items_.push_back(CleaningItem{
            hiddenDisplayName(relative.empty() ? candidate.filename() : relative),
            normalized,
            risk,
            true,
            selectable,
            selectable,
        });

        existing.insert(normalized);
        ++added;
        if (risk == "safe") {
            ++safeAdded;
        } else if (risk == "review") {
            ++reviewAdded;
        } else {
            ++neverAdded;
        }
    }

    messages.push_back(
        "HiddenScan: added " + std::to_string(added) +
        " candidates (safe=" + std::to_string(safeAdded) +
        ", review=" + std::to_string(reviewAdded) +
        ", never=" + std::to_string(neverAdded) + ")."
    );
    messages.push_back("HiddenScan: only risk=safe candidates are selectable by default.");
    return added;
}

std::size_t Cleaner::scanHomeDirectoryCandidates(std::vector<std::string>& messages) {
    messages.clear();

    const char* home = std::getenv("HOME");
    if (!home) {
        messages.push_back("HomeScan: HOME not available.");
        return 0;
    }

    const fs::path homePath = fs::path(home);
    std::error_code ec;
    fs::directory_iterator it(homePath, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        messages.push_back("HomeScan: cannot read home directory.");
        return 0;
    }

    std::unordered_set<std::string> existing;
    for (const auto& item : items_) {
        existing.insert(fs::path(item.path).lexically_normal().string());
    }

    std::size_t added = 0;
    std::size_t safeAdded = 0;
    std::size_t reviewAdded = 0;
    std::size_t neverAdded = 0;

    for (const auto& entry : it) {
        if (ec) {
            ec.clear();
            continue;
        }

        if (!entry.is_directory(ec)) {
            ec.clear();
            continue;
        }

        const fs::path candidate = entry.path();
        const std::string name = candidate.filename().string();
        if (name.empty() || name[0] == '.') {
            continue;
        }

        const std::string normalized = candidate.lexically_normal().string();
        if (existing.contains(normalized)) {
            continue;
        }

        const std::string risk = classifyHomePath(candidate.filename());
        const bool selectable = (risk == "safe");

        items_.push_back(CleaningItem{
            name,
            normalized,
            risk,
            false,
            false,
            selectable,
        });

        existing.insert(normalized);
        ++added;
        if (risk == "safe") {
            ++safeAdded;
        } else if (risk == "review") {
            ++reviewAdded;
        } else {
            ++neverAdded;
        }
    }

    messages.push_back(
        "HomeScan: added " + std::to_string(added) +
        " directories (safe=" + std::to_string(safeAdded) +
        ", review=" + std::to_string(reviewAdded) +
        ", never=" + std::to_string(neverAdded) + ")."
    );
    messages.push_back("HomeScan: non-system directories are safe and selectable; system defaults stay protected.");
    return added;
}

std::vector<Cleaner::ItemPreview> Cleaner::previewItems() const {
    return previewItemsFor(items_);
}

std::vector<Cleaner::ItemPreview> Cleaner::previewItemsFor(const std::vector<CleaningItem>& items) const {
    std::vector<ItemPreview> previews;
    previews.reserve(items.size());

    for (const auto& item : items) {
        const bool safe = isSafePath(item.path);
        previews.push_back(previewPath(item.path, safe));
    }

    return previews;
}

std::size_t Cleaner::removePathContents(const std::string& path, std::string& message) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        message = "Missing: " + path;
        return 0;
    }

    std::size_t removed = 0;
    if (fs::is_regular_file(path, ec) || fs::is_symlink(path, ec)) {
        removed = fs::remove(path, ec) ? 1 : 0;
    } else if (fs::is_directory(path, ec)) {
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            if (ec) {
                message = "Failed listing: " + path + " (" + ec.message() + ")";
                return removed;
            }
            removed += static_cast<std::size_t>(fs::remove_all(entry.path(), ec));
            if (ec) {
                message = "Partial clean: " + path + " (" + ec.message() + ")";
                return removed;
            }
        }
    } else {
        message = "Skipped unsupported path: " + path;
        return 0;
    }

    message = "Removed from: " + path + " (" + std::to_string(removed) + " entries)";
    return removed;
}

std::size_t Cleaner::previewSelected(std::vector<std::string>& messages, std::uintmax_t& totalBytes) const {
    messages.clear();
    totalBytes = 0;
    std::size_t totalEntries = 0;

    const auto previews = previewItems();
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        const auto& preview = previews[i];
        if (!item.selected) {
            continue;
        }

        const char* risk = item.risk.c_str();
        if (!preview.exists) {
            messages.push_back(item.name + " [" + risk + "]: missing");
            continue;
        }
        if (!preview.accessible) {
            messages.push_back(item.name + " [" + risk + "]: access denied (grant Full Disk Access to terminal/app)");
            continue;
        }

        totalEntries += preview.entries;
        totalBytes += preview.bytes;
        messages.push_back(
            item.name +
            " [" + risk + "]: " +
            std::to_string(preview.entries) +
            " entries, " +
            std::to_string(preview.bytes) +
            " bytes"
        );
    }

    return totalEntries;
}

std::size_t Cleaner::cleanSelected(
    std::vector<std::string>& messages,
    bool dryRun,
    const std::function<void(std::size_t, std::size_t, const CleaningItem&)>& onProgress,
    std::vector<CleanResult>* results
) {
    std::size_t total = 0;
    std::size_t selectedTotal = 0;
    std::size_t processed = 0;
    messages.clear();
    if (results) {
        results->clear();
    }

    for (const auto& item : items_) {
        if (item.selected) {
            ++selectedTotal;
        }
    }

    for (const auto& item : items_) {
        if (!item.selected) continue;
        ++processed;
        const CleanResult result = cleanItem(item, dryRun);
        messages.push_back(item.name + ": " + result.detail);
        if (result.status == "removed") {
            total += result.entries;
        }
        if (results) {
            results->push_back(result);
        }
        if (onProgress) {
            onProgress(processed, selectedTotal, item);
        }
    }
    return total;
}

Cleaner::CleanResult Cleaner::cleanItem(const CleaningItem& item, bool dryRun) const {
    if (!isSafePath(item.path)) {
        return CleanResult{item.name, item.path, item.risk, "skipped", 0, "Skipped unsafe path: " + item.path};
    }
    if (!item.selectable) {
        return CleanResult{item.name, item.path, item.risk, "skipped", 0, "Skipped protected candidate (risk=" + item.risk + ")"};
    }

    if (dryRun) {
        const auto preview = previewPath(fs::path(item.path), true);
        if (!preview.exists) {
            return CleanResult{item.name, item.path, item.risk, "missing", 0, "[dry-run] Missing: " + item.path};
        }
        if (!preview.accessible) {
            return CleanResult{item.name, item.path, item.risk, "error", 0, "[dry-run] Access denied: " + item.path};
        }
        return CleanResult{
            item.name,
            item.path,
            item.risk,
            "dry-run",
            preview.entries,
            "[dry-run] Would clean: " + item.path + " (" + std::to_string(preview.entries) + " entries)"
        };
    }

    std::string message;
    // Precaucion: esta llamada elimina de forma recursiva el contenido de la ruta seleccionada.
    // Solo debe ejecutarse sobre ubicaciones seguras del usuario y tras confirmacion explicita.
    const std::size_t removedNow = removePathContents(item.path, message);

    std::string status = "done";
    if (message.rfind("Removed from:", 0) == 0) {
        status = "removed";
    } else if (message.rfind("Missing:", 0) == 0) {
        status = "missing";
    } else if (message.rfind("Failed", 0) == 0 || message.rfind("Partial clean:", 0) == 0) {
        status = "error";
    }
    return CleanResult{item.name, item.path, item.risk, status, removedNow, message};
}
