# Register Allocation Tool

A command-line tool that performs register allocation for compiler intermediate representations using graph-coloring algorithms. Given a set of live ranges and a register configuration, the tool builds webs, constructs an interference graph, and assigns physical registers (or spills to memory) via one of four selectable strategies.

> This project was developed by <a href="https://github.com/T0m2sT">Pedro Tomás Teixeira</a> (up202404987) and <a href="https://github.com/Pinho13">Rafael Pinho e Silva</a> (up202406334).

![Grade](https://img.shields.io/badge/Grade-19.5%2F20-1E90FF?style=for-the-badge&labelColor=21262d)
![Course](https://img.shields.io/badge/Course-DA-1E90FF?style=for-the-badge&labelColor=21262d)
![Year](https://img.shields.io/badge/Year-2025%2F26-1E90FF?style=for-the-badge&labelColor=21262d)

## Build & Run

**Requirements:** C++17 compiler, CMake 3.17+

### Interactive Mode

```bash
./run.sh
```

Arrow-key menu with 10 options:
1. Load ranges file
2. Load registers file
3. Show live ranges
4. Build webs & show interference graph
5. Run register allocation
6. Show allocation result
7. Save result to file
8. Run basic datasets
9. Run all datasets
10. Exit

### Batch Mode

```bash
# Full paths
./run.sh -b <ranges_full_path> <registers_full_path> <output_full_path>

# Short form — looks up dataset/<folder>/, writes to outputs/<folder>/<stem>.txt
./run.sh -b <dataset_type> <ranges_filename> <registers_filename> <output_filename>
```

Example:
```bash
./run.sh -b basic ranges1 registers2 result1
```


## Architecture

```
Ranges file --> RangesParser --> LiveRange[] --> WebBuilder --> Web[] --> InterferenceGraph --> RegisterAllocator --> AllocationResult --> OutputWriter
                                                                                                      ^
                                                                                              RegistersParser <-- Registers file
```

| Layer | Files | Purpose |
|-------|-------|---------|
| Data Models | `AllocationData.h` | LiveRange, Web, RegisterConfig, AllocationResult |
| Parsing | `RangesParser.h/.cpp`, `RegistersParser.h/.cpp` | Live-range and register config file parsing |
| Graph | `WebBuilder.h/.cpp`, `InterferenceGraph.h/.cpp` | Web construction and interference graph |
| Algorithm | `RegisterAllocator.h/.cpp`, `Coloring_*.cpp` | Dispatch + four coloring strategies |
| I/O | `OutputWriter.h/.cpp`, `BatchProcessor.h/.cpp` | Result writing and batch dataset execution |
| UI | `Menu.h/.cpp`, `DisplayFormatter.h/.cpp` | ANSI terminal UI with arrow-key navigation |


## Algorithms

Four strategies are selectable via the registers file (`algorithm` field):

| Keyword | Strategy | Description |
|---------|----------|-------------|
| `basic` | Chaitin simplification | Iterative degree-based simplification + stack coloring. O(W²). |
| `spilling` | Basic + spill | Same as basic; webs that cannot be colored are spilled to memory. |
| `splitting` | Basic + split | Splits high-cost webs before retrying coloring to reduce interference. |
| `phantom` | BCT-Color | BCC decomposition → block-cut tree DP → per-block exact coloring + Kempe-chain reduction. Never worse than DSATUR. |

### Phantom (BCT-Color)

> To explore alternatives beyond the required algorithms, `Phantom` was designed by us as a novel hybrid register-allocation heuristic.

The `Phantom` algorithm decomposes the interference graph into biconnected components (BCCs) via Tarjan's DFS, builds a block-cut tree, and solves each block independently with an exact backtracking solver (cutoff ≤ 32 nodes) or DSATUR. A DSATUR floor guarantee ensures Phantom never spills more than plain DSATUR.

Post-processing steps:
- **Kempe-chain swaps** — eliminate the highest color by flipping two-color chains
- **Color reduction** — iteratively lower the color count via `reduceColors`
- **Color normalization** — remap to a compact 0..K-1 range


## Register File Format

```
numRegisters <N>
algorithm <basic|spilling|splitting|phantom>
```

For spilling/splitting, an optional parameter K controls the iteration limit:
```
numRegisters 3
algorithm spilling
K 5
```


## Output Format

```
web0 a [lines: 1 2 3] -> register 0
web1 b [lines: 2 3 4] -> register 1
web2 a [lines: 5 6]   -> register 0
web3 c [lines: 4 5 6] -> spilled
```

Batch output is written to `outputs/<folder>/`:
- `results.txt` — PASS/FAIL per test case + summary count
- One `.txt` file per test case with the full allocation block

A global `outputs/results_report.txt` is written when running all datasets.


## Project Structure

```
.
├── code
│   ├── CMakeLists.txt
│   ├── include
│   │   ├── core
│   │   │   ├── InterferenceGraph.h
│   │   │   ├── RegisterAllocator.h
│   │   │   └── WebBuilder.h
│   │   ├── io
│   │   │   ├── BatchProcessor.h
│   │   │   └── OutputWriter.h
│   │   ├── models
│   │   │   └── AllocationData.h
│   │   ├── parser
│   │   │   ├── RangesParser.h
│   │   │   └── RegistersParser.h
│   │   └── ui
│   │       ├── DisplayFormatter.h
│   │       └── Menu.h
│   └── src
│       ├── core
│       │   ├── Coloring_Basic.cpp
│       │   ├── Coloring_BCT.cpp
│       │   ├── Coloring_Dsatur.cpp
│       │   ├── Coloring_Utils.cpp
│       │   ├── InterferenceGraph.cpp
│       │   ├── RegisterAllocator.cpp
│       │   └── WebBuilder.cpp
│       ├── io
│       │   ├── BatchProcessor.cpp
│       │   └── OutputWriter.cpp
│       ├── parser
│       │   ├── RangesParser.cpp
│       │   └── RegistersParser.cpp
│       ├── ui
│       │   ├── DisplayFormatter.cpp
│       │   └── Menu.cpp
│       └── main.cpp
├── dataset
│   ├── adversarial/
│   ├── basic/
│   ├── edge_cases/
│   ├── hourglass/
│   └── stress/
├── description
│   └── Project2Description_rc6.pdf
├── docs
│   └── html/
├── outputs/
├── tests
│   └── run_tests.sh
└── run.sh
```


## Testing

```bash
bash tests/run_tests.sh
```

75 test cases across 5 dataset categories (basic, edge\_cases, hourglass, stress, adversarial). Each test asserts feasibility, register count, or an upper-bound constraint.

```
[PASS] basic/ranges1_registers2
[PASS] edge_cases/chain10-free1
...
75/75 passed
```


### Self-Evaluation

| Name | Contribution |
| :---: | :---: |
| Pedro Tomás Teixeira | 50% |
| Rafael Pinho e Silva | 50% |
