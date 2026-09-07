# RP2040 Game Hardware Bring-up

Full-game RP2040/ST7789 hardware test baseline after the ST7789 diagnostic firmware was observed working on the target display.

ST7789 profile:
- 240x240
- SPI0
- SCK GP18
- MOSI GP19
- CS GP15
- DC GP16
- RST GP17
- BLK GP20
- 40 MHz
- SPI mode 0
- 8-bit, MSB-first
- RGB565, MADCTL 0x00, inversion on

The game uses a single 240x240 indexed8 framebuffer and the existing palette-to-RGB565 line conversion.

## v0.34.1 ST7789 transport fix

The RP2040 ST7789 present path now serializes each RGB565 pixel explicitly as high byte then low byte before SPI transfer. This avoids host/ARM endianness assumptions from casting the uint16_t palette line directly to bytes.


### v0.34.2 display bring-up

The game backend now uses the exact ST7789 reset/init ordering proven by the standalone diagnostic firmware: SWRESET, SLPOUT, COLMOD, MADCTL, inversion, NORON, DISPON. At boot it also emits a short red/blue checkerboard through the same production display path before starting the game loop.
