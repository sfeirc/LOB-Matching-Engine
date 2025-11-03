# Benchmark script for LOB performance testing

Write-Host "=== LOB Performance Benchmark ===" -ForegroundColor Cyan
Write-Host ""

$datasets = @(
    @{name="1k"; path="data/large_dataset_1k.csv"},
    @{name="10k"; path="data/large_dataset_10k.csv"},
    @{name="100k"; path="data/large_dataset_100k.csv"},
    @{name="1M"; path="data/large_dataset_1000k.csv"},
    @{name="10M"; path="data/large_dataset_10000k.csv"}
)

$results = @()

foreach ($dataset in $datasets) {
    if (Test-Path $dataset.path) {
        Write-Host "Testing with $($dataset.name) messages..." -ForegroundColor Yellow
        $output = & .\replay.exe $dataset.path 2>&1 | Out-String
        
        $result = @{
            Name = $dataset.name
            Output = $output
        }
        $results += $result
        
        # Extract key metrics
        if ($output -match "Throughput: ([\d.]+) messages/second") {
            $throughput = $matches[1]
            Write-Host "  Throughput: $throughput msg/s" -ForegroundColor Green
        }
        
        Write-Host ""
    } else {
        Write-Host "Dataset $($dataset.path) not found, skipping..." -ForegroundColor Red
    }
}

Write-Host "=== Benchmark Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Detailed results:" -ForegroundColor Cyan
foreach ($result in $results) {
    Write-Host "`n--- $($result.Name) ---" -ForegroundColor Magenta
    Write-Host $result.Output
}

