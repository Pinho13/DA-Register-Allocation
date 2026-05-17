/**
 * @file main.cpp
 * @brief Entry point — batch mode dispatcher and interactive menu loop.
 */

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include <climits>
#include <filesystem>
#ifdef __APPLE__
#  include <mach-o/dyld.h>
#endif
#include "ui/Menu.h"
#include "io/BatchProcessor.h"
#include "ui/DisplayFormatter.h"
#include "parser/RangesParser.h"
#include "parser/RegistersParser.h"
#include "core/WebBuilder.h"
#include "core/InterferenceGraph.h"
#include "core/RegisterAllocator.h"
#include "io/OutputWriter.h"
#include "models/AllocationData.h"

namespace fs = std::filesystem;

struct termios g_originalTermios;

static fs::path datasetPath() {
    char buf[PATH_MAX];
#ifdef __APPLE__
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return fs::current_path() / "dataset";
    return fs::canonical(buf).parent_path() / ".." / ".." / "dataset";
#else
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0) return fs::current_path() / "dataset";
    buf[len] = '\0';
    return fs::path(buf).parent_path() / ".." / ".." / "dataset";
#endif
}

void restoreTerminal(int) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_originalTermios);
    std::cout << "\033[?25h" << std::flush;
    std::exit(0);
}

static void runInteractiveMenu() {
    Menu menu;

    std::vector<LiveRange> ranges;
    RegisterConfig config;
    std::vector<Web> webs;
    AllocationResult lastResult;
    bool rangesLoaded = false;
    bool configLoaded = false;
    std::string loadedRangesFile;
    std::string loadedConfigFile;

    const std::vector<std::string> menuOptions = {
        "Load ranges file",
        "Load registers file",
        "Show live ranges",
        "Build webs & show interference graph",
        "Run register allocation",
        "Show allocation result",
        "Save result to file",
        "Run all basic test datasets",
        "Exit"
    };

    while (true) {
        int choice = menu.arrowMenu(menuOptions);

        switch (choice) {
            case 0: {
                std::string filename = menu.browseFile(datasetPath(), "Load Ranges File", "registers");
                if (filename.empty()) break;
                ranges.clear();
                webs.clear();
                rangesLoaded = false;
                if (RangesParser::parse(filename, ranges)) {
                    rangesLoaded = true;
                    loadedRangesFile = filename;
                    menu.displayInBox("Load Ranges File", {
                        "Loaded: " + filename,
                        "Live range entries: " + std::to_string(ranges.size())
                    });
                } else {
                    menu.displayInBox("Load Ranges File", {
                        "!!Error: Could not open file.",
                        "!!Path: \"" + filename + "\""
                    });
                }
                break;
            }
            case 1: {
                std::string filename = menu.browseFile(datasetPath(), "Load Registers File", "ranges");
                if (filename.empty()) break;
                configLoaded = false;
                if (RegistersParser::parse(filename, config)) {
                    configLoaded = true;
                    loadedConfigFile = filename;
                    auto lines = DisplayFormatter::formatConfig(config);
                    lines.insert(lines.begin(), "Loaded: " + filename);
                    menu.displayInBox("Load Registers File", lines);
                } else {
                    menu.displayInBox("Load Registers File", {
                        "!!Error: Could not open or parse file.",
                        "!!Path: \"" + filename + "\""
                    });
                }
                break;
            }
            case 2: {
                if (!rangesLoaded)
                    menu.displayInBox("Live Ranges", {"!!No ranges file loaded.", "Use option 1 first."});
                else
                    menu.displayInBox("Live Ranges", DisplayFormatter::formatRanges(ranges));
                break;
            }
            case 3: {
                if (!rangesLoaded) {
                    menu.displayInBox("Webs & Interference Graph", {"!!No ranges loaded.", "Use option 1 first."});
                    break;
                }
                webs.clear();
                WebBuilder::build(ranges, webs);
                InterferenceGraph ig(webs);
                auto lines = DisplayFormatter::formatWebs(webs);
                lines.push_back("");
                lines.push_back("Interference edges:");
                for (int i = 0; i < ig.size(); i++)
                    for (int j : ig.neighbours(i))
                        if (j > i)
                            lines.push_back("  web" + std::to_string(i) + " <-> web" + std::to_string(j));
                menu.displayInBox("Webs & Interference Graph", lines);
                break;
            }
            case 4: {
                if (!rangesLoaded || !configLoaded) {
                    menu.displayInBox("Register Allocation", {
                        "!!Both ranges and registers files must be loaded.",
                        "Use options 1 and 2 first."
                    });
                    break;
                }
                webs.clear();
                WebBuilder::build(ranges, webs);
                InterferenceGraph ig(webs);
                lastResult = RegisterAllocator::allocate(webs, ig, config);
                menu.displayInBox("Register Allocation", DisplayFormatter::formatAllocationResult(lastResult));
                break;
            }
            case 5: {
                if (lastResult.webs.empty())
                    menu.displayInBox("Allocation Result", {"!!No allocation performed yet.", "Use option 4 first."});
                else
                    menu.displayInBox("Allocation Result", DisplayFormatter::formatAllocationResult(lastResult));
                break;
            }
            case 6: {
                if (lastResult.webs.empty()) {
                    menu.displayInBox("Save Result", {"!!No allocation to save.", "Run allocation first (option 4)."});
                    break;
                }
                std::string filename = menu.promptInBox("Save Result", "Output file path: ");
                OutputWriter::write(filename, lastResult);
                menu.displayInBox("Save Result", {"Result saved to: " + filename});
                break;
            }
            case 7: {
                auto lines = BatchProcessor::runAllBasicDatasets();
                menu.displayInBox("All Basic Datasets", lines);
                break;
            }
            case 8:
                std::cout << "\033[?25h" << Menu::CLEAR_SCREEN;
                return;
        }
    }
}

int main(int argc, char *argv[]) {
    tcgetattr(STDIN_FILENO, &g_originalTermios);
    std::signal(SIGINT, restoreTerminal);

    // Batch mode (full paths):  -b <ranges> <registers> <out>
    // Batch mode (short form):  -b <folder> <ranges-name> <registers-name> <out>
    if (argc >= 5 && std::string(argv[1]) == "-b") {
        if (argc == 5)
            return BatchProcessor::run(argv[2], argv[3], argv[4]);

        if (argc == 6) {
            fs::path root = datasetPath();
            std::string folder        = argv[2];
            std::string rangesName    = argv[3];
            std::string registersName = argv[4];
            std::string outFile       = argv[5];
            if (rangesName.size() < 4 || rangesName.substr(rangesName.size() - 4) != ".txt")
                rangesName += ".txt";
            if (registersName.size() < 4 || registersName.substr(registersName.size() - 4) != ".txt")
                registersName += ".txt";
            return BatchProcessor::run(
                (root / folder / "ranges"    / rangesName).string(),
                (root / folder / "registers" / registersName).string(),
                outFile);
        }

        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " -b <ranges.txt> <registers.txt> <out.txt>\n"
                  << "  " << argv[0] << " -b <folder> <ranges-name> <registers-name> <out.txt>\n";
        return 1;
    }

    runInteractiveMenu();
    return 0;
}
