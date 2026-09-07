# Windows build

> Recommended: the portable root build (native MSVC + vcpkg, no MSYS2).
> See [docs/platforms/windows.md](../docs/platforms/windows.md):
>
> ```powershell
> cmake -S . -B build-windows -DLANGUAGE3D_PLATFORM=host `
>   -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
> cmake --build build-windows --config Release
> ```
>
> The MSYS2 flow below remains as a legacy alternative.

The project keeps the same portable engine and SDL2 desktop backend. The RP2040/Pico SDK is not used by the Windows build.

## Recommended: MSYS2 UCRT64

Install MSYS2 and open **UCRT64**. Then:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-SDL2
```

From the project root:

```bash
cmake -S windows -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --parallel
```

Run:

```bash
./build-windows/language3d_mvp.exe
```

Keep the SDL2 DLL in the executable directory or on PATH.

## PowerShell

The included `build_windows.ps1` uses the same CMake project. It expects `cmake`, `ninja`, `python`, and a discoverable SDL2 development package.
