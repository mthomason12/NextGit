#!/usr/bin/env pwsh

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "=== Building CheapGlk ==="

Push-Location thirdparty/cheapglk
try {
    make clean
    make
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Building Git against CheapGlk ==="

Push-Location thirdparty/git
try {
    make clean
    make
    mv git gitc
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Build Complete ==="
