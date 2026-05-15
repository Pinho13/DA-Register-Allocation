/**
 * @file main.cpp
 * @brief Entry point for the Register Allocation tool.
 *
 * Supports two execution modes:
 *  - **Interactive**: arrow-key menu for loading files, inspecting webs/graphs, running
 *    allocation algorithms, and saving results.
 *  - **Batch**: `./register_alloc -b ranges.txt registers.txt output.txt`
 */

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <termios.h>
#include <unistd.h>
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

struct termios g_originalTermios;

void restoreTerminal(int) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_originalTermios);
    std::cout << "\033[?25h" << std::flush;
    std::exit(0);
}

int main(int argc, char *argv[]) {
    tcgetattr(STDIN_FILENO, &g_originalTermios);
    std::signal(SIGINT, restoreTerminal);

    // Batch mode: ./register_alloc -b ranges.txt registers.txt allocation.txt
    if (argc == 5 && std::string(argv[1]) == "-b") {
        return BatchProcessor::run(argv[2], argv[3], argv[4]);
    }

    Menu menu;

    // Application state
    std::vector<LiveRange> ranges;
    RegisterConfig config;
    std::vector<Web> webs;
    AllocationResult lastResult;
    bool rangesLoaded = false;
    bool configLoaded = false;
    std::string loadedRangesFile;
    std::string loadedConfigFile;

    std::vector<std::string> menuOptions = {
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
                std::string filename = menu.promptInBox("Load Ranges File", "File path: ");
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
                std::string filename = menu.promptInBox("Load Registers File", "File path: ");
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
                if (!rangesLoaded) {
                    menu.displayInBox("Live Ranges", {"!!No ranges file loaded.", "Use option 1 first."});
                } else {
                    menu.displayInBox("Live Ranges", DisplayFormatter::formatRanges(ranges));
                }
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
                for (int i = 0; i < ig.size(); i++) {
                    for (int j : ig.neighbours(i)) {
                        if (j > i) {
                            lines.push_back("  web" + std::to_string(i) + " <-> web" + std::to_string(j));
                        }
                    }
                }
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
                auto lines = DisplayFormatter::formatAllocationResult(lastResult);
                menu.displayInBox("Register Allocation", lines);
                break;
            }
            case 5: {
                if (lastResult.webs.empty()) {
                    menu.displayInBox("Allocation Result", {"!!No allocation performed yet.", "Use option 5 first."});
                } else {
                    menu.displayInBox("Allocation Result", DisplayFormatter::formatAllocationResult(lastResult));
                }
                break;
            }
            case 6: {
                if (lastResult.webs.empty()) {
                    menu.displayInBox("Save Result", {"!!No allocation to save.", "Run allocation first (option 5)."});
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
                return 0;
        }
    }

    return 0;
}
