$ErrorActionPreference = 'Stop'
foreach ($tool in @('cmake','ninja','python')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool was not found in PATH." }
}
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $root 'build-windows'
cmake -S (Join-Path $root 'windows') -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build $build --parallel
Write-Host "Build complete: $build\language3d_mvp.exe"
