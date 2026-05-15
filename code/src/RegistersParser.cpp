/**
 * @file RegistersParser.cpp
 * @brief Implementation of the registers configuration file parser.
 */

#include <fstream>
#include <sstream>
#include <algorithm>
#include "parser/RegistersParser.h"

bool RegistersParser::parse(const std::string &filename, RegisterConfig &config) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string lineStr;
    while (std::getline(file, lineStr)) {
        lineStr.erase(0, lineStr.find_first_not_of(" \t\r\n"));
        if (lineStr.empty() || lineStr[0] == '#') continue;

        auto colonPos = lineStr.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = lineStr.substr(0, colonPos);
        std::string val = lineStr.substr(colonPos + 1);

        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t\r\n") + 1);

        if (key == "registers") {
            try { config.numRegisters = std::stoi(val); } catch (...) { return false; }
        } else if (key == "algorithm") {
            auto commaPos = val.find(',');
            if (commaPos != std::string::npos) {
                config.algorithm = val.substr(0, commaPos);
                config.algorithm.erase(config.algorithm.find_last_not_of(" \t") + 1);
                std::string param = val.substr(commaPos + 1);
                param.erase(0, param.find_first_not_of(" \t"));
                try { config.algorithmParam = std::stoi(param); } catch (...) { return false; }
            } else {
                config.algorithm = val;
                config.algorithmParam = 0;
            }
        }
    }
    return config.numRegisters > 0;
}
