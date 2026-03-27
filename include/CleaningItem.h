#pragma once

#include <string>

struct CleaningItem {
    std::string name;
    std::string path;
    std::string risk{"safe"};
    bool hidden{false};
    bool selected{true};
    bool selectable{true};
};
