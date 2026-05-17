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

// Returns a string that, when printed, occupies exactly `displayCols` terminal columns.
// `extraBytes` accounts for multibyte UTF-8 sequences already in `text` whose byte
// count exceeds their display width (e.g. each ↑/↓ arrow = 3 bytes but 1 display col).
static std::string padTo(const std::string &text, int displayCols, int extraBytes = 0) {
    int target = displayCols + extraBytes; // target byte length
    std::string padded = text;
    if ((int)padded.size() > target) padded = padded.substr(0, target);
    while ((int)padded.size() < target) padded += " ";
    return padded;
}

// Centers `text` within `width` display columns (text contains no multibyte chars).
static std::string centered(const std::string &text, int width) {
    int pad = (width - (int)text.size()) / 2;
    if (pad < 0) pad = 0;
    return std::string(pad, ' ') + text;
}

std::string Menu::boxLine(const std::string &text) {
    return "║ " + padTo(text, BOX_INNER) + " ║";
}

// For lines that contain exactly two 3-byte UTF-8 arrows (↑ and ↓): each costs
// 2 extra bytes vs. display width, so we pad to BOX_INNER + 4 bytes.
std::string Menu::boxLineArrow(const std::string &text) {
    return "║ " + padTo(text, BOX_INNER, 4) + " ║";
}

std::string Menu::menuLine(const std::string &text, bool highlighted) {
    // The slot inside the box borders is BOX_INNER - 3 display cols:
    //   non-highlighted: ║ + 3 spaces + slot + 2 spaces + ║  (3+slot+2 = BOX_INNER-1? No.)
    //   ║(1) + space(1) + 3spaces + slot + 2spaces + space(1) + ║(1) — that's BOX_OUTER=70
    //   so slot = 70 - 1 - 1 - 3 - 2 - 1 - 1 = 61? Actually:
    //   "║   " = 4, "  ║" = 3, total border = 7, slot = BOX_OUTER - 7 = 63 = BOX_INNER - 3.
    std::string padded = padTo(text, BOX_INNER - 3, /* extraBytes in text */ 0);
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
    std::cout << pad << boxLine(centered("Register Allocation Tool", BOX_INNER)) << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << BOX_MID << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("  " + subtitle) << "\n";
    std::cout << pad << boxLine("") << "\n";
    for (const auto &line : lines) {
        if (line.size() >= 2 && line[0] == '!' && line[1] == '!') {
            std::string content = "  " + line.substr(2);
            std::cout << pad << "║ " << COLOR_RED << padTo(content, BOX_INNER) << COLOR_RESET << " ║\n";
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
    std::cout << pad << boxLine(centered("Register Allocation Tool", BOX_INNER)) << "\n";
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
        std::cout << pad << boxLine(centered("Register Allocation Tool", BOX_INNER)) << "\n";
        std::cout << pad << boxLine("") << "\n";
        std::cout << pad << BOX_MID << "\n";
        std::cout << pad << boxLine("") << "\n";
        for (int i = 0; i < visCount; i++) {
            int idx = scrollTop + i;
            std::string label = options[idx];
            // \u2191/\u2193 each = 3 bytes, 1 display col \u2192 2 extra bytes per arrow.
            // Pre-pad label to (BOX_INNER-3-2) display cols, then prepend "\u2191 " or "\u2193 ".
            // The resulting string has 2 extra bytes vs. its display width, which
            // menuLine compensates for via padTo(..., extraBytes=2).
            bool hasArrow = false;
            const int slotWidth = BOX_INNER - 3;      // display cols in the menuLine slot
            const int labelWidth = slotWidth - 2;      // leave 2 cols for arrow + space
            if (i == 0 && scrollTop > 0) {
                while ((int)label.size() < labelWidth) label += ' ';
                label = "\u2191 " + label;
                hasArrow = true;
            } else if (i == visCount - 1 && scrollTop + visCount < numOptions) {
                while ((int)label.size() < labelWidth) label += ' ';
                label = "\u2193 " + label;
                hasArrow = true;
            }
            // Pass extraBytes=2 when an arrow is present so padTo accounts for the
            // 2-byte gap between byte length and display width of the UTF-8 arrow.
            std::string padded = padTo(label, slotWidth, hasArrow ? 2 : 0);
            bool hi = (idx == selected);
            std::string row = hi
                ? "\u2551  " + HIGHLIGHT_ON + " " + padded + HIGHLIGHT_OFF + "  \u2551"
                : "\u2551   " + padded + "  \u2551";
            std::cout << pad << row << "\n";
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
