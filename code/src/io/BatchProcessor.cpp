/**
 * @file BatchProcessor.cpp
 * @brief Implementation of the non-interactive batch execution mode.
 */

#include <iostream>
#include <fstream>
#include <map>
#include <filesystem>
#include <climits>
#ifdef __APPLE__
#  include <mach-o/dyld.h>
#else
#  include <unistd.h>
#endif
#include "io/BatchProcessor.h"
#include "parser/RangesParser.h"
#include "parser/RegistersParser.h"
#include "core/WebBuilder.h"
#include "core/InterferenceGraph.h"
#include "core/RegisterAllocator.h"
#include "io/OutputWriter.h"

namespace fs = std::filesystem;

int BatchProcessor::run(const std::string &rangesFile,
                        const std::string &registersFile,
                        const std::string &outputFile) {
    std::vector<LiveRange> ranges;
    if (!RangesParser::parse(rangesFile, ranges)) {
        std::cerr << "Error: Failed to parse ranges file: " << rangesFile << "\n";
        return 1;
    }

    RegisterConfig config;
    if (!RegistersParser::parse(registersFile, config)) {
        std::cerr << "Error: Failed to parse registers file: " << registersFile << "\n";
        return 1;
    }

    std::vector<Web> webs;
    WebBuilder::build(ranges, webs);

    InterferenceGraph ig(webs);
    AllocationResult result = RegisterAllocator::allocate(webs, ig, config);

    OutputWriter::write(outputFile, result);
    return 0;
}

// ── path helpers ──────────────────────────────────────────────────────────────

static fs::path projectRoot() {
    char buf[PATH_MAX];
#ifdef __APPLE__
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return fs::current_path();
    fs::path binDir = fs::canonical(buf).parent_path();
#else
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0) return fs::current_path();
    buf[len] = '\0';
    fs::path binDir = fs::path(buf).parent_path();
#endif
    // binary is at <root>/code/cmake-build/register_alloc
    return binDir / ".." / "..";
}

static fs::path datasetRoot() { return projectRoot() / "dataset"; }
static fs::path outputsRoot() { return projectRoot() / "outputs"; }

// ── shared internal types ─────────────────────────────────────────────────────

namespace {

struct TestSpec {
    std::string rangesFile;      // relative to dataset root
    std::string registersFile;   // relative to dataset root
    std::string label;
    std::string feasibleExpect;  // "yes" | "no" | ""
    int regsUsedExpect   = -1;
    int regsAtMostExpect = -1;
    std::string outputFilename;  // written flat into outputs/<folder>/
};

struct RunResult {
    bool loaded       = false;
    bool feasible     = false;
    int  registersUsed = 0;
    AllocationResult alloc;
    std::string failReason;
};

RunResult runSpec(const fs::path &root, const TestSpec &tc) {
    RunResult r;
    std::vector<LiveRange> ranges;
    RegisterConfig config;
    if (!RangesParser::parse((root / tc.rangesFile).string(), ranges) ||
        !RegistersParser::parse((root / tc.registersFile).string(), config)) {
        r.failReason = "FAILED to load files";
        return r;
    }
    r.loaded = true;
    std::vector<Web> webs;
    WebBuilder::build(ranges, webs);
    InterferenceGraph ig(webs);
    r.alloc = RegisterAllocator::allocate(webs, ig, config);
    r.feasible = r.alloc.feasible;
    r.registersUsed = r.alloc.registersUsed;

    if (!tc.feasibleExpect.empty()) {
        bool want = (tc.feasibleExpect == "yes");
        if (want && !r.feasible)
            r.failReason = "expected feasible, got infeasible";
        else if (!want && r.feasible)
            r.failReason = "expected infeasible, got feasible";
    }
    if (r.failReason.empty() && tc.regsUsedExpect >= 0 && r.registersUsed != tc.regsUsedExpect)
        r.failReason = "expected registers=" + std::to_string(tc.regsUsedExpect) +
                       ", got " + std::to_string(r.registersUsed);
    if (r.failReason.empty() && tc.regsAtMostExpect >= 0 && r.registersUsed > tc.regsAtMostExpect)
        r.failReason = "expected registers<=" + std::to_string(tc.regsAtMostExpect) +
                       ", got " + std::to_string(r.registersUsed);
    return r;
}

// Writes one allocation result in the same format as OutputWriter::write.
void writeAllocBlock(std::ofstream &f, const AllocationResult &result) {
    f << "# Total number of webs followed by the listing of the program points of each one\n";
    f << "# program points in each web are sorted in ascending order\n";
    f << "webs: " << result.webs.size() << "\n";
    for (const auto &w : result.webs) {
        f << "web" << w.id << ": ";
        bool first = true;
        for (int ln : w.lines) {
            bool isDef = w.defLines.count(ln) > 0;
            bool isUse = w.useLines.count(ln) > 0;
            if (!first) f << ",";
            first = false;
            if (isDef && isUse)  f << ln;
            else if (isDef)      f << ln << "+";
            else if (isUse)      f << ln << "-";
            else                 f << ln;
        }
        f << "\n";
    }
    f << "# Total number of registers used, followed by assignment to webs\n";
    f << "registers: " << result.registersUsed << "\n";
    if (result.registersUsed == 0) {
        for (const auto &w : result.webs) f << "M: web" << w.id << "\n";
    } else {
        std::map<int, std::vector<int>> regToWebs;
        std::vector<int> memWebs;
        for (const auto &w : result.webs) {
            if (w.assignedRegister >= 0) regToWebs[w.assignedRegister].push_back(w.id);
            else                         memWebs.push_back(w.id);
        }
        for (auto &[reg, ids] : regToWebs)
            for (int id : ids) f << "r" << reg << ": web" << id << "\n";
        for (int id : memWebs) f << "M: web" << id << "\n";
    }
}

// All test specs mirroring run_tests.sh exactly, grouped by dataset folder.
// outputFilename is written flat into outputs/<folder>/ (no subfolders).
using Section = std::pair<std::string, std::vector<TestSpec>>;
std::vector<Section> allSections() {
    return {
        { "basic", {
            {"basic/ranges/ranges1.txt", "basic/registers/registers2.txt",
             "r1-reg2",   "yes", -1,  2, "ranges1_registers2.txt"},
            {"basic/ranges/ranges2.txt", "basic/registers/registers2.txt",
             "r2-reg2",   "yes", -1,  2, "ranges2_registers2.txt"},
            {"basic/ranges/ranges3.txt", "basic/registers/registers2.txt",
             "r3-reg2",   "yes", -1,  2, "ranges3_registers2.txt"},
            {"basic/ranges/ranges4.txt", "basic/registers/registers1.txt",
             "r4-reg1",   "yes",  1, -1, "ranges4_registers1.txt"},
            {"basic/ranges/ranges5.txt", "basic/registers/registers1.txt",
             "r5-reg1",   "yes",  1, -1, "ranges5_registers1.txt"},
            {"basic/ranges/ranges6.txt", "basic/registers/registers3.txt",
             "r6-reg3",   "yes", -1,  3, "ranges6_registers3.txt"},
        }},
        { "edge_cases", {
            // basic algorithm
            {"edge_cases/ranges/disconnected.txt",      "edge_cases/registers/basic1.txt",
             "basic/disconnected",       "yes", 1, -1, "disconnected_basic1.txt"},
            {"edge_cases/ranges/no_interference.txt",   "edge_cases/registers/basic1.txt",
             "basic/no_interference",    "yes", 1, -1, "no_interference_basic1.txt"},
            {"edge_cases/ranges/same_line_def_use.txt", "edge_cases/registers/basic1.txt",
             "basic/same_line_def_use",  "yes", 1, -1, "same_line_def_use_basic1.txt"},
            {"edge_cases/ranges/dsatur_dense.txt",      "edge_cases/registers/basic4.txt",
             "basic/dsatur_dense/4regs", "yes", 4, -1, "dsatur_dense_basic4.txt"},
            {"edge_cases/ranges/dsatur_dense.txt",      "edge_cases/registers/basic2.txt",
             "basic/dsatur_dense/2regs", "no", -1, -1, "dsatur_dense_basic2.txt"},
            // spilling
            {"edge_cases/ranges/spill_basic.txt",     "edge_cases/registers/spill1.txt",
             "spill/basic/K=1",   "yes", -1, -1, "spill_basic_spill1.txt"},
            {"edge_cases/ranges/spill_exceeds_k.txt", "edge_cases/registers/spill2.txt",
             "spill/exceeds/K=1", "no",  -1, -1, "spill_exceeds_k_spill2.txt"},
            {"edge_cases/ranges/spill_exceeds_k.txt", "edge_cases/registers/spill3.txt",
             "spill/exceeds/K=2", "yes", -1, -1, "spill_exceeds_k_spill3.txt"},
            // splitting
            {"edge_cases/ranges/split_simple.txt",       "edge_cases/registers/split_k1.txt",
             "split/simple/K=1",       "", -1, -1, "split_simple_split_k1.txt"},
            {"edge_cases/ranges/split_needed.txt",       "edge_cases/registers/split_k2.txt",
             "split/needed/K=2",       "", -1, -1, "split_needed_split_k2.txt"},
            {"edge_cases/ranges/split_unsplittable.txt", "edge_cases/registers/split_k5.txt",
             "split/unsplittable/K=5", "no",-1, -1,"split_unsplittable_split_k5.txt"},
            // dsatur
            {"edge_cases/ranges/dsatur_dense.txt", "edge_cases/registers/dsatur1.txt",
             "dsatur/dense/1reg",  "yes", 1, -1, "dsatur_dense_dsatur1.txt"},
            {"edge_cases/ranges/dsatur_dense.txt", "edge_cases/registers/dsatur3.txt",
             "dsatur/dense/3regs", "yes", 3, -1, "dsatur_dense_dsatur3.txt"},
            {"edge_cases/ranges/dsatur_dense.txt", "edge_cases/registers/dsatur4.txt",
             "dsatur/dense/4regs", "yes", 4, -1, "dsatur_dense_dsatur4.txt"},
            // free (BCT-Color)
            {"edge_cases/ranges/disconnected.txt",      "edge_cases/registers/free1.txt",
             "free/disconnected/1reg",      "yes", 1, -1, "disconnected_free1.txt"},
            {"edge_cases/ranges/no_interference.txt",   "edge_cases/registers/free1.txt",
             "free/no_interference/1reg",   "yes", 1, -1, "no_interference_free1.txt"},
            {"edge_cases/ranges/same_line_def_use.txt", "edge_cases/registers/free1.txt",
             "free/same_line_def_use/1reg", "yes", 1, -1, "same_line_def_use_free1.txt"},
            {"edge_cases/ranges/dsatur_dense.txt",      "edge_cases/registers/free4.txt",
             "free/dsatur_dense/4regs",     "yes", 4, -1, "dsatur_dense_free4.txt"},
            {"edge_cases/ranges/dsatur_dense.txt",      "edge_cases/registers/free3.txt",
             "free/dsatur_dense/3regs",     "yes", 3, -1, "dsatur_dense_free3.txt"},
            {"edge_cases/ranges/spill_basic.txt",       "edge_cases/registers/free2.txt",
             "free/spill_basic/2regs",      "yes", 2, -1, "spill_basic_free2.txt"},
            {"edge_cases/ranges/spill_exceeds_k.txt",   "edge_cases/registers/free2.txt",
             "free/spill_exceeds/2regs",    "", -1, -1,   "spill_exceeds_k_free2.txt"},
            {"edge_cases/ranges/web_merge_chain.txt",   "edge_cases/registers/free2.txt",
             "free/web_merge_chain/2regs",  "", -1, -1,   "web_merge_chain_free2.txt"},
        }},
        { "hourglass", {
            {"hourglass/ranges/hourglass1.txt", "hourglass/registers/basic3.txt",
             "h1-basic3", "yes", -1, 3, "hourglass1_basic3.txt"},
            {"hourglass/ranges/hourglass1.txt", "hourglass/registers/free3.txt",
             "h1-free3",  "yes", -1, 3, "hourglass1_free3.txt"},
            {"hourglass/ranges/hourglass1.txt", "hourglass/registers/basic2.txt",
             "h1-basic2", "no",  -1,-1, "hourglass1_basic2.txt"},
            {"hourglass/ranges/hourglass2.txt", "hourglass/registers/basic4.txt",
             "h2-basic4", "yes", -1, 4, "hourglass2_basic4.txt"},
            {"hourglass/ranges/hourglass2.txt", "hourglass/registers/free4.txt",
             "h2-free4",  "yes", -1, 4, "hourglass2_free4.txt"},
            {"hourglass/ranges/hourglass2.txt", "hourglass/registers/basic3.txt",
             "h2-basic3", "no",  -1,-1, "hourglass2_basic3.txt"},
            {"hourglass/ranges/hourglass3.txt", "hourglass/registers/free3.txt",
             "h3-free3",  "yes", -1, 3, "hourglass3_free3.txt"},
            {"hourglass/ranges/hourglass4.txt", "hourglass/registers/free3.txt",
             "h4-free3",  "yes", -1, 3, "hourglass4_free3.txt"},
            {"hourglass/ranges/hourglass5.txt", "hourglass/registers/free4.txt",
             "h5-free4",  "yes", -1, 4, "hourglass5_free4.txt"},
            {"hourglass/ranges/hourglass5.txt", "hourglass/registers/basic4.txt",
             "h5-basic4", "yes", -1, 4, "hourglass5_basic4.txt"},
        }},
        { "stress", {
            {"stress/ranges/path10.txt",      "stress/registers/free2.txt",  "path10-free2",      "yes", -1, 2, "path10_free2.txt"},
            {"stress/ranges/ladder.txt",      "stress/registers/free2.txt",  "ladder-free2",      "yes",  2,-1, "ladder_free2.txt"},
            {"stress/ranges/star_k1_8.txt",   "stress/registers/free2.txt",  "star_k1_8-free2",   "yes",  2,-1, "star_k1_8_free2.txt"},
            {"stress/ranges/five_k3.txt",     "stress/registers/free3.txt",  "five_k3-free3",     "yes", -1, 3, "five_k3_free3.txt"},
            {"stress/ranges/deep_chain.txt",  "stress/registers/free3.txt",  "deep_chain-free3",  "yes", -1, 3, "deep_chain_free3.txt"},
            {"stress/ranges/wheel5.txt",      "stress/registers/free3.txt",  "wheel5-free3",      "yes", -1, 3, "wheel5_free3.txt"},
            {"stress/ranges/five_k3.txt",     "stress/registers/basic2.txt", "five_k3-basic2",    "no",  -1,-1, "five_k3_basic2.txt"},
            {"stress/ranges/k4_edge_share.txt",     "stress/registers/free4.txt",  "k4_edge_share-free4",     "yes", 4,-1, "k4_edge_share_free4.txt"},
            {"stress/ranges/triple_hourglass.txt",  "stress/registers/free4.txt",  "triple_hourglass-free4",  "yes",-1, 4, "triple_hourglass_free4.txt"},
            {"stress/ranges/asym_hourglass.txt",    "stress/registers/free4.txt",  "asym_hourglass-free4",    "yes",-1, 4, "asym_hourglass_free4.txt"},
            {"stress/ranges/conflict_pressure.txt", "stress/registers/free4.txt",  "conflict_pressure-free4", "yes",-1, 4, "conflict_pressure_free4.txt"},
            {"stress/ranges/k4_edge_share.txt",     "stress/registers/basic3.txt", "k4_edge_share-basic3",    "no", -1,-1, "k4_edge_share_basic3.txt"},
        }},
        { "adversarial", {
            // χ=2
            {"adversarial/ranges/large_bipartite.txt", "adversarial/registers/free2.txt",
             "large_bipartite-free2", "yes", 2,-1, "large_bipartite_free2.txt"},
            {"adversarial/ranges/six_k2.txt",          "adversarial/registers/free2.txt",
             "six_k2-free2",          "yes", 2,-1, "six_k2_free2.txt"},
            {"adversarial/ranges/six_k2.txt",          "adversarial/registers/free1.txt",
             "six_k2-free1",          "yes", 1,-1, "six_k2_free1.txt"},
            // χ=3
            {"adversarial/ranges/petersen.txt",     "adversarial/registers/free3.txt",
             "petersen-free3",     "yes", 3,-1, "petersen_free3.txt"},
            {"adversarial/ranges/petersen.txt",     "adversarial/registers/free2.txt",
             "petersen-free2",     "yes", 2,-1, "petersen_free2.txt"},
            {"adversarial/ranges/odd_cycle_c7.txt", "adversarial/registers/free3.txt",
             "odd_cycle_c7-free3", "yes",-1, 3, "odd_cycle_c7_free3.txt"},
            {"adversarial/ranges/odd_cycle_c9.txt", "adversarial/registers/free3.txt",
             "odd_cycle_c9-free3", "yes",-1, 3, "odd_cycle_c9_free3.txt"},
            {"adversarial/ranges/mycielski_c5.txt", "adversarial/registers/free3.txt",
             "mycielski_c5-free3", "yes", 3,-1, "mycielski_c5_free3.txt"},
            {"adversarial/ranges/zigzag.txt",       "adversarial/registers/free3.txt",
             "zigzag-free3",       "yes",-1, 3, "zigzag_free3.txt"},
            {"adversarial/ranges/fat_ap_star.txt",  "adversarial/registers/free3.txt",
             "fat_ap_star-free3",  "yes", 3,-1, "fat_ap_star_free3.txt"},
            {"adversarial/ranges/chain_20_k3.txt",  "adversarial/registers/free3.txt",
             "chain_20_k3-free3",  "yes",-1, 3, "chain_20_k3_free3.txt"},
            {"adversarial/ranges/pure_c21.txt",     "adversarial/registers/free3.txt",
             "pure_c21-free3",     "yes",-1, 3, "pure_c21_free3.txt"},
            // χ=4
            {"adversarial/ranges/grotzsch.txt",            "adversarial/registers/free4.txt",
             "grotzsch-free4",            "yes",-1, 4, "grotzsch_free4.txt"},
            {"adversarial/ranges/grotzsch.txt",            "adversarial/registers/free3.txt",
             "grotzsch-free3",            "yes", 3,-1, "grotzsch_free3.txt"},
            {"adversarial/ranges/k4_chain.txt",            "adversarial/registers/free4.txt",
             "k4_chain-free4",            "yes", 4,-1, "k4_chain_free4.txt"},
            {"adversarial/ranges/k4_chain_10.txt",         "adversarial/registers/free4.txt",
             "k4_chain_10-free4",         "yes", 4,-1, "k4_chain_10_free4.txt"},
            {"adversarial/ranges/ap_color_conflict.txt",   "adversarial/registers/free4.txt",
             "ap_color_conflict-free4",   "yes", 4,-1, "ap_color_conflict_free4.txt"},
            {"adversarial/ranges/double_ap_shared.txt",    "adversarial/registers/free4.txt",
             "double_ap_shared-free4",    "yes", 4,-1, "double_ap_shared_free4.txt"},
            {"adversarial/ranges/w_shape_4ap.txt",         "adversarial/registers/free4.txt",
             "w_shape_4ap-free4",         "yes", 4,-1, "w_shape_4ap_free4.txt"},
            {"adversarial/ranges/two_k4_shared_edge.txt",  "adversarial/registers/free4.txt",
             "two_k4_shared_edge-free4",  "yes", 4,-1, "two_k4_shared_edge_free4.txt"},
            {"adversarial/ranges/three_k4_bridges.txt",    "adversarial/registers/free4.txt",
             "three_k4_bridges-free4",    "yes", 4,-1, "three_k4_bridges_free4.txt"},
            {"adversarial/ranges/dense_sparse_mix.txt",    "adversarial/registers/free4.txt",
             "dense_sparse_mix-free4",    "yes", 4,-1, "dense_sparse_mix_free4.txt"},
            {"adversarial/ranges/dense_k4partite_100.txt", "adversarial/registers/free4.txt",
             "dense_k4partite-free4",     "yes", 4,-1, "dense_k4partite_100_free4.txt"},
            // χ=5
            {"adversarial/ranges/mycielski_m4.txt", "adversarial/registers/free5.txt",
             "mycielski_m4-free5", "yes", 5,-1, "mycielski_m4_free5.txt"},
            {"adversarial/ranges/mycielski_m4.txt", "adversarial/registers/free4.txt",
             "mycielski_m4-free4", "yes", 4,-1, "mycielski_m4_free4.txt"},
        }},
    };
}

} // anonymous namespace

// ── public API ────────────────────────────────────────────────────────────────

std::vector<std::string> BatchProcessor::runAllBasicDatasets() {
    // kept for any legacy callers; delegates to the new function (no file output)
    fs::path root = datasetRoot();
    auto sections = allSections();
    const auto &basicSpecs = sections[0].second;

    std::vector<std::string> output;
    for (const auto &tc : basicSpecs) {
        auto r = runSpec(root, tc);
        std::string line = tc.label + " => " + std::to_string(r.registersUsed) + " reg(s)";
        if (!r.failReason.empty()) line = "!!" + line + " [" + r.failReason + "]";
        output.push_back(line);
    }
    return output;
}

std::vector<std::string> BatchProcessor::runBasicDatasetsToFile() {
    fs::path root    = datasetRoot();
    fs::path outBase = outputsRoot() / "basic";

    std::error_code ec;
    fs::create_directories(outBase, ec);
    if (ec) return { "!!Could not create outputs/basic/: " + ec.message() };

    auto sections   = allSections();
    const auto &specs = sections[0].second; // "basic"

    std::vector<std::string> display;
    int pass = 0, fail = 0;

    // results.txt — same info shown in the interface
    std::ofstream results(outBase / "results.txt");
    if (!results.is_open())
        return { "!!Could not write outputs/basic/results.txt" };

    results << "=== basic — Results ===\n\n";

    for (const auto &tc : specs) {
        auto r = runSpec(root, tc);
        bool ok = r.failReason.empty();
        std::string status = ok ? "PASS" : "FAIL";

        // interface display line
        std::string dispLine = status + " [" + tc.label + "]";
        if (r.loaded) dispLine += " => " + std::to_string(r.registersUsed) + " reg(s)";
        if (!ok) dispLine += " (" + r.failReason + ")";
        display.push_back(dispLine);

        // results.txt entry
        results << status << " [" << tc.label << "]";
        if (r.loaded) results << " => " << r.registersUsed << " reg(s)";
        if (!ok) results << " (" << r.failReason << ")";
        results << "\n";

        // individual output file
        std::ofstream indiv(outBase / tc.outputFilename);
        if (indiv.is_open() && r.loaded)
            writeAllocBlock(indiv, r.alloc);

        if (ok) pass++; else fail++;
    }

    results << "\nResults: " << pass << " passed, " << fail
            << " failed out of " << (pass + fail) << " tests\n";

    display.push_back("");
    display.push_back("Results: " + std::to_string(pass) + " passed, " +
                      std::to_string(fail) + " failed out of " +
                      std::to_string(pass + fail) + " tests");
    display.push_back("Saved to: outputs/basic/");
    return display;
}

std::vector<std::string> BatchProcessor::runAllTestsToFile() {
    fs::path root    = datasetRoot();
    fs::path outBase = outputsRoot();

    std::error_code ec;
    fs::create_directories(outBase, ec);
    if (ec) return { "!!Could not create outputs/: " + ec.message() };

    std::ofstream report(outBase / "results_report.txt");
    if (!report.is_open())
        return { "!!Could not write outputs/results_report.txt" };

    report << "=== Full Dataset Results — Register Allocator ===\n\n";

    int totalPass = 0, totalFail = 0;

    for (const auto &[folderName, specs] : allSections()) {
        fs::path outDir = outBase / folderName;
        ec.clear();
        fs::create_directories(outDir, ec);

        std::ofstream folderResults(outDir / "results.txt");
        folderResults << "=== " << folderName << " ===\n\n";

        report << "=== " << folderName << " ===\n";

        int secPass = 0, secFail = 0;

        for (const auto &tc : specs) {
            auto r = runSpec(root, tc);
            bool ok = r.failReason.empty() && r.loaded;
            std::string status = ok ? "PASS" : "FAIL";
            std::string line = status + " [" + tc.label + "]";
            if (!r.loaded)            line += " — ERROR: could not load files";
            else if (!r.failReason.empty()) line += " — " + r.failReason;

            report        << "  " << line << "\n";
            folderResults << "  " << line << "\n";

            if (r.loaded) {
                std::ofstream out(outDir / tc.outputFilename);
                if (out.is_open()) writeAllocBlock(out, r.alloc);
            }

            if (ok) secPass++; else secFail++;
        }

        int secTotal = secPass + secFail;
        folderResults << "\n" << secPass << " out of " << secTotal << " passed\n";
        report << "  " << secPass << " out of " << secTotal << " passed\n\n";

        totalPass += secPass;
        totalFail += secFail;
    }

    int grand = totalPass + totalFail;
    report << "════════════════════════════════════════════════════\n";
    report << totalPass << " out of " << grand << " passed\n";

    std::vector<std::string> display;
    if (totalFail == 0) {
        display.push_back("All " + std::to_string(grand) + " tests passed");
    } else {
        display.push_back("!!" + std::to_string(totalFail) + " test(s) failed — see results_report.txt");
        display.push_back(std::to_string(totalPass) + " out of " + std::to_string(grand) + " passed");
    }
    display.push_back("Saved to: outputs/");
    return display;
}
