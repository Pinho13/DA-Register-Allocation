#include <iostream>
#include <limits>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "Menu.h"

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
    return "\033[" + std::to_string(row) + ";1H";
}

std::string Menu::cursorToPos(int row, int col) {
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
    int topMargin = (termRows - boxHeight) / 2;

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
    int topMargin = (termRows - boxHeight) / 2;

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

    // Box height: top + empty + title + empty + mid + empty + options + empty + footer + bottom
    int boxHeight = 9 + (int)options.size();
    int topMargin = std::max(0, (termRows - boxHeight) / 2);

    int selected = 0;
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::cout << CLEAR_SCREEN;
    for (int i = 0; i < topMargin; i++) std::cout << "\n";
    std::cout << pad << BOX_TOP << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLine("                Conference Review Assignment Tool") << "\n";
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << BOX_MID << "\n";
    std::cout << pad << boxLine("") << "\n";

    // firstOptionRow is the terminal row where options start (1-indexed)
    int firstOptionRow = topMargin + 7;
    for (int i = 0; i < (int)options.size(); i++) {
        std::cout << pad << menuLine(options[i], i == selected) << "\n";
    }
    std::cout << pad << boxLine("") << "\n";
    std::cout << pad << boxLineArrow("  \u2191/\u2193 arrows, Enter to select") << "\n";
    std::cout << pad << BOX_BOTTOM << "\n";
    // Use absolute positioning to place cursor at bottom without scrolling
    int bottomRow = std::min(termRows, topMargin + boxHeight + topMargin);
    std::cout << cursorToPos(bottomRow, 1);
    std::cout << CURSOR_HIDE << std::flush;

    while (true) {
        char c = getchar();
        if (c == '\n') break;
        if (c == '\033') {
            getchar();
            char arrow = getchar();
            int oldSelected = selected;
            if (arrow == 'A') {
                selected = (selected > 0) ? selected - 1 : (int)options.size() - 1;
            } else if (arrow == 'B') {
                selected = (selected < (int)options.size() - 1) ? selected + 1 : 0;
            }

            if (oldSelected != selected) {
                std::cout << cursorToRow(firstOptionRow + oldSelected);
                std::cout << pad << menuLine(options[oldSelected], false);
                std::cout << cursorToRow(firstOptionRow + selected);
                std::cout << pad << menuLine(options[selected], true);
                std::cout << std::flush;
            }
        }
    }

    std::cout << CURSOR_SHOW;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return selected;
}
