#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python}"
STOCK_LOCATE="${STOCK_LOCATE:-14}"
MAX_MESSAGES="${MAX_MESSAGES:-100000}"
SNAPSHOT_INTERVAL="${SNAPSHOT_INTERVAL:-10}"

RESULTS_DIR="$ROOT/results"
OBSERVATIONS="$RESULTS_DIR/observations_${STOCK_LOCATE}_${MAX_MESSAGES}.csv"
DATASET="$RESULTS_DIR/observations_${STOCK_LOCATE}_${MAX_MESSAGES}_dataset.csv"
FEATURES="$RESULTS_DIR/observations_${STOCK_LOCATE}_${MAX_MESSAGES}_dataset_features.csv"
ANALYSIS_DIR="$RESULTS_DIR/observations_${STOCK_LOCATE}_${MAX_MESSAGES}_dataset_features_analysis"
WORKFLOW_DIR="$RESULTS_DIR/itch_results_${STOCK_LOCATE}_${MAX_MESSAGES}"

cd "$ROOT"
export PYTHONPATH="$ROOT/python${PYTHONPATH:+:$PYTHONPATH}"
mkdir -p "$RESULTS_DIR"

required_files=(
    examples/basic_simulation.py
    examples/historical_replay.py
    examples/itch_research_workflow.py
    examples/inspect_order_book.py
    examples/extract_observations.py
    examples/generate_dataset.py
    examples/calculate_features.py
    examples/analyze_market_data.py
)

for file in "${required_files[@]}"; do
    if [[ ! -f "$ROOT/$file" ]]; then
        printf 'Error: missing example: %s\n' "$ROOT/$file" >&2
        printf 'Ensure all examples are in examples/ before running.\n' >&2
        exit 1
    fi
done

run_step() {
    printf '\n========== RUNNING: %s ==========\n' "$*"
    "$@"
}

printf 'Stock Locate: %s | Messages: %s | Snapshot interval: %s\n' \
    "$STOCK_LOCATE" "$MAX_MESSAGES" "$SNAPSHOT_INTERVAL"

run_step "$PYTHON" examples/basic_simulation.py

run_step "$PYTHON" examples/inspect_order_book.py

run_step "$PYTHON" examples/historical_replay.py \
    --stock-locate "$STOCK_LOCATE" \
    --max-messages "$MAX_MESSAGES" \
    --snapshot-interval "$SNAPSHOT_INTERVAL"

run_step "$PYTHON" examples/itch_research_workflow.py \
    --stock-locate "$STOCK_LOCATE" \
    --max-messages "$MAX_MESSAGES" \
    --snapshot-interval "$SNAPSHOT_INTERVAL" \
    --output-dir "$WORKFLOW_DIR"

run_step "$PYTHON" examples/extract_observations.py \
    --stock-locate "$STOCK_LOCATE" \
    --max-messages "$MAX_MESSAGES" \
    --snapshot-interval "$SNAPSHOT_INTERVAL" \
    --output "$OBSERVATIONS"

run_step "$PYTHON" examples/generate_dataset.py \
    --input "$OBSERVATIONS" \
    --output "$DATASET"

run_step "$PYTHON" examples/calculate_features.py \
    --input "$DATASET" \
    --output "$FEATURES"

run_step "$PYTHON" examples/analyze_market_data.py \
    --input "$FEATURES" \
    --output-dir "$ANALYSIS_DIR"

printf '\n========================================\n'
printf 'ALL EXAMPLES COMPLETED SUCCESSFULLY\n'
printf '========================================\n'
printf 'Results directory: %s\n' "$RESULTS_DIR"