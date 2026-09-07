$exe = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..')).Path 'build-windows\language3d_mvp.exe'
if (-not (Test-Path $exe)) { throw "Build the project first: windows\build_windows.ps1" }
& $exe
