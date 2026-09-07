# RP2040 port status — v0.32.1

- Pico SDK / ARM GCC build system: integrated
- RP2040 + ST7789 240x240 SPI backend: integrated
- Indexed8 single framebuffer: 57.6 KiB
- Renderer: shared GameCore/RendererCore
- v0.32.1 fixes ARM EABI C++ type portability errors in `std::max` calls.
- Next hardware step: build UF2, flash Pico, verify ST7789 output, then measure FPS/SRAM on hardware.
