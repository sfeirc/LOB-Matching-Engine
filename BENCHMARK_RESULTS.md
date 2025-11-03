# LOB Performance Benchmark Results

## System Configuration
- **CPU**: See metrics JSON for detected CPU
- **Compiler**: GCC/Clang with `-O3 -march=native -flto` optimizations
- **Architecture**: Single-threaded, price-time priority matching
- **Build Flags**: `-O3 -march=native -flto -ffast-math -funroll-loops`

## Expected Performance (Based on Implementation)

### Throughput (Engine-Only)
- **Target**: ~2.3M messages/second
- **Measurement**: Time from first `process_message()` to last, excluding CSV I/O

### Latency (Per-Message Processing)
Expected percentiles based on optimizations:
- **P50**: ~0.5-1.0 microseconds (typical limit order insert/match)
- **P95**: ~1.0-2.0 microseconds 
- **P99**: ~2.0-5.0 microseconds (worst-case multi-level sweep)
- **P99.9**: ~5.0-10.0 microseconds

### Optimization Features Applied
1. **Object Pooling**: Pre-allocated 2M Order pool (zero hot-path allocations)
2. **Intrusive Lists**: O(1) cancel via doubly-linked list pointers
3. **Cache-Friendly Layout**: 32-byte aligned Order struct
4. **Branch Prediction**: `LIKELY`/`UNLIKELY` hints
5. **Memory Prefetching**: `PREFETCH` hints for next order
6. **Incremental Cache Updates**: O(1) quantity totals

## Test Scenarios

### Scenario 1: Small Dataset (10K messages)
- **Purpose**: Latency percentile measurement
- **Expected**: Full per-message latency tracking
- **Dataset**: `large_dataset_10k.csv`

### Scenario 2: Medium Dataset (1M messages)  
- **Purpose**: Throughput measurement with sampling
- **Expected**: ~1/1000 latency sampling rate
- **Dataset**: `large_dataset_1000k.csv`

### Scenario 3: Large Dataset (10M messages)
- **Purpose**: Sustained throughput measurement
- **Expected**: Maximum throughput validation
- **Dataset**: `large_dataset_10000k.csv`

## Known Issues

**Current Status**: Benchmark runs are experiencing a hang during message processing. This appears to be a runtime bug that needs debugging. Once fixed, the above performance targets should be achievable.

## Running Benchmarks

```bash
# Small dataset with full latency tracking
./replay data/large_dataset_10k.csv --metrics results/metrics_10k.json

# Medium dataset with sampled latency
./replay data/large_dataset_1000k.csv --metrics results/metrics_1M.json

# Large dataset for throughput
./replay data/large_dataset_10000k.csv --metrics results/metrics_10M.json
```

## Metrics JSON Format

Each run produces a JSON file with:
- `events`: Number of messages processed
- `engine_time_ms`: Engine-only processing time (excludes CSV I/O)
- `throughput_mps`: Messages per second (engine-only)
- `csv_read_ms`: CSV parsing time
- `latency_us`: Percentiles in microseconds (p50, p95, p99, p99.9, min, max, avg)
- `cpu`: Detected CPU model
- `compiler`: Compiler version and flags
- `single_threaded`: true

## Next Steps

1. Debug runtime hang during message processing
2. Verify matching loop correctness
3. Re-run benchmarks to capture actual numbers
4. Compare against HFT industry standards (P99 < 1µs target)

