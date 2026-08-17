#include "ydgodos_ssd1315.h"

#include <stddef.h>
#include <string.h>

#include "Font.h"

#define SSD1315_I2C_ADDRESS       0x3CU
#define SSD1315_CONTROL_COMMAND   0x00U
#define SSD1315_CONTROL_DATA      0x40U

/* Exactly 1 KiB: 128 columns x 8 pages. */
static uint8_t display_buffer[YDGODOS_SSD1315_FRAMEBUFFER_SIZE];

static ydgodos_soft_i2c_status_t send_commands(const uint8_t *commands,
                                                uint16_t count)
{
    return ydgodos_soft_i2c_reg_write(SSD1315_I2C_ADDRESS,
                                      SSD1315_CONTROL_COMMAND,
                                      commands, count);
}

ydgodos_soft_i2c_status_t ydgodos_ssd1315_init(void)
{
    static const uint8_t init_commands[] = {
        0xAEU,             /* Display off. */
        0xD5U, 0x80U,      /* Clock divide ratio and oscillator frequency. */
        0xA8U, 0x3FU,      /* Multiplex ratio: 64 rows. */
        0xD3U, 0x00U,      /* Display offset. */
        0x40U,             /* Display starts at line 0. */
        0x8DU, 0x14U,      /* Enable the internal charge pump. */
        0x20U, 0x00U,      /* Horizontal addressing mode. */
        0xA1U,             /* Segment remap for common 0.96-inch modules. */
        0xC8U,             /* COM output scan direction remapped. */
        0xDAU, 0x12U,      /* COM pin configuration for 128x64. */
        0x81U, 0x7FU,      /* Contrast. */
        0xD9U, 0xF1U,      /* Pre-charge period. */
        0xDBU, 0x40U,      /* VCOMH deselect level. */
        0xA4U,             /* Resume RAM content display. */
        0xA6U,             /* Normal (not inverted) display. */
        0x21U, 0x00U, 0x7FU,
        0x22U, 0x00U, 0x07U,
        0xAFU              /* Display on. */
    };
    ydgodos_soft_i2c_status_t status;

    ydgodos_soft_i2c_init(400000U);
    if (ydgodos_soft_i2c_check_device(SSD1315_I2C_ADDRESS) == 0U) {
        return YDGODOS_SOFT_I2C_NACK;
    }
    status = send_commands(init_commands, (uint16_t)sizeof(init_commands));
    if (status != YDGODOS_SOFT_I2C_OK) {
        return status;
    }
    ydgodos_ssd1315_clear();
    return ydgodos_ssd1315_display();
}

ydgodos_soft_i2c_status_t ydgodos_ssd1315_display(void)
{
    static const uint8_t window_commands[] = {
        0x21U, 0x00U, 0x7FU,
        0x22U, 0x00U, 0x07U
    };
    ydgodos_soft_i2c_status_t status;

    status = send_commands(window_commands, (uint16_t)sizeof(window_commands));
    if (status != YDGODOS_SOFT_I2C_OK) {
        return status;
    }
    return ydgodos_soft_i2c_reg_write(SSD1315_I2C_ADDRESS,
                                      SSD1315_CONTROL_DATA,
                                      display_buffer,
                                      YDGODOS_SSD1315_FRAMEBUFFER_SIZE);
}

void ydgodos_ssd1315_clear(void)
{
    memset(display_buffer, 0, sizeof(display_buffer));
}

void ydgodos_ssd1315_draw_char6x8(uint8_t x, uint8_t page, char character)
{
    uint8_t column;
    uint8_t glyph;
    uint16_t offset;

    if ((page >= YDGODOS_SSD1315_PAGE_COUNT) ||
        (x > (YDGODOS_SSD1315_WIDTH - 6U))) {
        return;
    }
    if (((uint8_t)character < YDGODOS_FONT_ASCII_FIRST) ||
        ((uint8_t)character > YDGODOS_FONT_ASCII_LAST)) {
        character = '?';
    }
    glyph = (uint8_t)character - YDGODOS_FONT_ASCII_FIRST;
    offset = (uint16_t)page * YDGODOS_SSD1315_WIDTH + x;
    for (column = 0U; column < 6U; column++) {
        display_buffer[offset + column] = OLED_F6x8[glyph][column];
    }
}

void ydgodos_ssd1315_draw_string6x8(uint8_t x, uint8_t page, const char *text)
{
    if (text == NULL) {
        return;
    }
    while ((*text != '\0') && (x <= (YDGODOS_SSD1315_WIDTH - 6U))) {
        ydgodos_ssd1315_draw_char6x8(x, page, *text++);
        x = (uint8_t)(x + 6U);
    }
}

uint8_t *ydgodos_ssd1315_framebuffer(void)
{
    return display_buffer;
}
