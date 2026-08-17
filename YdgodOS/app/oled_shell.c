#include "oled_shell.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "ydgodos_ssd1315.h"

#define OLED_SHELL_COLUMNS          21U
#define OLED_SHELL_VISIBLE_ROWS      8U
#define OLED_SHELL_HISTORY_ROWS     (OLED_SHELL_CHARACTER_BUFFER_SIZE / OLED_SHELL_COLUMNS)
#define OLED_SHELL_REFRESH_MS       (1000U / OLED_SHELL_REFRESH_HZ)
#define OLED_SHELL_RETRY_TICKS      10U

/* Exactly 1 KiB. The first 1008 bytes hold 48 rows x 21 columns. */
static uint8_t character_buffer[OLED_SHELL_CHARACTER_BUFFER_SIZE];
static uint8_t current_row;
static uint8_t history_row_count;
static uint8_t current_column;
static uint8_t escape_state;
static uint8_t csi_parameter;
static uint8_t view_offset;
static uint8_t history_initialized;
static volatile uint8_t display_dirty;

static uint16_t row_offset(uint8_t row)
{
    return (uint16_t)row * OLED_SHELL_COLUMNS;
}

static void history_initialize_locked(void)
{
    if (history_initialized == 0U) {
        memset(character_buffer, ' ', sizeof(character_buffer));
        current_row = 0U;
        history_row_count = 1U;
        current_column = 0U;
        escape_state = 0U;
        csi_parameter = 0U;
        view_offset = 0U;
        history_initialized = 1U;
        display_dirty = 1U;
    }
}

static void history_clear_locked(void)
{
    memset(character_buffer, ' ', sizeof(character_buffer));
    current_row = 0U;
    history_row_count = 1U;
    current_column = 0U;
    view_offset = 0U;
    display_dirty = 1U;
}

static uint8_t maximum_view_offset_locked(void)
{
    if (history_row_count > OLED_SHELL_VISIBLE_ROWS) {
        return (uint8_t)(history_row_count - OLED_SHELL_VISIBLE_ROWS);
    }
    return 0U;
}

static void history_new_line_locked(void)
{
    uint8_t maximum_offset;

    current_row++;
    if (current_row >= OLED_SHELL_HISTORY_ROWS) {
        current_row = 0U;
    }
    if (history_row_count < OLED_SHELL_HISTORY_ROWS) {
        history_row_count++;
    }
    memset(&character_buffer[row_offset(current_row)], ' ', OLED_SHELL_COLUMNS);
    current_column = 0U;

    /* Keep the same old rows visible while new output arrives. */
    maximum_offset = maximum_view_offset_locked();
    if ((view_offset != 0U) && (view_offset < maximum_offset)) {
        view_offset++;
    }
}

static void history_put_printable_locked(uint8_t character)
{
    if (current_column >= OLED_SHELL_COLUMNS) {
        history_new_line_locked();
    }
    character_buffer[row_offset(current_row) + current_column] = character;
    current_column++;
}

static void history_put_locked(uint8_t character)
{
    uint8_t spaces;

    if (escape_state == 1U) {
        if (character == (uint8_t)'[') {
            escape_state = 2U;
            csi_parameter = 0U;
        } else {
            escape_state = 0U;
        }
        return;
    }
    if (escape_state == 2U) {
        if ((character >= (uint8_t)'0') && (character <= (uint8_t)'9')) {
            csi_parameter = (uint8_t)(csi_parameter * 10U +
                                      character - (uint8_t)'0');
            return;
        }
        if ((character >= 0x40U) && (character <= 0x7EU)) {
            if ((character == (uint8_t)'J') && (csi_parameter == 2U)) {
                history_clear_locked();
            }
            escape_state = 0U;
        }
        return;
    }
    if (character == 0x1BU) {
        escape_state = 1U;
    } else if (character == (uint8_t)'\n') {
        history_new_line_locked();
    } else if (character == (uint8_t)'\r') {
        current_column = 0U;
    } else if (character == (uint8_t)'\b') {
        if (current_column != 0U) {
            current_column--;
            character_buffer[row_offset(current_row) + current_column] = ' ';
        }
    } else if (character == (uint8_t)'\t') {
        spaces = (uint8_t)(4U - (current_column & 3U));
        while (spaces-- != 0U) {
            history_put_printable_locked(' ');
        }
    } else if ((character >= 0x20U) && (character <= 0x7EU)) {
        history_put_printable_locked(character);
    } else if (character >= 0x80U) {
        history_put_printable_locked('?');
    }
}

static void history_write(const uint8_t *data, uint16_t length)
{
    uint16_t index;

    if ((data == NULL) || (length == 0U)) {
        return;
    }
    taskENTER_CRITICAL();
    history_initialize_locked();
    for (index = 0U; index < length; index++) {
        history_put_locked(data[index]);
    }
    display_dirty = 1U;
    taskEXIT_CRITICAL();
}

void oled_shell_write_tx(const uint8_t *data, uint16_t length)
{
    history_write(data, length);
}

void oled_shell_write_rx(const uint8_t *data, uint16_t length)
{
    history_write(data, length);
}

void oled_shell_scroll_up(void)
{
    uint8_t maximum_offset;

    taskENTER_CRITICAL();
    history_initialize_locked();
    maximum_offset = maximum_view_offset_locked();
    if (view_offset < maximum_offset) {
        view_offset++;
        display_dirty = 1U;
    }
    taskEXIT_CRITICAL();
}

void oled_shell_scroll_down(void)
{
    taskENTER_CRITICAL();
    history_initialize_locked();
    if (view_offset != 0U) {
        view_offset--;
        display_dirty = 1U;
    }
    taskEXIT_CRITICAL();
}

static void snapshot_view(uint8_t view[OLED_SHELL_VISIBLE_ROWS][OLED_SHELL_COLUMNS])
{
    uint8_t available_rows;
    uint8_t visible_rows;
    uint8_t leading_rows;
    uint8_t output_row;
    uint8_t age;
    uint8_t source_row;

    taskENTER_CRITICAL();
    history_initialize_locked();
    memset(view, ' ', OLED_SHELL_VISIBLE_ROWS * OLED_SHELL_COLUMNS);
    available_rows = (uint8_t)(history_row_count - view_offset);
    visible_rows = (available_rows < OLED_SHELL_VISIBLE_ROWS) ?
                   available_rows : OLED_SHELL_VISIBLE_ROWS;
    leading_rows = (uint8_t)(OLED_SHELL_VISIBLE_ROWS - visible_rows);

    for (output_row = 0U; output_row < visible_rows; output_row++) {
        age = (uint8_t)(view_offset + visible_rows - output_row - 1U);
        source_row = (uint8_t)((current_row + OLED_SHELL_HISTORY_ROWS - age) %
                               OLED_SHELL_HISTORY_ROWS);
        memcpy(view[leading_rows + output_row],
               &character_buffer[row_offset(source_row)], OLED_SHELL_COLUMNS);
    }
    taskEXIT_CRITICAL();
}

static uint8_t take_display_dirty(void)
{
    uint8_t dirty;

    taskENTER_CRITICAL();
    dirty = display_dirty;
    display_dirty = 0U;
    taskEXIT_CRITICAL();
    return dirty;
}

static void mark_display_dirty(void)
{
    taskENTER_CRITICAL();
    display_dirty = 1U;
    taskEXIT_CRITICAL();
}

static void render_display(void)
{
    uint8_t view[OLED_SHELL_VISIBLE_ROWS][OLED_SHELL_COLUMNS];
    uint8_t row;
    uint8_t column;

    snapshot_view(view);
    ydgodos_ssd1315_clear();
    for (row = 0U; row < OLED_SHELL_VISIBLE_ROWS; row++) {
        for (column = 0U; column < OLED_SHELL_COLUMNS; column++) {
            ydgodos_ssd1315_draw_char6x8((uint8_t)(column * 6U), row,
                                          (char)view[row][column]);
        }
    }
}

void oled_shell_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint8_t initialized = 0U;
    uint8_t retry_count = 0U;

    (void)argument;
    taskENTER_CRITICAL();
    history_initialize_locked();
    taskEXIT_CRITICAL();

    for (;;) {
        if (initialized == 0U) {
            if (retry_count == 0U) {
                if (ydgodos_ssd1315_init() == YDGODOS_SOFT_I2C_OK) {
                    initialized = 1U;
                    render_display();
                    if (ydgodos_ssd1315_display() == YDGODOS_SOFT_I2C_OK) {
                        (void)take_display_dirty();
                    } else {
                        initialized = 0U;
                    }
                }
                retry_count = OLED_SHELL_RETRY_TICKS;
            } else {
                retry_count--;
            }
        } else if (take_display_dirty() != 0U) {
            render_display();
            if (ydgodos_ssd1315_display() != YDGODOS_SOFT_I2C_OK) {
                initialized = 0U;
                retry_count = OLED_SHELL_RETRY_TICKS;
                mark_display_dirty();
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(OLED_SHELL_REFRESH_MS));
    }
}
