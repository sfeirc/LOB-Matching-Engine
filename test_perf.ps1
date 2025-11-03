# Performance test script
Write-Host "=== Testing 1M Messages ===" -ForegroundColor Green
.\replay.exe data\large_dataset_1000k.csv

Write-Host "`n=== Testing 10M Messages ===" -ForegroundColor Green  
.\replay.exe data\large_dataset_10000k.csv

