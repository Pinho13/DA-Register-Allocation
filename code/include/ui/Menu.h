#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>

/**
 * @brief Terminal UI renderer — draws boxes, arrow menus, and prompts.
 *
 * Pure presentation layer. Knows nothing about domain data (submissions,
 * reviewers, flow networks). Receives pre-formatted strings to display.
 */
class Menu {
public:
    void displayInBox(const std::string &subtitle, const std::vector<std::string> &lines);
    std::string promptInBox(const std::string &subtitle, const std::string &prompt);
    int arrowMenu(const std::vector<std::string> &options);

    static const std::string CLEAR_SCREEN;

private:
    static const std::string HIGHLIGHT_ON;
    static const std::string HIGHLIGHT_OFF;
    static const std::string COLOR_RED;
    static const std::string COLOR_RESET;
    static const std::string CURSOR_HIDE;
    static const std::string CURSOR_SHOW;
    static const std::string CURSOR_SAVE;
    static const std::string CURSOR_RESTORE;

    static const int BOX_INNER;
    static const std::string BOX_TOP;
    static const std::string BOX_MID;
    static const std::string BOX_BOTTOM;

    static void getTerminalSize(int &rows, int &cols);
    static std::string hPad(int cols);
    static std::string cursorToRow(int row);
    static std::string cursorToPos(int row, int col);
    static std::string boxLine(const std::string &text);
    static std::string boxLineArrow(const std::string &text);
    static std::string menuLine(const std::string &text, bool highlighted);
};

#endif
