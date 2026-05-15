#ifndef ALLOCATIONDATA_H
#define ALLOCATIONDATA_H

#include <string>
#include <vector>
#include <set>

/**
 * @file AllocationData.h
 * @brief Core data structures for the Register Allocation project.
 */

/**
 * @brief Represents a single live range for a variable: a set of program line numbers.
 *
 * A line number may carry a '+' (definition point) or '-' (last-use point) marker.
 * These are stored separately so interference logic can use them.
 */
struct LiveRange {
    std::string varName;           /**< Variable this range belongs to */
    std::vector<int> lines;        /**< Sorted line numbers (without markers) */
    std::set<int> defLines;        /**< Lines marked with '+' (definition points) */
    std::set<int> useLines;        /**< Lines marked with '-' (last-use points) */
};

/**
 * @brief Represents a web: the union of one or more merged live ranges for a variable.
 *
 * A web is the unit that gets assigned a register (or memory).
 */
struct Web {
    int id;                        /**< Unique web index (web0, web1, ...) */
    std::string varName;           /**< Variable name this web belongs to */
    std::vector<int> lines;        /**< Sorted merged program line numbers */
    std::set<int> defLines;        /**< Definition points across all merged ranges */
    std::set<int> useLines;        /**< Last-use points across all merged ranges */
    int assignedRegister;          /**< -1 = unassigned, -2 = spilled to memory, >= 0 = register index */

    Web() : id(-1), assignedRegister(-1) {}

    /** @brief Returns the set of program points as a sorted set for interference checks. */
    std::set<int> lineSet() const {
        return std::set<int>(lines.begin(), lines.end());
    }
};

/**
 * @brief Holds the parsed register configuration from the registers input file.
 */
struct RegisterConfig {
    int numRegisters;              /**< Maximum number of physical registers available */
    std::string algorithm;         /**< Algorithm name: "basic", "spilling", "splitting", "free" */
    int algorithmParam;            /**< Numeric parameter K for spilling/splitting (0 if unused) */

    RegisterConfig() : numRegisters(0), algorithm("basic"), algorithmParam(0) {}
};

/**
 * @brief Holds the full result of a register allocation run.
 */
struct AllocationResult {
    std::vector<Web> webs;         /**< All webs, in order, with assignments filled in */
    int registersUsed;             /**< Number of registers actually used (0 = all spilled) */
    bool feasible;                 /**< True if allocation succeeded without spilling everything */

    AllocationResult() : registersUsed(0), feasible(false) {}
};

#endif
