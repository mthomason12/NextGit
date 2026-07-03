#!/usr/bin/env pwsh

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "=== Building NextGlk ==="

Push-Location nextglk
try {
    make clean
    make
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Building Git against NextGlk ==="

Push-Location thirdparty/git
try {
    make `
        GLK=nextglk `
        GLKINCLUDEDIR=../../nextglk `
        GLKLIBDIR=../../nextglk `
        clean

    make `
        GLK=nextglk `
        GLKINCLUDEDIR=../../nextglk `
        GLKLIBDIR=../../nextglk `
        OPTIONS="-DUSE_MMAP"
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Build Complete ==="