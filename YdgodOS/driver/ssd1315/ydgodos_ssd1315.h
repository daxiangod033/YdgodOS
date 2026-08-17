#ifndef YDGODOS_SSD1315_H
#define YDGODOS_SSD1315_H

#include <stdint.h>

#include "ydgodos_soft_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define YDGODOS_SSD1315_WIDTH             128U
#define YDGODOS_SSD1315_HEIGHT             64U
#define YDGODOS_SSD1315_PAGE_COUNT          8U
#define YDGODOS_SSD1315_FRAMEBUFFER_SIZE 1024U

ydgodos_soft_i2c_status_t ydgodos_ssd1315_init(void);
ydgodos_soft_i2c_status_t ydgodos_ssd1315_display(void);
void ydgodos_ssd1315_clear(void);
void ydgodos_ssd1315_draw_char6x8(uint8_t x, uint8_t page, char character);
void ydgodos_ssd1315_draw_string6x8(uint8_t x, uint8_t page, const char *text);
uint8_t *ydgodos_ssd1315_framebuffer(void);

#ifdef __cplusplus
}
#endif

#endif /* YDGODOS_SSD1315_H */
