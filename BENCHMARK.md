# LOB Performance Benchmarking Guide

## Quick Start

### Single Dataset Test
```bash
cd build
./replay.exe data/large_dataset_10k.csv
```

### Test All Datasets (PowerShell)
```powershell
cd build
.\scripts\benchmark.ps1
```

### Test All Datasets (Bash/Linux)
```bash
cd build
bash ../scripts/benchmark.sh
```

## Available Datasets

Your `build/data/` directory contains:
- `large_dataset_1k.csv` - 1,000 messages (for quick tests)
- `large_dataset_10k.csv` - 10,000 messages (includes latency stats)
- `large_dataset_100k.csv` - 100,000 messages (includes latency stats)
- `large_dataset_1000k.csv` - 1,000,000 messages (includes latency stats)
- `large_dataset_10000k.csv` - 10,000,000 messages (throughput only)

## Performance Metrics Explained

### Throughput
- **Messages/second**: How many messages the engine can process per second
- Higher is better

### Latency Statistics (for datasets ≤ 1M)
- **Min**: Fastest message processing time
- **Avg**: Average processing time
- **P50**: Median latency (50% of messages are faster)
- **P95**: 95th percentile (95% of messages are faster) - important for HFT
- **P99**: 99th percentile - critical for low-latency trading
- **P99.9**: 99.9th percentile - worst-case scenarios
- **Max**: Slowest message processing time

### What to Look For

**Good Performance Indicators:**
- Throughput: > 500,000 messages/second
- P99 latency: < 10 microseconds (10,000 ns)
- Consistent latency (low variance between P50 and P99)

**Red Flags:**
- P99 much higher than P50 (indicates spikes)
- Throughput drops significantly with larger datasets
- High max latency (could indicate memory allocation issues)

## Interpreting Results

### Example Good Result (10k messages):
```
Throughput: 1,711,742 messages/second
P50: 500 ns
P95: 1000 ns  
P99: 1700 ns
```
This shows very consistent, fast processing.

### Typical Performance Targets:
- **HFT Systems**: < 1 microsecond P99 latency
- **Market Making**: < 10 microseconds P99 latency
- **Backtesting**: > 100,000 messages/second throughput

## Custom Testing

To test with a specific dataset:
```bash
./replay.exe data/your_custom_file.csv
```

To generate a custom dataset:
```bash
./generate_dataset.exe 5000000  # Generate 5M messages
./replay.exe data/large_dataset_5000k.csv
```

## Tips for Optimization

1. **Compare results**: Run benchmarks multiple times to check consistency
2. **Warm-up**: First run may be slower due to CPU cache warming
3. **System load**: Close other applications for accurate measurements
4. **Dataset variety**: Test with different message type distributions

