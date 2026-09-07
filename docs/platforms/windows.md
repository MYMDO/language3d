# Windows

No MSYS2, Arch, pacman or manual DLL hunting is required. The build uses the
native MSVC toolchain, CMake and vcpkg-provided SDL2.

## Prerequisites

- Visual Studio 2022 (Desktop development with C++) **or** the standalone
  Build Tools for Visual Studio
- CMake ≥ 3.20 and Ninja (both ship with Visual Studio; or install them
  separately and put them on `PATH`)
- Python 3 (for the build-time asset embedding step)

## Build (command line)

```powershell
vcpkg install sdl2:x64-windows-static
cmake -S . -B build-windows -DLANGUAGE3D_PLATFORM=host `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build-windows --config Release
ctest --test-dir build-windows -C Release --output-on-failure
```

## Portability (no VC Redistributable needed)

The Windows build links the MSVC runtime **statically** (`/MT`,
`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`) and consumes SDL2 as a static
library (vcpkg `x64-windows-static` triplet, zlib-licensed — static linking
is license-compatible). The result is a single self-contained
`language3d.exe` whose imports are Windows system DLLs only
(`KERNEL32`, `USER32`, `GDI32`, `WINMM`, `IMM32`, `OLE32`, `SHELL32`, …).
CI enforces this with a `dumpbin /DEPENDENTS` gate that fails the build if
`MSVCP*`, `VCRUNTIME*` or `SDL2.dll` ever reappear in the dependency chain.

Or with the presets (after configuring once with the toolchain file above):

```powershell
cmake --build --preset windows-release
```

## Run

```powershell
.\build-windows\Release\language3d.exe
```

`F11` toggles fullscreen. Click the window to capture the mouse (`Esc` releases it).

## Package

```powershell
powershell -ExecutionPolicy Bypass -File tools\package-windows.ps1
```

Produces `dist\language3d-<version>-windows-x86_64.zip` with the `.exe`,
assets, bundled `SDL2.dll` when discoverable, README and license.
