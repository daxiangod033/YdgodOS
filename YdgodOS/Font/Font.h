#ifndef YDGODOS_FONT_H
#define YDGODOS_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YDGODOS_FONT_ASCII_FIRST 32U
#define YDGODOS_FONT_ASCII_LAST  126U
#define YDGODOS_FONT_GLYPH_COUNT 95U

extern const uint8_t OLED_F8x16[YDGODOS_FONT_GLYPH_COUNT][16];
extern const uint8_t OLED_F6x8[YDGODOS_FONT_GLYPH_COUNT][6];

#ifdef __cplusplus
}
#endif

#endif /* YDGODOS_FONT_H */
