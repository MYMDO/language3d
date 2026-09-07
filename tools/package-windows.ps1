# Package a Windows release archive from an existing build-windows directory.
# Usage: powershell -ExecutionPolicy Bypass -File tools\package-windows.ps1
# Produces: language3d-<version>-windows-x86_64.zip
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = (Get-Content (Join-Path $root 'VERSION')).Trim()
$build = Join-Path $root 'build-windows'
$out = Join-Path $root 'dist'
$stage = Join-Path $out 'stage-windows'
$pkgName = "language3d-$version-windows-x86_64"
$pkgDir = Join-Path $stage $pkgName

$exe = Join-Path $build 'Release\language3d.exe'
if (-not (Test-Path $exe)) { $exe = Join-Path $build 'language3d.exe' }
if (-not (Test-Path $exe)) { throw "language3d.exe not found in $build - build first (preset windows-release)." }

Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $pkgDir | Out-Null
Copy-Item $exe (Join-Path $pkgDir 'language3d.exe')
Copy-Item (Join-Path $root 'assets') (Join-Path $pkgDir 'assets') -Recurse
Copy-Item (Join-Path $root 'README.md') (Join-Path $pkgDir 'README.md')
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $pkgDir 'LICENSE.txt')
$platformDoc = Join-Path $root 'docs\platforms\windows.md'
if (Test-Path $platformDoc) { Copy-Item $platformDoc (Join-Path $pkgDir 'README-platform.md') }

# Bundle the MSVC runtime + SDL2 next to the exe when they are discoverable.
# NOTE: the canonical build (static MSVC runtime + static SDL2, see
# docs/platforms/windows.md) needs no companion DLLs at all; this block only
# exists for legacy shared-CRT builds and is a harmless no-op otherwise.
$vcpkgBin = Join-Path $env:VCPKG_INSTALLATION_ROOT 'installed\x64-windows\bin\SDL2.dll'
if (($env:VCPKG_INSTALLATION_ROOT) -and (Test-Path $vcpkgBin)) {
  Copy-Item $vcpkgBin (Join-Path $pkgDir 'SDL2.dll')
} else {
  $sdl2 = Get-Command SDL2.dll -ErrorAction SilentlyContinue
  if ($sdl2) { Copy-Item $sdl2.Source (Join-Path $pkgDir 'SDL2.dll') }
}

New-Item -ItemType Directory -Force -Path $out | Out-Null
$zip = Join-Path $out "$pkgName.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $pkgDir -DestinationPath $zip
Write-Host "OK: $zip"
