#include "oled_shell.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "ydgodos_ssd1315.h"

#define OLED_SHELL_COLUMNS           21U
#define OLED_SHELL_ROWS               8U
#define OLED_SHELL_TX_SOURCE_FLAG   0x00U
#define OLED_SHELL_RX_SOURCE_FLAG   0x80U
#define OLED_SHELL_DATA_MASK        0x7FU
#define OLED_SHELL_BUFFER_MASK      (OLED_SHELL_CHARACTER_BUFFER_SIZE - 1U)
#define OLED_SHELL_REFRESH_MS       (1000U / OLED_SHELL_REFRESH_HZ)
#define OLED_SHELL_RETRY_TICKS      10U

#if (OLED_SHELL_CHARACTER_BUFFER_SIZE & OLED_SHELL_BUFFER_MASK) != 0
#error OLED_SHELL_CHARACTER_BUFFER_SIZE must be a power of two
#endif

typedef struct {
    char cells[OLED_SHELL_ROWS][OLED_SHELL_COLUMNS];
    uint8_t row;
    uint8_t column;
    uint8_t escape_state;
} oled_shell_terminal_t;

/* Exactly 1 KiB. Bit 7 stores RX/TX source; bits 0..6 store ASCII. */
static uint8_t character_buffer[OLED_SHELL_CHARACTER_BUFFER_SIZE];
static volatile uint16_t buffer_head;
static volatile uint16_t buffer_tail;
static volatile uint16_t buffer_count;
static volatile uint32_t dropped_characters;

static oled_shell_terminal_t terminal;

static uint8_t normalize_character(uint8_t character)
{
    if ((character == '\r') || (character == '\n') ||
        (character == '\t') || (character == '\b') ||
        (character == 0x1BU)) {
        return character;
    }
    if ((character < 0x20U) || (character > 0x7EU)) {
        return (uint8_t)'?';
    }
    return character;
}

static void buffer_write(const uint8_t *data, uint16_t length, uint8_t source)
{
    uint16_t index;

    if ((data == NULL) || (length == 0U)) {
        return;
    }
    taskENTER_CRITICAL();
    for (index = 0U; index < length; index++) {
        character_buffer[buffer_head] = (uint8_t)(normalize_character(data[index]) | source);
        buffer_head = (uint16_t)((buffer_head + 1U) & OLED_SHELL_BUFFER_MASK);
        if (buffer_count < OLED_SHELL_CHARACTER_BUFFER_SIZE) {
            buffer_count++;
        } else {
            buffer_tail = (uint16_t)((buffer_tail + 1U) & OLED_SHELL_BUFFER_MASK);
            dropped_characters++;
        }
    }
    taskEXIT_CRITICAL();
}

static uint8_t buffer_read(uint8_t *value)
{
    uint8_t available = 0U;

    taskENTER_CRITICAL();
    if (buffer_count != 0U) {
        *value = character_buffer[buffer_tail];
        buffer_tail = (uint16_t)((buffer_tail + 1U) & OLED_SHELL_BUFFER_MASK);
        buffer_count--;
        available = 1U;
    }
    taskEXIT_CRITICAL();
    return available;
}

void oled_shell_write_tx(const uint8_t *data, uint16_t length)
{
    buffer_write(data, length, OLED_SHELL_TX_SOURCE_FLAG);
}

void oled_shell_write_rx(const uint8_t *data, uint16_t length)
{
    buffer_write(data, length, OLED_SHELL_RX_SOURCE_FLAG);
}

static void terminal_reset(oled_shell_terminal_t *terminal)
{
    memset(terminal->cells, ' ', sizeof(terminal->cells));
    terminal->row = 0U;
    terminal->column = 0U;
    terminal->escape_state = 0U;
}

static void terminal_new_line(oled_shell_terminal_t *terminal)
{
    if (terminal->row + 1U < OLED_SHELL_ROWS) {
        terminal->row++;
    } else {
        memmove(terminal->cells[0], terminal->cells[1],
                (OLED_SHELL_ROWS - 1U) * OLED_SHELL_COLUMNS);
        memset(terminal->cells[OLED_SHELL_ROWS - 1U], ' ', OLED_SHELL_COLUMNS);
    }
    terminal->column = 0U;
}

static void terminal_put_printable(oled_shell_terminal_t *terminal, char character)
{
    if (terminal->column >= OLED_SHELL_COLUMNS) {
        terminal_new_line(terminal);
    }
    terminal->cells[terminal->row][terminal->column++] = character;
}

static void terminal_put(oled_shell_terminal_t *terminal, uint8_t character)
{
    uint8_t spaces;

    if (terminal->escape_state == 1U) {
        terminal->escape_state = (character == (uint8_t)'[') ? 2U : 0U;
        return;
    }
    if (terminal->escape_state == 2U) {
        if ((character >= 0x40U) && (character <= 0x7EU)) {
            terminal->escape_state = 0U;
        }
        return;
    }
    if (character == 0x1BU) {
        terminal->escape_state = 1U;
    } else if (character == (uint8_t)'\n') {
        terminal_new_line(terminal);
    } else if (character == (uint8_t)'\r') {
        terminal->column = 0U;
    } else if (character == (uint8_t)'\b') {
        if (terminal->column != 0U) {
            terminal->column--;
            terminal->cells[terminal->row][terminal->column] = ' ';
        }
    } else if (character == (uint8_t)'\t') {
        spaces = (uint8_t)(4U - (terminal->column & 3U));
        while (spaces-- != 0U) {
            terminal_put_printable(terminal, ' ');
        }
    } else if ((character >= 0x20U) && (character <= 0x7EU)) {
        terminal_put_printable(terminal, (char)character);
    }
}

static uint8_t process_character_buffer(void)
{
    uint8_t entry;
    uint8_t changed = 0U;

    while (buffer_read(&entry) != 0U) {
        terminal_put(&terminal, entry & OLED_SHELL_DATA_MASK);
        changed = 1U;
    }
    return changed;
}

static void render_terminal(const oled_shell_terminal_t *terminal)
{
    uint8_t row;
    uint8_t column;

    for (row = 0U; row < OLED_SHELL_ROWS; row++) {
        for (column = 0U; column < OLED_SHELL_COLUMNS; column++) {
            ydgodos_ssd1315_draw_char6x8((uint8_t)(column * 6U),
                                          row,
                                          terminal->cells[row][column]);
        }
    }
}

static void render_display(void)
{
    ydgodos_ssd1315_clear();
    render_terminal(&terminal);
}

void oled_shell_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint8_t initialized = 0U;
    uint8_t retry_count = 0U;
    uint8_t changed;

    (void)argument;
    terminal_reset(&terminal);

    for (;;) {
        changed = process_character_buffer();
        if (initialized == 0U) {
            if (retry_count == 0U) {
                if (ydgodos_ssd1315_init() == YDGODOS_SOFT_I2C_OK) {
                    initialized = 1U;
                    render_display();
                    (void)ydgodos_ssd1315_display();
                }
                retry_count = OLED_SHELL_RETRY_TICKS;
            } else {
                retry_count--;
            }
        } else if (changed != 0U) {
            render_display();
            if (ydgodos_ssd1315_display() != YDGODOS_SOFT_I2C_OK) {
                initialized = 0U;
                retry_count = OLED_SHELL_RETRY_TICKS;
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(OLED_SHELL_REFRESH_MS));
    }
}
