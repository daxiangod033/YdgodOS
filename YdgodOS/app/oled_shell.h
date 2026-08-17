#ifndef OLED_SHELL_H
#define OLED_SHELL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_SHELL_CHARACTER_BUFFER_SIZE 1024U
#define OLED_SHELL_REFRESH_HZ               20U

void oled_shell_write_tx(const uint8_t *data, uint16_t length);
void oled_shell_write_rx(const uint8_t *data, uint16_t length);
void oled_shell_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* OLED_SHELL_H */
