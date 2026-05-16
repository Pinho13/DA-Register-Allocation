#!/usr/bin/env bash
# run_tests.sh — full regression suite for register_alloc
# Usage: bash tests/run_tests.sh  (from project root)

set -euo pipefail

BIN="code/cmake-build/register_alloc"
TMPOUT=$(mktemp)
ERRTMP=$(mktemp)
trap "rm -f $TMPOUT $ERRTMP" EXIT

PASS=0; FAIL=0

# ── helpers ──────────────────────────────────────────────────────────────────

# run RANGES REGS LABEL [assertions...]
# assertions:
#   feasible yes|no      — yes: no M: lines; no: at least one M: line
#   regs_used INT        — exact match of "registers: N" line
#   regs_at_most INT     — "registers: N" where N <= val
run() {
    local ranges=$1 regs=$2 label=$3
    shift 3

    local failed=false reason=""

    if ! timeout 10 "$BIN" -b "$ranges" "$regs" "$TMPOUT" 2>"$ERRTMP"; then
        failed=true; reason="binary returned non-zero / timeout"
    fi

    local out=""
    [[ -f "$TMPOUT" ]] && out=$(cat "$TMPOUT")

    while [[ $# -ge 2 ]]; do
        local key=$1 val=$2; shift 2
        case "$key" in
            feasible)
                local err_warn=false
                grep -q "Warning: Register allocation was not possible" "$ERRTMP" && err_warn=true
                if [[ "$val" == "yes" ]] && $err_warn; then
                    failed=true; reason="expected feasible, got infeasible warning"
                elif [[ "$val" == "no" ]] && ! $err_warn; then
                    failed=true; reason="expected infeasible warning, got feasible"
                fi
                ;;
            regs_used)
                local got
                got=$(echo "$out" | grep -oE '^registers: [0-9]+' | head -1 | grep -oE '[0-9]+$')
                if [[ "$got" != "$val" ]]; then
                    failed=true; reason="expected registers=$val, got '${got:-<none>}'"
                fi
                ;;
            regs_at_most)
                local got
                got=$(echo "$out" | grep -oE '^registers: [0-9]+' | head -1 | grep -oE '[0-9]+$')
                if [[ -z "$got" ]] || [[ "$got" -gt "$val" ]]; then
                    failed=true; reason="expected registers≤$val, got '${got:-<none>}'"
                fi
                ;;
        esac
    done

    if $failed; then
        echo "FAIL [$label]: $reason"
        FAIL=$((FAIL+1))
    else
        echo "PASS [$label]"
        PASS=$((PASS+1))
    fi
}

# ── Professor basic tests ────────────────────────────────────────────────────

echo "=== Basic (professor dataset) ==="
run dataset/basic/ranges/ranges1.txt dataset/basic/registers/registers2.txt "basic/r1-reg2" feasible yes regs_at_most 2
run dataset/basic/ranges/ranges2.txt dataset/basic/registers/registers2.txt "basic/r2-reg2" feasible yes regs_at_most 2
run dataset/basic/ranges/ranges3.txt dataset/basic/registers/registers2.txt "basic/r3-reg2" feasible yes regs_at_most 2
run dataset/basic/ranges/ranges4.txt dataset/basic/registers/registers1.txt "basic/r4-reg1" feasible yes regs_used 1
run dataset/basic/ranges/ranges5.txt dataset/basic/registers/registers1.txt "basic/r5-reg1" feasible yes regs_used 1
run dataset/basic/ranges/ranges6.txt dataset/basic/registers/registers3.txt "basic/r6-reg3" feasible yes regs_at_most 3

# ── Edge cases: basic ─────────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: basic ==="
run dataset/edge_cases/ranges/disconnected.txt      dataset/edge_cases/registers/basic1.txt "basic/disconnected"      feasible yes regs_used 1
run dataset/edge_cases/ranges/no_interference.txt   dataset/edge_cases/registers/basic1.txt "basic/no_interference"   feasible yes regs_used 1
run dataset/edge_cases/ranges/same_line_def_use.txt dataset/edge_cases/registers/basic1.txt "basic/same_line_def_use" feasible yes regs_used 1
run dataset/edge_cases/ranges/dsatur_dense.txt      dataset/edge_cases/registers/basic4.txt "basic/dsatur_dense/4regs" feasible yes regs_used 4
run dataset/edge_cases/ranges/dsatur_dense.txt      dataset/edge_cases/registers/basic2.txt "basic/dsatur_dense/2regs" feasible no

# ── Edge cases: spilling ─────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: spilling ==="
run dataset/edge_cases/ranges/spill_basic.txt      dataset/edge_cases/registers/spill1.txt "spill/basic/K=1"   feasible yes
run dataset/edge_cases/ranges/spill_exceeds_k.txt  dataset/edge_cases/registers/spill2.txt "spill/exceeds/K=1" feasible no
run dataset/edge_cases/ranges/spill_exceeds_k.txt  dataset/edge_cases/registers/spill3.txt "spill/exceeds/K=2" feasible yes

# ── Edge cases: splitting ─────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: splitting ==="
run dataset/edge_cases/ranges/split_simple.txt       dataset/edge_cases/registers/split_k1.txt "split/simple/K=1"
run dataset/edge_cases/ranges/split_needed.txt       dataset/edge_cases/registers/split_k2.txt "split/needed/K=2"
run dataset/edge_cases/ranges/split_unsplittable.txt dataset/edge_cases/registers/split_k5.txt "split/unsplittable/K=5" feasible no

# ── Edge cases: dsatur ────────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: dsatur ==="
run dataset/edge_cases/ranges/dsatur_dense.txt dataset/edge_cases/registers/dsatur1.txt "dsatur/dense/1reg" feasible yes regs_used 1
run dataset/edge_cases/ranges/dsatur_dense.txt dataset/edge_cases/registers/dsatur3.txt "dsatur/dense/3regs" feasible yes regs_used 3
run dataset/edge_cases/ranges/dsatur_dense.txt dataset/edge_cases/registers/dsatur4.txt "dsatur/dense/4regs" feasible yes regs_used 4

# ── Edge cases: free (tree DP + backtracking) ─────────────────────────────────

echo ""
echo "=== Edge cases: free (partitioned + tree DP + backtracking) ==="
run dataset/edge_cases/ranges/disconnected.txt      dataset/edge_cases/registers/free1.txt "free/disconnected/1reg"      feasible yes regs_used 1
run dataset/edge_cases/ranges/no_interference.txt   dataset/edge_cases/registers/free1.txt "free/no_interference/1reg"   feasible yes regs_used 1
run dataset/edge_cases/ranges/same_line_def_use.txt dataset/edge_cases/registers/free1.txt "free/same_line_def_use/1reg" feasible yes regs_used 1
run dataset/edge_cases/ranges/dsatur_dense.txt      dataset/edge_cases/registers/free4.txt "free/dsatur_dense/4regs"     feasible yes regs_used 4
run dataset/edge_cases/ranges/dsatur_dense.txt      dataset/edge_cases/registers/free3.txt "free/dsatur_dense/3regs"     feasible yes regs_used 3
run dataset/edge_cases/ranges/spill_basic.txt       dataset/edge_cases/registers/free2.txt "free/spill_basic/2regs"      feasible yes regs_used 2
run dataset/edge_cases/ranges/spill_exceeds_k.txt   dataset/edge_cases/registers/free2.txt "free/spill_exceeds/2regs"
run dataset/edge_cases/ranges/web_merge_chain.txt   dataset/edge_cases/registers/free2.txt "free/web_merge_chain/2regs"

# ── Summary ───────────────────────────────────────────────────────────────────

echo ""
echo "────────────────────────────────"
echo "Results: $PASS passed, $FAIL failed out of $((PASS+FAIL)) tests"
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
