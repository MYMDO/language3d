# Linux

## Install dependencies

Fedora:

```bash
sudo dnf install gcc-c++ make cmake ninja-build SDL2-devel pkgconf-pkg-config python3
```

Ubuntu/Debian:

```bash
sudo apt install g++ make cmake ninja-build libsdl2-dev pkg-config python3
```

## Build

```bash
./tools/build-linux.sh        # release (default)
./tools/build-linux.sh debug  # debug
```

The game binary is `build-linux/language3d`.

Equivalent manual CMake invocations:

```bash
cmake --preset linux-release
cmake --build --preset linux-release
```

## Run

```bash
./build-linux/language3d
```

`F11` toggles fullscreen. Click the window to capture the mouse (`Esc` releases it).

## Tests

```bash
./tools/test.sh
# or
ctest --preset linux-release --output-on-failure
```

## Package

```bash
./tools/package-linux.sh
```

Produces `dist/language3d-<version>-linux-x86_64.tar.gz` containing the
executable, assets, README and license.
