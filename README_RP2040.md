# RP2040 hardware build

## Prerequisites

Set the SDK path:

```bash
export PICO_SDK_PATH="$HOME/pico-sdk"
```

The RP2040 target intentionally lets the Pico SDK select the correct ARM toolchain. Do not set `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, or `CMAKE_ASM_COMPILER` manually for this build.

## Build

```bash
make rp2040
```

Output:

```text
build-rp2040/language3d_rp2040.uf2
```

## Flash

Hold BOOTSEL while connecting the Pico, then copy the UF2 file to the `RPI-RP2` drive.

## ST7789 wiring

```text
ST7789   RP2040
GND      GND
VCC      3V3
SCL      GP18
SDA      GP19
DC       GP16
RES      GP17
BLK      GP20
```

The current first bring-up driver uses 40 MHz SPI and sends RGB565 line-by-line from an indexed8 framebuffer.

## v0.32.2 build fix

Renamed the engine-local `math.h` header to `l3d_math.h`. The old generic name collided with the Pico SDK's `<math.h>` include while compiling SDK C sources such as `pico_double/double_math.c`. The engine sources and tests now include `l3d_math.h`, preventing host/SDK header shadowing while leaving the portable API unchanged.


## ST7789 CS variant (current hardware profile)

This build assumes the previously successful extra contact on the display flex is the display Chip Select (CS). Default RP2040 mapping is `GP15 = CS`, active low. If your measured/remembered wire uses another GPIO, change `PIN_CS` in `platform/rp2040/src/display.cpp`.

Signals:
- SCL/SCK: GP18
- SDA/MOSI: GP19
- DC: GP16
- RES: GP17
- CS: GP15
- BLK: GP20

CS is explicitly driven high while idle and low for ST7789 transactions.
