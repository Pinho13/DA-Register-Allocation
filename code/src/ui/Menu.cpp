#include <iostream>
#include <limits>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <filesystem>
#include "ui/Menu.h"

namespace fs = std::filesystem;

// ANSI escape code constants
const std::string Menu::CLEAR_SCREEN    = "\033[2J\033[H";
const std::string Menu::HIGHLIGHT_ON    = "\033[7m";
const std::string Menu::HIGHLIGHT_OFF   = "\033[0m";
const std::string Menu::COLOR_RED       = "\033[31m";
const std::string Menu::COLOR_RESET     = "\033[0m";
const std::string Menu::CURSOR_HIDE     = "\033[?25l";
const std::string Menu::CURSOR_SHOW     = "\033[?25h";
const std::string Menu::CURSOR_SAVE     = "\033[s";
const std::string Menu::CURSOR_RESTORE  = "\033[u";

const int Menu::BOX_INNER = 66;
const std::string Menu::BOX_TOP    = "╔════════════════════════════════════════════════════════════════════╗";
const std::string Menu::BOX_MID    = "╠════════════════════════════════════════════════════════════════════╣";
const std::string Menu::BOX_BOTTOM = "╚════════════════════════════════════════════════════════════════════╝";

static const int BOX_OUTER = 70; // ║ + space + 66 inner + space + ║

void Menu::getTerminalSize(int &rows, int &cols) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0 && w.ws_col > 0) {
        rows = w.ws_row;
        cols = w.ws_col;
    } else {
        rows = 24;
        cols = 80;
    }
}

std::string Menu::hPad(int cols) {
    int pad = (cols - BOX_OUTER) / 2;
    if (pad <= 0) return "";
    return std::string(pad, ' ');
}

std::string Menu::cursorToRow(int row) {
    if (row < 1) row = 1;
    return "\033[" + std::to_string(row) + ";1H";
}

std::string Menu::cursorToPos(int row, int col) {
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    return "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H";
}

std::string Menu::boxLine(const std::string &text) {
    std::string padded = text;
    if ((int)padded.size() > BOX_INNER) padded = padded.substr(0, BOX_INNER);
    while ((int)padded.size() < BOX_INNER) padded += " ";
    return "║ " + padded + " ║";
}

std::string Menu::boxLineArrow(const std::string &text) {
    std::string padded = text;
    while ((int)padded.size() < BOX_INNER + 4) padded += " ";
    return "║ " + padded + " ║";
}

std::string Menu::menuLine(const std::string &text, bool highlighted) {
    std::string padded = text;
    while ((int)padded.size() < BOX_INNER - 3) padded += " ";
    if (highlighted)
        return "║  " + HIGHLIGHT_ON + " " + padded + HIGHLIGHT_OFF + "  ║";
    return "║   " + padded + "  ║";
}

void Menu::displayInBox(const std::string &subtitle, const std::vector<std::string> &lines) {
    int termRows, termCols;
    getTerminalSize(termRows, termCols);
    std::string pad = hPad(termCols);

    // Box height: top + empty + title + empty + mid + empty + subtitle + empty + lines + empty + footer + bottom = 10 + lines.size()
    int boxHeight = 11 + (int)lines.size();
    int topMargin = std::max(0, (termRows - boxHeight) / 2);

    std::cout << CLEAR_SCREEN;
    for (int i = 0; i < topMargin; i++) std::cout << "\n";
    std::cout << cursorToPos(topMargin + 1, 1);
    std::cout << pad << BOX_TOP << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("                Conference Review Assignment Tool") << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << BOX_MID << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("  " + subtitle) << "\n";
    std::cout << pad << boxLine("") << "\n";
    for (const auto &line : lines) {
        if (line.size() >= 2 && line[0] == '!' && line[1] == '!') {
            std::string content = line.substr(2);
            std::string padded = "  " + content;
            if ((int)padded.size() > BOX_INNER) padded = padded.substr(0, BOX_INNER);
            while ((int)padded.size() < BOX_INNER) padded += " ";
            std::cout << pad << "║ " << COLOR_RED << padded << COLOR_RESET << " ║\n";
        } else {
            std::cout << pad << boxLine("  " + line) << "\n";
        }
    }
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("  Press Enter to go back...") << "\n";
    std::cout << pad << BOX_BOTTOM << std::endl;
    for (int i = 0; i < topMargin; i++) std::cout << "\n";
    std::cout << CURSOR_HIDE << std::flush;

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    tcflush(STDIN_FILENO, TCIFLUSH);

    while (getchar() != '\n') {}
    std::cout << CURSOR_SHOW << std::flush;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

std::string Menu::promptInBox(const std::string &subtitle, const std::string &prompt) {
    int termRows, termCols;
    getTerminalSize(termRows, termCols);
    std::string pad = hPad(termCols);

    int boxHeight = 11;
    int topMargin = std::max(0, (termRows - boxHeight) / 2);

    std::cout << CLEAR_SCREEN;
    std::cout << cursorToPos(topMargin + 1, 1);
    std::cout << pad << BOX_TOP << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("                Conference Review Assignment Tool") << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << BOX_MID << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("  " + subtitle) << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("  " + prompt) << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << BOX_BOTTOM << "\n";
    std::cout << std::flush;

    // Prompt line is the 9th line of the box, box starts at topMargin+1
    int promptRow = topMargin + 9;
    int promptCol = (int)pad.size() + 4 + (int)prompt.size() + 1;
    std::cout << cursorToPos(promptRow, promptCol);

    std::string input;
    std::cin >> input;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return input;
}

int Menu::arrowMenu(const std::vector<std::string> &options) {
    int termRows, termCols;
    getTerminalSize(termRows, termCols);
    std::string pad = hPad(termCols);

    // Fixed chrome: top(1) + empty(1) + title(1) + empty(1) + mid(1) + empty(1)
    //             + empty(1) + footer(1) + bottom(1) = 9 lines around the options
    const int CHROME = 9;
    // Maximum visible options given terminal height
    int maxVisible = std::max(1, termRows - CHROME);
    int numOptions  = (int)options.size();
    int visCount    = std::min(numOptions, maxVisible);

    int topMargin    = std::max(0, (termRows - (CHROME + visCount)) / 2);

    int selected  = 0;
    int scrollTop = 0; // index of first visible option

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    auto redrawAll = [&]() {
        std::cout << CLEAR_SCREEN;
        for (int i = 0; i < topMargin; i++) std::cout << "\n";
        std::cout << pad << BOX_TOP << "\n";
        std::cout << pad << boxLine("") << "\n";
        std::cout << pad << boxLine("                Register Allocation Tool") << "\n";
        std::cout << pad << boxLine("") << "\n";
        std::cout << pad << BOX_MID << "\n";
        std::cout << pad << boxLine("") << "\n";
        for (int i = 0; i < visCount; i++) {
            int idx = scrollTop + i;
            std::string label = options[idx];
            // show scroll hints on first/last visible slot
            // menuLine pads by byte length; \u2191/\u2193 are 3 bytes but 1 display col,
            // so pre-pad the label to (BOX_INNER-3-2) display cols first.
            const int labelWidth = BOX_INNER - 3 - 2; // 2 = "\u2191 " display width
            if (i == 0 && scrollTop > 0) {
                while ((int)label.size() < labelWidth) label += ' ';
                label = "\u2191 " + label;
            } else if (i == visCount - 1 && scrollTop + visCount < numOptions) {
                while ((int)label.size() < labelWidth) label += ' ';
                label = "\u2193 " + label;
            }
            std::cout << pad << menuLine(label, idx == selected) << "\n";
        }
        std::cout << pad << boxLine("") << "\n";
        std::cout << pad << boxLineArrow("  \u2191/\u2193 arrows, Enter to select") << "\n";
        std::cout << pad << BOX_BOTTOM << "\n";
        int bottomRow = topMargin + CHROME + visCount;
        std::cout << cursorToPos(std::min(termRows, bottomRow), 1);
        std::cout << CURSOR_HIDE << std::flush;
    };

    redrawAll();

    while (true) {
        char c = getchar();
        if (c == '\n') break;
        if (c == '\033') {
            getchar();
            char arrow = getchar();
            int oldSelected = selected;
            if (arrow == 'A') {
                selected = (selected > 0) ? selected - 1 : numOptions - 1;
            } else if (arrow == 'B') {
                selected = (selected < numOptions - 1) ? selected + 1 : 0;
            }
            if (oldSelected == selected) continue;

            // adjust scroll window
            if (selected < scrollTop)
                scrollTop = selected;
            else if (selected >= scrollTop + visCount)
                scrollTop = selected - visCount + 1;

            redrawAll();
        }
    }

    std::cout << CURSOR_SHOW;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return selected;
}

std::string Menu::browseFile(const fs::path &startDir, const std::string &subtitle,
                              const std::string &hideDirName) {
    fs::path current = startDir;

    while (true) {
        // collect and sort entries: dirs first, then files
        std::vector<fs::path> dirs, files;
        try {
            for (const auto &entry : fs::directory_iterator(current)) {
                if (entry.is_directory()) {
                    if (hideDirName.empty() || entry.path().filename() != hideDirName)
                        dirs.push_back(entry.path());
                } else if (entry.is_regular_file()) {
                    files.push_back(entry.path());
                }
            }
        } catch (...) {}

        std::sort(dirs.begin(),  dirs.end());
        std::sort(files.begin(), files.end());

        // build option list
        std::vector<std::string> options;
        if (current != startDir)
            options.push_back(".. (back)");
        for (const auto &d : dirs)
            options.push_back("[dir] " + d.filename().string());
        for (const auto &f : files)
            options.push_back(f.filename().string());
        options.push_back("(cancel)");

        int choice = arrowMenu(options);
        const std::string &picked = options[choice];

        if (picked == "(cancel)")
            return "";

        if (picked == ".. (back)") {
            if (current == startDir) return "";
            fs::path up = current.parent_path();
            // if we auto-descended into a subdir (e.g. basic/ranges), skip back
            // past the intermediate dir to the startDir level
            if (up != startDir && up.parent_path() == startDir)
                up = startDir;
            current = up;
            continue;
        }

        if (picked.rfind("[dir] ", 0) == 0) {
            fs::path next = current / picked.substr(6);
            // auto-descend into the matching subdir if it exists
            if (!hideDirName.empty()) {
                fs::path target = next / (hideDirName == "registers" ? "ranges" : "registers");
                if (fs::is_directory(target))
                    next = target;
            }
            current = next;
            continue;
        }

        // it's a file
        return (current / picked).string();
    }
}
