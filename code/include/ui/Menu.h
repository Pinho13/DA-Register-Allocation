#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>
#include <filesystem>

/**
 * @brief Terminal UI renderer — draws boxes, arrow menus, and prompts.
 *
 * Pure presentation layer. Knows nothing about domain data.
 * Receives pre-formatted strings and renders them using ANSI escape codes.
 */
class Menu {
public:
    /**
     * @brief Renders a centred box with a subtitle and a list of content lines.
     *
     * Lines prefixed with "!!" are rendered in red. Waits for Enter before returning.
     *
     * @param subtitle Label displayed below the title bar.
     * @param lines    Content lines to display inside the box.
     */
    void displayInBox(const std::string &subtitle, const std::vector<std::string> &lines);

    /**
     * @brief Renders a box with an inline text prompt and returns the user's input.
     *
     * @param subtitle Label displayed below the title bar.
     * @param prompt   Prompt string shown on the input line.
     * @return The string entered by the user (whitespace-trimmed by cin).
     */
    std::string promptInBox(const std::string &subtitle, const std::string &prompt);

    /**
     * @brief Renders an arrow-key navigable menu and returns the selected index.
     *
     * Scrolls automatically when options exceed the terminal height.
     * Up/down arrow keys move selection; Enter confirms.
     *
     * @param options List of option labels to display.
     * @return Zero-based index of the selected option.
     */
    int arrowMenu(const std::vector<std::string> &options);

    /**
     * @brief Interactive filesystem browser starting at @p startDir.
     *
     * Directories are listed before files. Selecting a directory descends into it;
     * selecting a file returns its full path. Auto-descends past intermediate
     * subdirectories named @p hideDirName when entering a folder.
     *
     * @param startDir    Root directory to start browsing from.
     * @param subtitle    Label shown in the menu box header.
     * @param hideDirName If non-empty, directories with this name are hidden from listing.
     * @return Full path of the selected file, or "" if the user cancelled.
     */
    std::string browseFile(const std::filesystem::path &startDir,
                           const std::string &subtitle,
                           const std::string &hideDirName = "");

    static const std::string CLEAR_SCREEN; /**< ANSI escape: clear screen and home cursor. */

private:
    // ANSI escape code constants
    static const std::string HIGHLIGHT_ON;   /**< Reverse-video on. */
    static const std::string HIGHLIGHT_OFF;  /**< All attributes reset. */
    static const std::string COLOR_RED;      /**< Foreground red. */
    static const std::string COLOR_RESET;    /**< Foreground reset. */
    static const std::string CURSOR_HIDE;    /**< Hide cursor. */
    static const std::string CURSOR_SHOW;    /**< Show cursor. */
    static const std::string CURSOR_SAVE;    /**< Save cursor position. */
    static const std::string CURSOR_RESTORE; /**< Restore cursor position. */

    // Box drawing constants
    static const int BOX_INNER;         /**< Inner content width in display columns (66). */
    static const std::string BOX_TOP;   /**< Top border line. */
    static const std::string BOX_MID;   /**< Mid separator line. */
    static const std::string BOX_BOTTOM;/**< Bottom border line. */

    /**
     * @brief Queries the terminal dimensions via ioctl.
     * @param rows Output: terminal row count.
     * @param cols Output: terminal column count.
     */
    static void getTerminalSize(int &rows, int &cols);

    /**
     * @brief Returns a horizontal padding string to centre the box in @p cols columns.
     * @param cols Terminal width in columns.
     * @return A string of spaces for left-padding.
     */
    static std::string hPad(int cols);

    /**
     * @brief Returns an ANSI escape sequence to move the cursor to @p row, column 1.
     * @param row Target row (1-based).
     */
    static std::string cursorToRow(int row);

    /**
     * @brief Returns an ANSI escape sequence to move the cursor to (@p row, @p col).
     * @param row Target row (1-based).
     * @param col Target column (1-based).
     */
    static std::string cursorToPos(int row, int col);

    /**
     * @brief Wraps @p text in box-border characters padded to BOX_INNER display columns.
     * @param text Content string (no multibyte characters).
     * @return Formatted box line string.
     */
    static std::string boxLine(const std::string &text);

    /**
     * @brief Like boxLine but accounts for 4 extra bytes from two UTF-8 arrow characters.
     * @param text Content string containing exactly two ↑/↓ arrows (3 bytes each, 1 display col).
     * @return Formatted box line string.
     */
    static std::string boxLineArrow(const std::string &text);

    /**
     * @brief Formats a single menu option row, optionally highlighted.
     *
     * @param text        Option label text.
     * @param highlighted If true, wraps label in reverse-video escape codes.
     * @return Formatted menu row string.
     */
    static std::string menuLine(const std::string &text, bool highlighted);
};

#endif
