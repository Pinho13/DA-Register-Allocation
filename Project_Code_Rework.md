```bash
Act as a senior compiler engineer and C++17 systems architect.

You are improving a register allocation project written in modern C++.

Your job is NOT to rewrite the project randomly.
Your job is to incrementally improve correctness, architecture, heuristics, maintainability, and testability while preserving the existing CLI and overall project structure.

You must think like a production-grade reviewer:
- identify architectural weaknesses
- detect hidden bugs
- improve algorithms carefully
- avoid unnecessary abstractions
- preserve explainability for an academic/compiler project

The project is a register allocator pipeline:

ranges.txt
→ RangesParser
→ live ranges
→ WebBuilder
→ merged webs
→ InterferenceGraph
→ graph coloring allocator
→ OutputWriter

Supported modes:
- basic
- spilling,K
- splitting,K
- free (DSATUR)

The project uses C++17 and CMake.

==================================================
GLOBAL RULES
==================================================

1. DO NOT rewrite the entire project.
2. DO NOT introduce overengineering.
3. Preserve the CLI behavior.
4. Preserve compatibility with current input/output files.
5. Prefer incremental commits/refactors.
6. Explain WHY every improvement matters.
7. Every algorithmic change must:
   - explain complexity
   - explain tradeoffs
   - explain expected behavior changes
8. Add tests whenever you fix bugs.
9. Prioritize correctness before optimization.
10. Preserve readability and educational value.

==================================================
PHASE 1 — FULL CODEBASE ANALYSIS
==================================================

First:

1. Analyze the entire architecture.
2. Explain:
   - core pipeline
   - responsibilities of each module
   - data flow
   - ownership/lifetime assumptions
   - graph representation
   - allocator flow
3. Identify:
   - code smells
   - duplicated logic
   - weak abstractions
   - unsafe assumptions
   - algorithmic bottlenecks
   - potential hidden bugs
4. Produce:
   - architecture summary
   - dependency map
   - prioritized improvement list

DO NOT MODIFY CODE YET.

==================================================
PHASE 2 — BUILD SYSTEM + REPOSITORY CLEANUP
==================================================

Goals:
- reproducible clean builds
- remove machine-specific artifacts
- improve repository hygiene

Tasks:
1. Remove generated build folders from git:
   - build/
   - cmake-build-*/
   - binaries
   - .DS_Store

2. Add proper `.gitignore`.

3. Verify clean build:

cmake -S . -B build
cmake --build build
Ensure project builds from scratch with no IDE dependency.

Deliver:

updated .gitignore
explanation of cleanup
==================================================
PHASE 3 — FIX OUTPUT BUGS

Investigate OutputWriter carefully.

Known issue:
If a range both starts and ends on the same line, output formatting may lose one marker because the implementation checks "+" before "-".

Tasks:

Find the exact faulty logic.
Redesign event representation if needed.
Ensure multiple events on the same line are preserved.
Preserve expected assignment formatting.

Add regression tests.

Acceptance:

same-line start/end handled correctly
no existing formatting regressions
==================================================
PHASE 4 — VALIDATION LAYER

Add a robust allocation validator.

Create functionality like:

bool validateAllocation(
    const InterferenceGraph& graph,
    const AllocationResult& result
);

Validator must check:

no interfering nodes share register
all non-spilled webs have registers
spilled webs marked correctly
split intervals remain valid
graph consistency assumptions hold

Add:

assertions
debug diagnostics
validation tests

Goal:
Catch invalid allocator states immediately.

==================================================
PHASE 5 — REFACTOR ALLOCATION RESULT MODEL

Introduce clearer allocator result structures.

Replace scattered maps/flags with something like:

struct AllocationResult {
    bool success;
    std::unordered_map<int, std::string> assignments;
    std::unordered_set<int> spilled;
    std::vector<SplitInfo> splits;
};

Goals:

centralized allocation state
easier debugging
cleaner algorithms
simpler validation

Do NOT overengineer.

==================================================
PHASE 6 — IMPROVE BASIC GRAPH COLORING

Review the basic allocator.

Tasks:

Validate simplification stack logic.
Ensure:
push/pop correctness
proper degree updates
correct color assignment
Improve tie-breaking heuristics if useful.
Remove duplicated logic.

Explain:

complexity
Chaitin-style behavior
heuristic limitations

Add tests:

colorable graphs
non-colorable graphs
disconnected graphs
dense interference graphs
==================================================
PHASE 7 — REAL SPILLING HEURISTIC

Current spilling mode is weak.

Implement a real spill heuristic.

Requirements:

Detect coloring failure properly.
Select spill candidates intelligently.

Possible heuristics:

highest degree
longest live range
interference pressure
cost/benefit scoring

Recommended:

spillCost = liveRangeLength / (degree + 1)

or

score = degree * liveRangeLength

Choose ONE and justify it.

Algorithm:

choose spill candidate
temporarily remove it
recolor graph
mark spilled
repeat until:
success
spills exceed K

Add:

diagnostics
test cases
before/after examples
==================================================
PHASE 8 — IMPROVE SPLITTING

Current midpoint splitting is simplistic.

Improve split selection.

Goal:
Reduce interference meaningfully.

Possible strategy:

simulate candidate split points
estimate resulting interference
choose best split

Constraints:

no invalid intervals
no empty splits
respect K limit

Add tests:

useful split
impossible split
split still fails coloring

Explain:

heuristic quality
complexity tradeoffs
==================================================
PHASE 9 — STRENGTHEN DSATUR MODE

Review free/DSATUR mode.

Ensure correct DSATUR behavior:

choose highest saturation node
tie-break by degree
assign lowest available color
create new color only when necessary

Validate:

correctness
saturation updates
deterministic behavior if possible

Add tests:

sparse graph
dense graph
complete graph
disconnected graph

Explain:

why DSATUR often outperforms naive greedy coloring
==================================================
PHASE 10 — INTERFERENCE GRAPH IMPROVEMENTS

Review graph representation.

Improve:

adjacency management
duplicate edge prevention
graph traversal clarity
const correctness
ownership semantics

Possible improvements:

unordered_set adjacency
utility APIs
graph validation

DO NOT overabstract.

==================================================
PHASE 11 — WEB BUILDING IMPROVEMENTS

Review WebBuilder carefully.

Check:

overlap merging correctness
edge cases
adjacent intervals
same-variable handling
split interactions

Add tests:

overlapping ranges
nested ranges
adjacent ranges
isolated ranges

Explain assumptions.

==================================================
PHASE 12 — PARSER HARDENING

Improve parser robustness.

Handle:

malformed lines
invalid ranges
negative values
empty input
duplicated declarations
whitespace issues

Add useful diagnostics.

Avoid crashing on invalid input.

==================================================
PHASE 13 — TEST INFRASTRUCTURE

If no framework exists:

add Catch2 or lightweight assert tests

Create organized tests for:

parser
web builder
graph
allocator
spilling
splitting
DSATUR
output formatting
validation

Goal:
Make regressions easy to detect.

==================================================
PHASE 14 — PERFORMANCE REVIEW

Profile major bottlenecks.

Analyze:

graph construction complexity
coloring complexity
splitting cost
copying overhead
unnecessary allocations

Optimize ONLY where justified.

Prefer:

algorithmic improvements
avoiding needless copies
over micro-optimizations.
==================================================
PHASE 15 — CODE QUALITY

Improve:

naming
const correctness
function responsibilities
comments
documentation
dead code
duplicated logic

Possible improvements:

enum class AllocationMode
helper utilities
centralized allocator utilities

Avoid:

giant inheritance hierarchies
unnecessary templates
enterprise-style abstraction
==================================================
PHASE 16 — FINAL VALIDATION

Run:

cmake -S . -B build
cmake --build build

Then test:

./register_alloc -b ranges.txt registers.txt output.txt

Also create manual edge-case scenarios:

no interference
full interference
forced spill
split-needed graph
disconnected graph
same-line ranges
==================================================
FINAL DELIVERABLES

At the end provide:

Full architecture review
List of bugs fixed
List of algorithmic improvements
Complexity analysis
Files changed
Tests added
Before/after behavior examples
Remaining limitations
Suggested future improvements

For every major change:

explain WHY
explain tradeoffs
explain complexity impact
explain behavioral impact

DO NOT produce shallow explanations.
DO NOT overengineer.
DO NOT rewrite the project unnecessarily.

Think like a compiler engineer improving a real allocator incrementally.