#!/usr/bin/env bash
# run_tests.sh — full regression suite for register_alloc
# Usage: bash tests/run_tests.sh  (from project root)

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BIN="$ROOT/code/cmake-build/register_alloc"
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

    timeout 10 "$BIN" -b "$ranges" "$regs" "$TMPOUT" 2>"$ERRTMP" || {
        failed=true; reason="binary returned non-zero / timeout"
    }

    local out=""
    [[ -f "$TMPOUT" ]] && out=$(cat "$TMPOUT")

    while [[ $# -ge 2 ]]; do
        local key=$1 val=$2; shift 2
        case "$key" in
            feasible)
                local err_warn=false
                grep -q "Warning: Register allocation was not possible" "$ERRTMP" 2>/dev/null && err_warn=true || true
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

# ── Professor basic tests ─────────────────────────────────────────────────────

echo "=== Basic (professor dataset) ==="
run "$ROOT"/dataset/basic/ranges/ranges1.txt "$ROOT"/dataset/basic/registers/registers2.txt "basic/r1-reg2" feasible yes regs_at_most 2
run "$ROOT"/dataset/basic/ranges/ranges2.txt "$ROOT"/dataset/basic/registers/registers2.txt "basic/r2-reg2" feasible yes regs_at_most 2
run "$ROOT"/dataset/basic/ranges/ranges3.txt "$ROOT"/dataset/basic/registers/registers2.txt "basic/r3-reg2" feasible yes regs_at_most 2
run "$ROOT"/dataset/basic/ranges/ranges4.txt "$ROOT"/dataset/basic/registers/registers1.txt "basic/r4-reg1" feasible yes regs_used 1
run "$ROOT"/dataset/basic/ranges/ranges5.txt "$ROOT"/dataset/basic/registers/registers1.txt "basic/r5-reg1" feasible yes regs_used 1
run "$ROOT"/dataset/basic/ranges/ranges6.txt "$ROOT"/dataset/basic/registers/registers3.txt "basic/r6-reg3" feasible yes regs_at_most 3

# ── Edge cases: basic ─────────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: basic ==="
run "$ROOT"/dataset/edge_cases/ranges/disconnected.txt      "$ROOT"/dataset/edge_cases/registers/basic1.txt "basic/disconnected"       feasible yes regs_used 1
run "$ROOT"/dataset/edge_cases/ranges/no_interference.txt   "$ROOT"/dataset/edge_cases/registers/basic1.txt "basic/no_interference"    feasible yes regs_used 1
run "$ROOT"/dataset/edge_cases/ranges/same_line_def_use.txt "$ROOT"/dataset/edge_cases/registers/basic1.txt "basic/same_line_def_use"  feasible yes regs_used 1
run "$ROOT"/dataset/edge_cases/ranges/dsatur_dense.txt      "$ROOT"/dataset/edge_cases/registers/basic4.txt "basic/dsatur_dense/4regs" feasible yes regs_used 4
run "$ROOT"/dataset/edge_cases/ranges/dsatur_dense.txt      "$ROOT"/dataset/edge_cases/registers/basic2.txt "basic/dsatur_dense/2regs" feasible no

# ── Edge cases: spilling ──────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: spilling ==="
run "$ROOT"/dataset/edge_cases/ranges/spill_basic.txt     "$ROOT"/dataset/edge_cases/registers/spill1.txt "spill/basic/K=1"   feasible yes
run "$ROOT"/dataset/edge_cases/ranges/spill_exceeds_k.txt "$ROOT"/dataset/edge_cases/registers/spill2.txt "spill/exceeds/K=1" feasible no
run "$ROOT"/dataset/edge_cases/ranges/spill_exceeds_k.txt "$ROOT"/dataset/edge_cases/registers/spill3.txt "spill/exceeds/K=2" feasible yes

# ── Edge cases: splitting ─────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: splitting ==="
run "$ROOT"/dataset/edge_cases/ranges/split_simple.txt       "$ROOT"/dataset/edge_cases/registers/split_k1.txt "split/simple/K=1"
run "$ROOT"/dataset/edge_cases/ranges/split_needed.txt       "$ROOT"/dataset/edge_cases/registers/split_k2.txt "split/needed/K=2"
run "$ROOT"/dataset/edge_cases/ranges/split_unsplittable.txt "$ROOT"/dataset/edge_cases/registers/split_k5.txt "split/unsplittable/K=5" feasible no

# ── Edge cases: dsatur ────────────────────────────────────────────────────────

echo ""
echo "=== Edge cases: dsatur ==="
run "$ROOT"/dataset/edge_cases/ranges/dsatur_dense.txt "$ROOT"/dataset/edge_cases/registers/dsatur1.txt "dsatur/dense/1reg"  feasible yes regs_used 1
run "$ROOT"/dataset/edge_cases/ranges/dsatur_dense.txt "$ROOT"/dataset/edge_cases/registers/dsatur3.txt "dsatur/dense/3regs" feasible yes regs_used 3
run "$ROOT"/dataset/edge_cases/ranges/dsatur_dense.txt "$ROOT"/dataset/edge_cases/registers/dsatur4.txt "dsatur/dense/4regs" feasible yes regs_used 4

# ── Edge cases: free (BCT-Color) ──────────────────────────────────────────────

echo ""
echo "=== Edge cases: free (partitioned + tree DP + backtracking) ==="
run "$ROOT"/dataset/edge_cases/ranges/disconnected.txt      "$ROOT"/dataset/edge_cases/registers/phantom1.txt "free/disconnected/1reg"      feasible yes regs_used 1
run "$ROOT"/dataset/edge_cases/ranges/no_interference.txt   "$ROOT"/dataset/edge_cases/registers/phantom1.txt "free/no_interference/1reg"   feasible yes regs_used 1
run "$ROOT"/dataset/edge_cases/ranges/same_line_def_use.txt "$ROOT"/dataset/edge_cases/registers/phantom1.txt "free/same_line_def_use/1reg" feasible yes regs_used 1
run "$ROOT"/dataset/edge_cases/ranges/dsatur_dense.txt      "$ROOT"/dataset/edge_cases/registers/phantom4.txt "free/dsatur_dense/4regs"     feasible yes regs_used 4
run "$ROOT"/dataset/edge_cases/ranges/dsatur_dense.txt      "$ROOT"/dataset/edge_cases/registers/phantom3.txt "free/dsatur_dense/3regs"     feasible yes regs_used 3
run "$ROOT"/dataset/edge_cases/ranges/spill_basic.txt       "$ROOT"/dataset/edge_cases/registers/phantom2.txt "free/spill_basic/2regs"      feasible yes regs_used 2
run "$ROOT"/dataset/edge_cases/ranges/spill_exceeds_k.txt   "$ROOT"/dataset/edge_cases/registers/phantom2.txt "free/spill_exceeds/2regs"
run "$ROOT"/dataset/edge_cases/ranges/web_merge_chain.txt   "$ROOT"/dataset/edge_cases/registers/phantom2.txt "free/web_merge_chain/2regs"

# ── Hourglass ─────────────────────────────────────────────────────────────────

echo ""
echo "=== Hourglass ==="
# K3+K3 via AP — feasible with 3 regs, infeasible with 2
run "$ROOT"/dataset/hourglass/ranges/hourglass1.txt "$ROOT"/dataset/hourglass/registers/basic3.txt "hourglass/h1-basic3"  feasible yes regs_at_most 3
run "$ROOT"/dataset/hourglass/ranges/hourglass1.txt "$ROOT"/dataset/hourglass/registers/phantom3.txt  "hourglass/h1-phantom3"   feasible yes regs_at_most 3
run "$ROOT"/dataset/hourglass/ranges/hourglass1.txt "$ROOT"/dataset/hourglass/registers/basic2.txt "hourglass/h1-basic2"  feasible no
# K4+K3 via AP — needs 4 regs
run "$ROOT"/dataset/hourglass/ranges/hourglass2.txt "$ROOT"/dataset/hourglass/registers/basic4.txt "hourglass/h2-basic4"  feasible yes regs_at_most 4
run "$ROOT"/dataset/hourglass/ranges/hourglass2.txt "$ROOT"/dataset/hourglass/registers/phantom4.txt  "hourglass/h2-phantom4"   feasible yes regs_at_most 4
run "$ROOT"/dataset/hourglass/ranges/hourglass2.txt "$ROOT"/dataset/hourglass/registers/basic3.txt "hourglass/h2-basic3"  feasible no
# K3+K3+K3 chain — 3 regs
run "$ROOT"/dataset/hourglass/ranges/hourglass3.txt "$ROOT"/dataset/hourglass/registers/phantom3.txt  "hourglass/h3-phantom3"   feasible yes regs_at_most 3
run "$ROOT"/dataset/hourglass/ranges/hourglass4.txt "$ROOT"/dataset/hourglass/registers/phantom3.txt  "hourglass/h4-phantom3"   feasible yes regs_at_most 3
# Deep chain — 4 regs
run "$ROOT"/dataset/hourglass/ranges/hourglass5.txt "$ROOT"/dataset/hourglass/registers/phantom4.txt  "hourglass/h5-phantom4"   feasible yes regs_at_most 4
run "$ROOT"/dataset/hourglass/ranges/hourglass5.txt "$ROOT"/dataset/hourglass/registers/basic4.txt "hourglass/h5-basic4"  feasible yes regs_at_most 4

# ── Stress ────────────────────────────────────────────────────────────────────

echo ""
echo "=== Stress ==="
# χ=2 cases — must color with 2 (path10 aliases to 1 register: no real interference)
run "$ROOT"/dataset/stress/ranges/path10.txt      "$ROOT"/dataset/stress/registers/phantom2.txt  "stress/path10-phantom2"      feasible yes regs_at_most 2
run "$ROOT"/dataset/stress/ranges/ladder.txt      "$ROOT"/dataset/stress/registers/phantom2.txt  "stress/ladder-phantom2"      feasible yes regs_used 2
run "$ROOT"/dataset/stress/ranges/star_k1_8.txt   "$ROOT"/dataset/stress/registers/phantom2.txt  "stress/star_k1_8-phantom2"   feasible yes regs_used 2
# χ=3 cases — feasible with 3, not with 2
run "$ROOT"/dataset/stress/ranges/five_k3.txt     "$ROOT"/dataset/stress/registers/phantom3.txt  "stress/five_k3-phantom3"     feasible yes regs_at_most 3
run "$ROOT"/dataset/stress/ranges/deep_chain.txt  "$ROOT"/dataset/stress/registers/phantom3.txt  "stress/deep_chain-phantom3"  feasible yes regs_at_most 3
run "$ROOT"/dataset/stress/ranges/wheel5.txt      "$ROOT"/dataset/stress/registers/phantom3.txt  "stress/wheel5-phantom3"      feasible yes regs_at_most 3
run "$ROOT"/dataset/stress/ranges/five_k3.txt     "$ROOT"/dataset/stress/registers/basic2.txt "stress/five_k3-basic2"    feasible no
# χ=4 cases — feasible with 4
run "$ROOT"/dataset/stress/ranges/k4_edge_share.txt     "$ROOT"/dataset/stress/registers/phantom4.txt  "stress/k4_edge_share-phantom4"     feasible yes regs_used 4
run "$ROOT"/dataset/stress/ranges/triple_hourglass.txt  "$ROOT"/dataset/stress/registers/phantom4.txt  "stress/triple_hourglass-phantom4"  feasible yes regs_at_most 4
run "$ROOT"/dataset/stress/ranges/asym_hourglass.txt    "$ROOT"/dataset/stress/registers/phantom4.txt  "stress/asym_hourglass-phantom4"    feasible yes regs_at_most 4
run "$ROOT"/dataset/stress/ranges/conflict_pressure.txt "$ROOT"/dataset/stress/registers/phantom4.txt  "stress/conflict_pressure-phantom4" feasible yes regs_at_most 4
run "$ROOT"/dataset/stress/ranges/k4_edge_share.txt     "$ROOT"/dataset/stress/registers/basic3.txt "stress/k4_edge_share-basic3"    feasible no

# ── Adversarial ───────────────────────────────────────────────────────────────

echo ""
echo "=== Adversarial ==="
# χ=2
run "$ROOT"/dataset/adversarial/ranges/large_bipartite.txt "$ROOT"/dataset/adversarial/registers/phantom2.txt  "adversarial/large_bipartite-phantom2"  feasible yes regs_used 2
run "$ROOT"/dataset/adversarial/ranges/six_k2.txt          "$ROOT"/dataset/adversarial/registers/phantom2.txt  "adversarial/six_k2-phantom2"           feasible yes regs_used 2
run "$ROOT"/dataset/adversarial/ranges/six_k2.txt          "$ROOT"/dataset/adversarial/registers/phantom1.txt  "adversarial/six_k2-phantom1"           feasible yes regs_used 1
# χ=3
run "$ROOT"/dataset/adversarial/ranges/petersen.txt        "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/petersen-phantom3"         feasible yes regs_used 3
run "$ROOT"/dataset/adversarial/ranges/petersen.txt        "$ROOT"/dataset/adversarial/registers/phantom2.txt  "adversarial/petersen-phantom2"         feasible yes regs_used 2
run "$ROOT"/dataset/adversarial/ranges/odd_cycle_c7.txt    "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/odd_cycle_c7-phantom3"     feasible yes regs_at_most 3
run "$ROOT"/dataset/adversarial/ranges/odd_cycle_c9.txt    "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/odd_cycle_c9-phantom3"     feasible yes regs_at_most 3
run "$ROOT"/dataset/adversarial/ranges/mycielski_c5.txt    "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/mycielski_c5-phantom3"     feasible yes regs_used 3
run "$ROOT"/dataset/adversarial/ranges/zigzag.txt          "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/zigzag-phantom3"           feasible yes regs_at_most 3
run "$ROOT"/dataset/adversarial/ranges/fat_ap_star.txt     "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/fat_ap_star-phantom3"      feasible yes regs_used 3
run "$ROOT"/dataset/adversarial/ranges/chain_20_k3.txt     "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/chain_20_k3-phantom3"      feasible yes regs_at_most 3
run "$ROOT"/dataset/adversarial/ranges/pure_c21.txt        "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/pure_c21-phantom3"         feasible yes regs_at_most 3
# χ=4
run "$ROOT"/dataset/adversarial/ranges/grotzsch.txt           "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/grotzsch-phantom4"           feasible yes regs_at_most 4
run "$ROOT"/dataset/adversarial/ranges/grotzsch.txt           "$ROOT"/dataset/adversarial/registers/phantom3.txt  "adversarial/grotzsch-phantom3"           feasible yes regs_used 3
run "$ROOT"/dataset/adversarial/ranges/k4_chain.txt           "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/k4_chain-phantom4"           feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/k4_chain_10.txt        "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/k4_chain_10-phantom4"        feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/ap_color_conflict.txt  "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/ap_color_conflict-phantom4"  feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/double_ap_shared.txt   "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/double_ap_shared-phantom4"   feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/w_shape_4ap.txt        "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/w_shape_4ap-phantom4"        feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/two_k4_shared_edge.txt "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/two_k4_shared_edge-phantom4" feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/three_k4_bridges.txt   "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/three_k4_bridges-phantom4"   feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/dense_sparse_mix.txt   "$ROOT"/dataset/adversarial/registers/phantom4.txt  "adversarial/dense_sparse_mix-phantom4"   feasible yes regs_used 4
run "$ROOT"/dataset/adversarial/ranges/dense_k4partite_100.txt "$ROOT"/dataset/adversarial/registers/phantom4.txt "adversarial/dense_k4partite-phantom4"   feasible yes regs_used 4
# χ=5
run "$ROOT"/dataset/adversarial/ranges/mycielski_m4.txt "$ROOT"/dataset/adversarial/registers/phantom5.txt "adversarial/mycielski_m4-phantom5" feasible yes regs_used 5
run "$ROOT"/dataset/adversarial/ranges/mycielski_m4.txt "$ROOT"/dataset/adversarial/registers/phantom4.txt "adversarial/mycielski_m4-phantom4" feasible yes regs_used 4

# ── Summary ───────────────────────────────────────────────────────────────────

echo ""
echo "────────────────────────────────"
echo "Results: $PASS passed, $FAIL failed out of $((PASS+FAIL)) tests"
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
