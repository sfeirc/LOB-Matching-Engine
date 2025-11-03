#!/bin/bash
# Reproducible benchmark script for LOB performance testing

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"
RESULTS_DIR="$SCRIPT_DIR/../results"

mkdir -p "$RESULTS_DIR"

cd "$BUILD_DIR"

echo "=== LOB Benchmark Suite ==="
echo "Build directory: $BUILD_DIR"
echo "Results directory: $RESULTS_DIR"
echo ""

# Test datasets
DATASETS=(
    "data/large_dataset_10k.csv:10k"
    "data/large_dataset_100k.csv:100k"
    "data/large_dataset_1000k.csv:1M"
    "data/large_dataset_10000k.csv:10M"
)

for dataset_info in "${DATASETS[@]}"; do
    IFS=':' read -r dataset_file dataset_name <<< "$dataset_info"
    
    if [ ! -f "$dataset_file" ]; then
        echo "Skipping $dataset_file (not found)"
        continue
    fi
    
    echo "Running benchmark: $dataset_name"
    echo "-------------------"
    
    OUTPUT_FILE="$RESULTS_DIR/metrics_${dataset_name}.json"
    ./replay "$dataset_file" --metrics "$OUTPUT_FILE"
    
    echo ""
done

echo "=== Benchmark Complete ==="
echo "Results written to: $RESULTS_DIR/"

