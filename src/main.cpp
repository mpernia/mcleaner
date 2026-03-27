#include "Tui.h"

#include <iostream>
#include <string>

#ifndef MCLEANER_VERSION
#define MCLEANER_VERSION "dev"
#endif

namespace {
void printHelp(const char* program) {
    std::cout
        << "Usage: " << program << " [--dry-run] [--help] [--version]\n"
        << "\n"
        << "Options:\n"
        << "  --dry-run   Simulate cleaning without deleting files\n"
        << "  --help      Show this help and exit\n"
        << "  --version   Show version and exit\n";
}
}

int main(int argc, char** argv) {
    bool dryRun = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dry-run") {
            dryRun = true;
        } else if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mcleaner " << MCLEANER_VERSION << "\n";
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n\n";
            printHelp(argv[0]);
            return 1;
        }
    }

    Tui tui(dryRun);
    return tui.run();
}
