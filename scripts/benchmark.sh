#!/bin/bash
# Benchmark script for LOB performance testing

echo "=== LOB Performance Benchmark ==="
echo ""

declare -a datasets=(
    "data/large_dataset_1k.csv:1k"
    "data/large_dataset_10k.csv:10k"
    "data/large_dataset_100k.csv:100k"
    "data/large_dataset_1000k.csv:1M"
    "data/large_dataset_10000k.csv:10M"
)

for entry in "${datasets[@]}"; do
    IFS=':' read -r file name <<< "$entry"
    if [ -f "$file" ]; then
        echo "Testing with $name messages..."
        ./replay "$file"
        echo ""
    else
        echo "Dataset $file not found, skipping..."
    fi
done

echo "=== Benchmark Complete ==="

