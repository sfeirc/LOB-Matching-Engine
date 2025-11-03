# Reproducible benchmark script for LOB performance testing (PowerShell)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "..\build"
$ResultsDir = Join-Path $ScriptDir "..\results"

New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null

Push-Location $BuildDir

Write-Host "=== LOB Benchmark Suite ===" -ForegroundColor Green
Write-Host "Build directory: $BuildDir"
Write-Host "Results directory: $ResultsDir"
Write-Host ""

$Datasets = @(
    @{File="data\large_dataset_10k.csv"; Name="10k"},
    @{File="data\large_dataset_100k.csv"; Name="100k"},
    @{File="data\large_dataset_1000k.csv"; Name="1M"},
    @{File="data\large_dataset_10000k.csv"; Name="10M"}
)

foreach ($dataset in $Datasets) {
    if (-not (Test-Path $dataset.File)) {
        Write-Host "Skipping $($dataset.File) (not found)" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "Running benchmark: $($dataset.Name)" -ForegroundColor Cyan
    Write-Host "-------------------"
    
    $OutputFile = Join-Path $ResultsDir "metrics_$($dataset.Name).json"
    & ".\replay.exe" $dataset.File --metrics $OutputFile
    
    Write-Host ""
}

Write-Host "=== Benchmark Complete ===" -ForegroundColor Green
Write-Host "Results written to: $ResultsDir"

Pop-Location

