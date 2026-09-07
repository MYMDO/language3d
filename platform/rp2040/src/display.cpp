#include "rp2040_display.h"
#include <pico/stdlib.h>
#include <hardware/spi.h>
#include <hardware/gpio.h>
#include <cstring>
#include <cstdint>

namespace {
constexpr uint16_t W = 240, H = 240;
constexpr uint PIN_SCK = 18;
constexpr uint PIN_MOSI = 19;
constexpr uint PIN_DC = 16;
constexpr uint PIN_RST = 17;
constexpr uint PIN_CS = 15;
constexpr uint PIN_BL = 20;
static uint8_t g_framebuffer[W * H];
static uint8_t g_line_bytes[W * 2];

void cmd(uint8_t value) {
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 0);
    spi_write_blocking(spi0, &value, 1);
    gpio_put(PIN_CS, 1);
}
void data(const uint8_t* bytes, size_t count) {
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);
    spi_write_blocking(spi0, bytes, count);
    gpio_put(PIN_CS, 1);
}
void cmd_data(uint8_t value, const uint8_t* bytes, size_t count) {
    cmd(value);
    if (count) data(bytes, count);
}
void reset_panel() {
    gpio_put(PIN_RST, 1); sleep_ms(5);
    gpio_put(PIN_RST, 0); sleep_ms(20);
    gpio_put(PIN_RST, 1); sleep_ms(120);
}
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t d[4];
    d[0] = uint8_t(x0 >> 8); d[1] = uint8_t(x0); d[2] = uint8_t(x1 >> 8); d[3] = uint8_t(x1);
    cmd_data(0x2A, d, 4);
    d[0] = uint8_t(y0 >> 8); d[1] = uint8_t(y0); d[2] = uint8_t(y1 >> 8); d[3] = uint8_t(y1);
    cmd_data(0x2B, d, 4);
    cmd(0x2C);
}
}

extern "C" int rp2040_display_init(void) {
    spi_init(spi0, 40000000u);
    // Working ST7789 diagnostic baseline: SPI mode 0, 8-bit, MSB-first.
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_DC); gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_init(PIN_BL); gpio_set_dir(PIN_BL, GPIO_OUT);

    gpio_put(PIN_CS, 1);
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_BL, 1);

    // Use the exact initialization sequence proven by the standalone
    // ST7789 diagnostic firmware. In particular, SWRESET + SLPOUT must
    // precede COLMOD/MADCTL on this panel profile.
    reset_panel();
    cmd(0x01); // SWRESET
    sleep_ms(150);
    cmd(0x11); // SLPOUT
    sleep_ms(120);

    const uint8_t colmod = 0x55; // RGB565 / 16 bpp
    const uint8_t madctl = 0x00; // portrait, RGB order
    cmd_data(0x3A, &colmod, 1);
    cmd_data(0x36, &madctl, 1);
    cmd(0x21); // display inversion on
    cmd(0x13); // normal display mode
    cmd(0x29); // display on
    sleep_ms(20);

    std::memset(g_framebuffer, 0, sizeof(g_framebuffer));
    return 1;
}

extern "C" uint8_t* rp2040_display_framebuffer(void) { return g_framebuffer; }

extern "C" int rp2040_display_present_indexed8(
    const uint8_t* pixels, uint16_t width, uint16_t height, uint32_t stride,
    const uint16_t* palette_rgb565) {
    if (!pixels || !palette_rgb565 || width != W || height != H || stride < W) return 0;

    // Keep command framing identical to the validated diagnostic driver.
    // set_window() manages CS around each command; pixel payload is then
    // streamed with a single CS assertion.
    set_window(0, 0, W - 1, H - 1);
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);
    for (uint16_t y = 0; y < H; ++y) {
        const uint8_t* src = pixels + size_t(y) * stride;
        for (uint16_t x = 0; x < W; ++x) {
            const uint16_t color = palette_rgb565[src[x]];
            // ST7789 RGB565 is transmitted most-significant byte first.
            g_line_bytes[size_t(x) * 2u + 0u] = uint8_t(color >> 8);
            g_line_bytes[size_t(x) * 2u + 1u] = uint8_t(color & 0xFFu);
        }
        spi_write_blocking(spi0, g_line_bytes, sizeof(g_line_bytes));
    }
    gpio_put(PIN_CS, 1);
    return 1;
}

extern "C" void rp2040_display_shutdown(void) {
    gpio_put(PIN_BL, 0);
    spi_deinit(spi0);
}
