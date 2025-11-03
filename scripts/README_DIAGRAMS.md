# Diagram Generation

## Overview

The `generate_diagrams.py` script generates four performance diagrams for the LOB matching engine:

1. **throughput_chart.png** - Throughput scaling vs dataset size
2. **latency_distribution.png** - Latency percentiles and distribution histogram
3. **memory_profile.png** - Memory allocation breakdown and usage over dataset sizes
4. **performance_comparison.png** - Comparison with industry standards

## Usage

```bash
# Install dependencies
pip install matplotlib numpy

# Generate diagrams
python scripts/generate_diagrams.py
```

## Output

Diagrams are saved to `docs/diagrams/`:
- `throughput_chart.png` - Shows throughput (msg/s) and engine time scaling
- `latency_distribution.png` - Shows P50/P95/P99/P99.9/Max latencies and distribution
- `memory_profile.png` - Shows memory breakdown pie chart and usage over datasets
- `performance_comparison.png` - Shows throughput and latency vs industry standards

## Updating Diagrams

To update with real benchmark data:

1. Run benchmarks: `./replay data/large_dataset_10000k.csv --metrics results/metrics.json`
2. Extract metrics from JSON files
3. Update values in `generate_diagrams.py`:
   - `throughput` array (line ~18)
   - `engine_time` array (line ~19)
   - `latency_values` array (line ~40)
   - Memory sizes (line ~75)
   - Comparison values (lines ~108, ~120)

## Diagram Details

### Throughput Chart
- X-axis: Dataset sizes (10K, 100K, 1M, 10M)
- Y-axis: Throughput (M msg/s) and Engine Time (seconds)
- Shows scaling behavior as dataset size increases

### Latency Distribution
- Left: Bar chart of latency percentiles (P50, P95, P99, P99.9, Max)
- Right: Histogram of latency distribution (simulated)
- Red line: HFT target (1 μs)

### Memory Profile
- Left: Pie chart of memory breakdown by component
- Right: Memory usage scaling with dataset size (log scale)
- Shows object pool, trade vector, order lookup, price levels

### Performance Comparison
- Left: Throughput comparison (horizontal bar chart)
- Right: P99 latency comparison (horizontal bar chart, log scale)
- Compares: This LOB Engine, HFT Target, Academic LOB, Reference (std::map)

