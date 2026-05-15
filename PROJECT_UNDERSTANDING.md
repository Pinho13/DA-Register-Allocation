# DA Project 2 — Compiler Register Allocation: Full Understanding & Plan

**Deadline:** May 17, 2026, at 23:59 (PT time)  
**Today:** May 14, 2026 — **3 days remaining**

---

## 1. What Is This Project?

You are building a **command-line tool** that solves the **register allocation problem** from compiler design. This is the phase of a compiler where it decides: *"which CPU register holds which variable, and when?"*

The reason this is hard: a modern CPU has a small, fixed number of registers (e.g., 2, 3, 4), but a program can have many variables. If two variables are needed *at the same time*, they cannot share a register. The problem of figuring out the minimum number of registers needed — and which variable gets which register — turns out to be equivalent to **graph coloring**, which is NP-complete.

---

## 2. Core Concepts (Read This Carefully)

### 2.1. Live Ranges
A **live range** for a variable is a sequence of program line numbers during which a value of that variable is "alive" — meaning it has been defined (written) and hasn't yet been used for the last time.

In the input file, each live range is expressed as a comma-separated list of line numbers:
- A line number with `+` means the variable is **defined** (written) here — this is where the live range *starts*.
- A line number with `-` means the variable is **last used** (read) here — this is where the live range *ends*.
- Line numbers without a suffix are "in between" — the variable is alive but neither freshly defined nor last used here.

Example from `ranges1.txt`:
```
sum: 7+,8,9,10-      # sum is written at line 7, still alive at 8,9, last used at 10
i: 1+,2,3,4,5,6-     # i is written at line 1, last used at line 6
i: 9+,10,11,12-      # i is written again at line 9, last used at line 12
i: 12+,13,14-        # i is written again at line 12, last used at line 14
i: 20+,11,12-        # i is written at line 20, last used at line 12 (loop back!)
```

### 2.2. Webs
A **web** is a union of live ranges for the same variable that *overlap* at any program point. Two live ranges of the same variable are merged into the same web if they share at least one line number.

- If range A covers lines {1,2,3,4,5,6} and range B covers lines {6,7,8}, they share line 6 → they belong to the **same web**.
- If range A covers {1,2,3} and range B covers {9,10,11} with no overlap → they are **different webs** (the variable gets two separate register slots).

**Special fusion rule:** If a live range ends at line `N-` AND another live range of the same variable starts at line `N+` (same line number), they must be merged because that line is of the form `i = i + 1` — a read-then-write on the same line.

A web is essentially the unit that gets assigned a register. Not variables — webs.

### 2.3. Interference Graph
Two webs **interfere** if they are simultaneously alive at any program point.

**Subtle rule:** Web A ends at line N due to a *use* (`N-`) and Web B starts at line N due to a *definition* (`N+`) at the same instruction → they do **NOT** interfere (B's value doesn't exist yet when A is last read).

Build a graph where:
- Each node = one web
- Each edge = the two webs interfere (they are alive at the same time)

### 2.4. Graph Coloring = Register Allocation
- Each **color** = one physical register (r0, r1, r2, ...)
- Two connected nodes (interfering webs) **cannot** have the same color
- Goal: color the graph using **at most N colors** (N = number of registers provided)
- If impossible, we either **spill** (assign to memory) or **split** webs

---

## 3. The Greedy Coloring Algorithm (T2.1 — Basic)

```
greedyGraphColoringAlgorithm(Graph G, int N):
  // Phase 1: Simplification
  while G is not empty:
    for each node n in G:
      if degree(n) < N:
        remove n from G
        push n onto stack S
    if all remaining nodes have degree >= N:
      select one node K to "spill" (no register assigned)
      remove K from G

  // Phase 2: Coloring (assign registers)
  while stack is not empty:
    n = pop from S
    assign n a color different from all its neighbors (a valid color always exists)

  return N and color assignment
```

To find the **minimum N**: try N = 1, 2, 3, ... up to max_degree+1 until the graph can be colored without spilling anything (or with acceptable spilling per task variant).

---

## 4. Input Format

### Live Ranges File (e.g., `ranges1.txt`)
```
# comment lines start with #
variable_name: line+,line,line,line-
variable_name: line+,line-
```

### Registers File (e.g., `registers2.txt`)
```
# comment
registers: N          # number of physical registers available
algorithm: basic      # or: spilling, K  or  splitting, K  or  free
```

---

## 5. Output Format

```
# program points in each web are sorted in ascending order
webs: 3
web0: 1+,2,3,4,5,6-
web1: 9+,10,11,12,13,14-,20+
web2: 7+,8,9,10-
# register assignment
registers: 2
r0: web0
r0: web1
r1: web2
```

If allocation is **impossible** (cannot color with N registers):
```
webs: 3
web0: ...
web1: ...
web2: ...
registers: 0
M: web0
M: web1
M: web2
```
And print a **console warning**: "Register allocation with N registers was not possible."

---

## 6. The Four Algorithm Tasks

| Task | Points | Algorithm keyword | Description |
|------|--------|-------------------|-------------|
| T2.1 | 4.0 | `basic` | Basic greedy coloring. If can't color → report failure, assign all to M. |
| T2.2 | 3.0 | `spilling, K` | Spill up to K webs to memory to make coloring possible. Choose which webs to spill wisely (e.g., spill high-degree nodes first). |
| T2.3 | 3.0 | `splitting, K` | Split up to K webs into two parts at a chosen boundary, reducing interference. |
| T2.4 | 4.0 | `free` | Your own algorithm — any approach, any heuristic, just: interfering webs cannot share a register. |

---

## 7. Input Examples Walkthrough

### ranges1.txt + registers2.txt (N=2)
Variable `i` has 4 live ranges. They share line numbers (12 appears in two ranges), so they merge into webs. The key is to figure out which ranges merge and which stay separate.

### ranges2.txt (x has many overlapping ranges → single web for x)
`x: 1+,2,3,4,7` and `x: 7,8` share line 7 → merge.  
Merged `x: 1+,2,3,4,7,8` and `x: 8,9-` share line 8 → merge further.  
Merged `x: 1+,...,9-` and `x: 8,11,12,13-` share line 8 → all into one web.  
→ x has **one web** covering {1,2,3,4,7,8,9,11,12,13}.

---

## 8. What You Must Build — Technical Requirements

### Language
No language is specified — use **C++** (most DA projects at FEUP use C++, and the graph structure referenced in the project is the standard FEUP DA graph).

### Data Structures
- Must use the **graph structure from TP lectures** as the primary interference graph representation.
- May add auxiliary structures for: web storage, live range merging, coloring state.

### Interface
Two modes:
1. **Interactive menu** — user can load files, run algorithms, view results
2. **Batch mode**: `./myProg -b ranges.txt registers.txt allocation.txt`

### Documentation
- **Doxygen** comments on all functions
- **Time complexity analysis** for each major algorithm

---

## 9. My Thought Process — Understanding the Hard Parts

### Hard Part 1: Web Merging
The live range fusion rule is the trickiest part. Algorithm:
1. For each variable, collect all its live ranges as sets of line numbers.
2. Use a greedy union-find or repeated merging: if two ranges share any line number, merge them.
3. Special case: if range A ends at `N-` and range B starts at `N+`, merge them (same-line def-use).
4. Repeat until no more merges are possible.

### Hard Part 2: Interference Detection
Two webs interfere if their sets of line numbers have any overlap, **except** the case where one ends with `N-` and the other starts with `N+` at the same line. In that case: no interference.

### Hard Part 3: The Coloring Algorithm
The algorithm described is a **Chaitin-style** simplification. Key implementation note: don't physically delete nodes. Instead, maintain a "removed" flag and recompute effective degrees.

### Hard Part 4: Splitting (T2.3)
Splitting a web means: find a line in the middle of the web, create two new webs (before and after that point). The split must happen at a point where the web doesn't interfere with as many others. Good heuristic: split at the line that minimizes resulting interference.

---

## 10. Grading Breakdown

| Task | Description | Points |
|------|-------------|--------|
| T1.1 | Command-line menu + batch mode | 1.0 |
| T1.2 | File parsing + data structures | 1.0 |
| T1.3 | Doxygen docs + complexity analysis | 2.0 |
| T2.1 | Basic greedy graph coloring | 4.0 |
| T2.2 | Spilling (up to K webs to memory) | 3.0 |
| T2.3 | Splitting (up to K webs in two) | 3.0 |
| T2.4 | Your own algorithm | 4.0 |
| T3.1 | 10-min demo + PowerPoint | 2.0 |
| **Total** | | **20.0** |

---

## 11. Implementation Plan (3-Day Sprint)

### Day 1 — Thursday May 14 (TODAY)
- [ ] Set up C++ project structure with CMake
- [ ] Implement file parsers (ranges file + registers file)
- [ ] Implement live range → web merging (union-find approach)
- [ ] Implement interference graph construction
- [ ] Write basic data structures (Web class, InterferenceGraph wrapping the TP graph)

### Day 2 — Friday May 15
- [ ] Implement T2.1: basic greedy graph coloring algorithm
- [ ] Implement output writer (both success and failure format)
- [ ] Implement command-line menu (T1.1)
- [ ] Implement batch mode: `./prog -b ranges.txt registers.txt out.txt`
- [ ] Test all 6 basic test cases from `basic/` folder
- [ ] Implement T2.2: spilling (choose high-degree nodes to spill)

### Day 3 — Saturday May 16
- [ ] Implement T2.3: web splitting
- [ ] Implement T2.4: free algorithm (e.g., DSATUR heuristic — color node with most constraints first)
- [ ] Add Doxygen comments throughout + complexity analysis
- [ ] Prepare PowerPoint for demo
- [ ] Final testing + edge cases

### Submission — Sunday May 17 by 23:59
- [ ] Zip as `DA2026_PRJ2_T<TN>_G<GN>.zip`
- [ ] Include: `code/`, `documentation/` (Doxygen HTML), `presentation.pdf`

---

## 12. Open Questions (Clarification Needed Before Coding)

1. **Language**: Is the project in C++? The description mentions "the graph structure from TP lectures" — do you have that graph class already?
2. **Group**: Are you working alone or with teammates?
3. **Algorithm priority**: Given 3 days, T2.1 + T2.2 are the safest to prioritize (7 points). T2.3 and T2.4 are harder. Do you want to aim for full marks or focus on correctness of the basics first?
