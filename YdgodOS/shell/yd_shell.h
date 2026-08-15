#ifndef YD_SHELL_H
#define YD_SHELL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YD_SHELL_LINE_SIZE 96U
#define YD_SHELL_ARG_MAX   8U
#define YD_SHELL_HISTORY_DEPTH 4U
#define YD_SHELL_COMPLETION_MAX 24U

typedef struct yd_shell yd_shell_t;

typedef int (*yd_shell_read_fn)(uint8_t *ch);
typedef void (*yd_shell_write_fn)(const uint8_t *data, uint16_t length);
typedef int (*yd_shell_command_fn)(yd_shell_t *shell, int argc, char **argv);
typedef uint16_t (*yd_shell_completion_fn)(uint8_t argument_index,
                                           const char *prefix,
                                           const char **candidates,
                                           uint16_t max_candidates);

typedef struct {
    const char *name;
    const char *usage;
    const char *summary;
    yd_shell_command_fn function;
    yd_shell_completion_fn complete;
} yd_shell_command_t;

struct yd_shell {
    yd_shell_read_fn read;
    yd_shell_write_fn write;
    const yd_shell_command_t *commands;
    uint16_t command_count;
    const char *prompt;
    char line[YD_SHELL_LINE_SIZE];
    char history[YD_SHELL_HISTORY_DEPTH][YD_SHELL_LINE_SIZE];
    uint16_t length;
    uint8_t history_count;
    uint8_t history_index;
    uint8_t escape_state;
    uint8_t tab_pending;
    uint8_t ignore_lf;
};

void yd_shell_init(yd_shell_t *shell,
                   yd_shell_read_fn read_fn,
                   yd_shell_write_fn write_fn,
                   const yd_shell_command_t *commands,
                   uint16_t command_count,
                   const char *prompt);
void yd_shell_poll(yd_shell_t *shell);
void yd_shell_prompt(yd_shell_t *shell);
void yd_shell_puts(yd_shell_t *shell, const char *text);
void yd_shell_printf(yd_shell_t *shell, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
