#!/usr/bin/env bash
# benchmark.sh — algorithm comparison across all datasets
# Usage: bash tests/benchmark.sh [--verbose]  (from project root)
#
# Algorithms compared:
#   basic    — Chaitin-style greedy simplification
#   dsatur   — plain global DSATUR (single pass, highest-degree tie-break)
#   BCT-Color (free) — biconnected decomposition + best-of-5 DSATUR + exact blocks + cost DP

set -uo pipefail

BIN="code/cmake-build/register_alloc"
VERBOSE=false
[[ "${1:-}" == "--verbose" ]] && VERBOSE=true

TMPOUT=$(mktemp)
TMPREG=$(mktemp)
TMPERR=$(mktemp)
trap "rm -f $TMPOUT $TMPREG $TMPERR" EXIT

# ── helpers ──────────────────────────────────────────────────────────────────

run_algo() {
    local ranges=$1 N=$2 algo=$3

    printf 'registers: %d\nalgorithm: %s\n' "$N" "$algo" > "$TMPREG"

    local t0 t1
    t0=$(date +%s%N)
    timeout 5 "$BIN" -b "$ranges" "$TMPREG" "$TMPOUT" 2>"$TMPERR" || true
    t1=$(date +%s%N)
    g_ms=$(( (t1 - t0) / 1000000 ))

    local out=""
    [[ -f "$TMPOUT" ]] && out=$(cat "$TMPOUT")

    g_spills=$(echo "$out" | grep -c '^M:' || true)
    g_regs=$(echo "$out" | grep -oE '^registers: [0-9]+' | head -1 | grep -oE '[0-9]+$' || true)
    g_regs=${g_regs:-0}

    if grep -q "Warning: Register allocation was not possible" "$TMPERR" 2>/dev/null; then
        g_feasible="no"
    else
        g_feasible="yes"
    fi
}

# ── collect ranges files ──────────────────────────────────────────────────────

mapfile -t RANGES < <(find dataset -name 'ranges*.txt' -o -name '*.txt' -path '*/ranges/*' | sort)

# ── per-algorithm totals ──────────────────────────────────────────────────────

declare -A TOT_SPILLS TOT_REGS TOT_MS TOT_RUNS
for a in basic dsatur free; do
    TOT_SPILLS[$a]=0; TOT_REGS[$a]=0; TOT_MS[$a]=0; TOT_RUNS[$a]=0
done

bct_sp_better=0 bct_sp_worse=0 bct_sp_same=0
bct_rg_better=0 bct_rg_worse=0 bct_rg_same=0

echo "════════════════════════════════════════════════════════════════════════"
echo "  Register Allocator Benchmark"
echo "  DSATUR = plain global DSATUR   BCT-Color = BCC decomposition + enhancements"
echo "════════════════════════════════════════════════════════════════════════"
echo ""

if $VERBOSE; then
    printf "%-38s %3s  %6s %5s  %8s %5s  %8s %5s\n" \
        "File" "N" "BASIC" "sp" "DSATUR" "sp" "BCT-Color" "sp"
    printf "%-38s %3s  %6s %5s  %8s %5s  %8s %5s\n" \
        "──────────────────────────────────────" "───" \
        "──────" "──" "────────" "──" "─────────" "──"
fi

for ranges in "${RANGES[@]}"; do
    for N in 1 2 3 4 5 6; do
        declare -A sp=() regs=() ms=() feas=()

        for algo in basic dsatur free; do
            run_algo "$ranges" "$N" "$algo"
            sp[$algo]=$g_spills
            regs[$algo]=$g_regs
            ms[$algo]=$g_ms
            feas[$algo]=$g_feasible

            TOT_SPILLS[$algo]=$(( TOT_SPILLS[$algo] + g_spills ))
            TOT_REGS[$algo]=$(( TOT_REGS[$algo] + g_regs ))
            TOT_MS[$algo]=$(( TOT_MS[$algo] + g_ms ))
            TOT_RUNS[$algo]=$(( TOT_RUNS[$algo] + 1 ))
        done

        if (( sp[free] < sp[dsatur] )); then
            bct_sp_better=$(( bct_sp_better + 1 ))
        elif (( sp[free] > sp[dsatur] )); then
            bct_sp_worse=$(( bct_sp_worse + 1 ))
        else
            bct_sp_same=$(( bct_sp_same + 1 ))
        fi

        if [[ "${feas[free]}" == "yes" && "${feas[dsatur]}" == "yes" ]]; then
            if (( regs[free] < regs[dsatur] )); then
                bct_rg_better=$(( bct_rg_better + 1 ))
            elif (( regs[free] > regs[dsatur] )); then
                bct_rg_worse=$(( bct_rg_worse + 1 ))
            else
                bct_rg_same=$(( bct_rg_same + 1 ))
            fi
        fi

        if $VERBOSE; then
            label=$(basename "$ranges" .txt)
            printf "%-38s %3d  %6s %5d  %8s %5d  %8s %5d\n" \
                "${label:0:38}" "$N" \
                "${feas[basic]}"  "${sp[basic]}" \
                "${feas[dsatur]}" "${sp[dsatur]}" \
                "${feas[free]}"   "${sp[free]}"
        fi
    done
done

total_runs=${TOT_RUNS[basic]}

echo "════════════════════════════════════════════════════════════════════════"
echo "  Totals across $total_runs (ranges × N) combinations"
echo "════════════════════════════════════════════════════════════════════════"
printf "  %-12s  %8s  %8s  %8s\n" "Algorithm" "Spills" "Regs" "Time(ms)"
printf "  %-12s  %8s  %8s  %8s\n" "────────────" "──────" "──────" "────────"
for a in basic dsatur free; do
    label="$a"; [[ "$a" == "free" ]] && label="BCT-Color"
    printf "  %-12s  %8d  %8d  %8d\n" "$label" "${TOT_SPILLS[$a]}" "${TOT_REGS[$a]}" "${TOT_MS[$a]}"
done

echo ""
echo "════════════════════════════════════════════════════════════════════════"
echo "  BCT-Color vs plain DSATUR head-to-head"
echo "════════════════════════════════════════════════════════════════════════"
echo ""
echo "  Spill count:"
printf "    BCT-Color better : %d\n" $bct_sp_better
printf "    Tied             : %d\n" $bct_sp_same
printf "    BCT-Color worse  : %d\n" $bct_sp_worse
echo ""
echo "  Register count (feasible runs only):"
printf "    BCT-Color better : %d\n" $bct_rg_better
printf "    Tied             : %d\n" $bct_rg_same
printf "    BCT-Color worse  : %d\n" $bct_rg_worse
echo ""

if (( bct_sp_worse > 0 )); then
    echo "  *** BCT-Color lost on spills in $bct_sp_worse case(s) — run with --verbose to inspect ***"
fi
if (( bct_rg_worse > 0 )); then
    echo "  *** BCT-Color used more registers in $bct_rg_worse case(s) — run with --verbose to inspect ***"
fi

echo "════════════════════════════════════════════════════════════════════════"
