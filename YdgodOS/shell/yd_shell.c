#include "yd_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void shell_write_char(yd_shell_t *shell, char ch)
{
    shell->write((const uint8_t *)&ch, 1U);
}

void yd_shell_puts(yd_shell_t *shell, const char *text)
{
    if ((shell == NULL) || (text == NULL)) {
        return;
    }
    shell->write((const uint8_t *)text, (uint16_t)strlen(text));
}

void yd_shell_printf(yd_shell_t *shell, const char *format, ...)
{
    char buffer[192];
    int length;
    va_list args;

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0) {
        return;
    }
    if ((unsigned int)length >= sizeof(buffer)) {
        length = (int)sizeof(buffer) - 1;
    }
    shell->write((const uint8_t *)buffer, (uint16_t)length);
}

void yd_shell_prompt(yd_shell_t *shell)
{
    yd_shell_puts(shell, shell->prompt);
}

void yd_shell_init(yd_shell_t *shell,
                   yd_shell_read_fn read_fn,
                   yd_shell_write_fn write_fn,
                   const yd_shell_command_t *commands,
                   uint16_t command_count,
                   const char *prompt)
{
    memset(shell, 0, sizeof(*shell));
    shell->read = read_fn;
    shell->write = write_fn;
    shell->commands = commands;
    shell->command_count = command_count;
    shell->prompt = prompt;
}

static void shell_help(yd_shell_t *shell, int argc, char **argv)
{
    uint16_t index;

    if (argc > 1) {
        for (index = 0U; index < shell->command_count; index++) {
            if (strcmp(argv[1], shell->commands[index].name) == 0) {
                yd_shell_printf(shell, "usage: %s\r\n%s\r\n",
                                shell->commands[index].usage,
                                shell->commands[index].summary);
                return;
            }
        }
        yd_shell_printf(shell, "help: no such command: %s\r\n", argv[1]);
        return;
    }

    yd_shell_puts(shell,
                  "Built-in commands:\r\n"
                  "  help [command]       Show commands or detailed usage\r\n"
                  "  clear                Clear the terminal\r\n"
                  "  echo [text ...]      Print text\r\n"
                  "  history              Show recent commands\r\n");
    for (index = 0U; index < shell->command_count; index++) {
        yd_shell_printf(shell, "  %-20s %s\r\n",
                        shell->commands[index].usage,
                        shell->commands[index].summary);
    }
}

static int shell_split(char *line, char **argv)
{
    int argc = 0;
    char *cursor = line;

    while ((*cursor != '\0') && (argc < (int)YD_SHELL_ARG_MAX)) {
        while ((*cursor == ' ') || (*cursor == '\t')) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        argv[argc++] = cursor;
        while ((*cursor != '\0') && (*cursor != ' ') && (*cursor != '\t')) {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return argc;
}

static void shell_save_history(yd_shell_t *shell)
{
    uint8_t index;

    if (shell->length == 0U) {
        return;
    }
    if ((shell->history_count > 0U) &&
        (strcmp(shell->history[shell->history_count - 1U], shell->line) == 0)) {
        shell->history_index = 0U;
        return;
    }
    if (shell->history_count == YD_SHELL_HISTORY_DEPTH) {
        for (index = 1U; index < YD_SHELL_HISTORY_DEPTH; index++) {
            strcpy(shell->history[index - 1U], shell->history[index]);
        }
        shell->history_count--;
    }
    strcpy(shell->history[shell->history_count], shell->line);
    shell->history_count++;
    shell->history_index = 0U;
}

static void shell_show_history(yd_shell_t *shell)
{
    uint8_t index;

    for (index = 0U; index < shell->history_count; index++) {
        yd_shell_printf(shell, "  %u  %s\r\n", (unsigned int)(index + 1U),
                        shell->history[index]);
    }
}

static void shell_execute(yd_shell_t *shell)
{
    char *argv[YD_SHELL_ARG_MAX];
    int argc;
    int index;

    shell->line[shell->length] = '\0';
    shell_save_history(shell);
    argc = shell_split(shell->line, argv);
    if (argc == 0) {
        return;
    }

    if (strcmp(argv[0], "help") == 0) {
        shell_help(shell, argc, argv);
        return;
    }
    if (strcmp(argv[0], "clear") == 0) {
        yd_shell_puts(shell, "\033[2J\033[H");
        return;
    }
    if (strcmp(argv[0], "echo") == 0) {
        for (index = 1; index < argc; index++) {
            yd_shell_puts(shell, argv[index]);
            if ((index + 1) < argc) {
                shell_write_char(shell, ' ');
            }
        }
        yd_shell_puts(shell, "\r\n");
        return;
    }
    if (strcmp(argv[0], "history") == 0) {
        shell_show_history(shell);
        return;
    }

    for (index = 0; index < (int)shell->command_count; index++) {
        if (strcmp(argv[0], shell->commands[index].name) == 0) {
            (void)shell->commands[index].function(shell, argc, argv);
            return;
        }
    }
    yd_shell_printf(shell, "%s: command not found (try 'help')\r\n", argv[0]);
}

static const char *const builtin_names[] = {"help", "clear", "echo", "history"};

static uint8_t shell_is_separator(char ch)
{
    return ((ch == ' ') || (ch == '\t')) ? 1U : 0U;
}

static uint16_t shell_add_command_candidates(yd_shell_t *shell,
                                              const char **candidates,
                                              uint16_t count)
{
    uint16_t index;

    for (index = 0U;
         (index < (uint16_t)(sizeof(builtin_names) / sizeof(builtin_names[0]))) &&
         (count < YD_SHELL_COMPLETION_MAX);
         index++) {
        candidates[count++] = builtin_names[index];
    }
    for (index = 0U;
         (index < shell->command_count) && (count < YD_SHELL_COMPLETION_MAX);
         index++) {
        candidates[count++] = shell->commands[index].name;
    }
    return count;
}

static uint16_t shell_completion_context(yd_shell_t *shell,
                                          uint16_t *token_start,
                                          uint8_t *argument_index,
                                          char *command_name)
{
    uint16_t cursor = shell->length;
    uint16_t index = 0U;
    uint16_t name_length = 0U;
    uint8_t token = 0U;

    while ((cursor > 0U) && !shell_is_separator(shell->line[cursor - 1U])) {
        cursor--;
    }
    *token_start = cursor;

    while ((index < cursor) && shell_is_separator(shell->line[index])) {
        index++;
    }
    while ((index < cursor) && !shell_is_separator(shell->line[index])) {
        if (name_length < (YD_SHELL_LINE_SIZE - 1U)) {
            command_name[name_length++] = shell->line[index];
        }
        index++;
    }
    command_name[name_length] = '\0';

    index = 0U;
    while (index < cursor) {
        while ((index < cursor) && shell_is_separator(shell->line[index])) {
            index++;
        }
        if (index >= cursor) {
            break;
        }
        token++;
        while ((index < cursor) && !shell_is_separator(shell->line[index])) {
            index++;
        }
    }
    *argument_index = token;
    return (uint16_t)(shell->length - cursor);
}

static uint16_t shell_collect_candidates(yd_shell_t *shell,
                                          uint8_t argument_index,
                                          const char *command_name,
                                          const char *prefix,
                                          const char **candidates)
{
    const char *raw[YD_SHELL_COMPLETION_MAX];
    uint16_t raw_count = 0U;
    uint16_t count = 0U;
    uint16_t index;
    size_t prefix_length = strlen(prefix);

    if (argument_index == 0U) {
        raw_count = shell_add_command_candidates(shell, raw, raw_count);
    } else if ((argument_index == 1U) && (strcmp(command_name, "help") == 0)) {
        raw_count = shell_add_command_candidates(shell, raw, raw_count);
    } else {
        for (index = 0U; index < shell->command_count; index++) {
            if ((strcmp(command_name, shell->commands[index].name) == 0) &&
                (shell->commands[index].complete != NULL)) {
                raw_count = shell->commands[index].complete(
                    argument_index, prefix, raw, YD_SHELL_COMPLETION_MAX);
                if (raw_count > YD_SHELL_COMPLETION_MAX) {
                    raw_count = YD_SHELL_COMPLETION_MAX;
                }
                break;
            }
        }
    }

    for (index = 0U; index < raw_count; index++) {
        if ((raw[index] != NULL) &&
            (strncmp(raw[index], prefix, prefix_length) == 0)) {
            candidates[count++] = raw[index];
        }
    }
    return count;
}

static size_t shell_common_prefix(const char **candidates, uint16_t count)
{
    size_t length = strlen(candidates[0]);
    uint16_t index;

    for (index = 1U; index < count; index++) {
        size_t cursor = 0U;
        while ((cursor < length) && (candidates[0][cursor] != '\0') &&
               (candidates[index][cursor] == candidates[0][cursor])) {
            cursor++;
        }
        length = cursor;
    }
    return length;
}

static void shell_append_completion(yd_shell_t *shell,
                                    const char *text,
                                    size_t from,
                                    size_t to)
{
    while ((from < to) && (shell->length < (YD_SHELL_LINE_SIZE - 1U))) {
        shell->line[shell->length++] = text[from];
        shell_write_char(shell, text[from]);
        from++;
    }
    shell->line[shell->length] = '\0';
}

static void shell_list_candidates(yd_shell_t *shell,
                                  const char **candidates,
                                  uint16_t count)
{
    uint16_t index;

    yd_shell_puts(shell, "\r\n");
    for (index = 0U; index < count; index++) {
        yd_shell_printf(shell, "%-16s", candidates[index]);
        if (((index + 1U) % 4U) == 0U) {
            yd_shell_puts(shell, "\r\n");
        }
    }
    if ((count % 4U) != 0U) {
        yd_shell_puts(shell, "\r\n");
    }
    yd_shell_prompt(shell);
    shell->write((const uint8_t *)shell->line, shell->length);
}

static void shell_complete(yd_shell_t *shell)
{
    const char *candidates[YD_SHELL_COMPLETION_MAX];
    char command_name[YD_SHELL_LINE_SIZE];
    uint16_t token_start;
    uint16_t prefix_length;
    uint16_t count;
    uint8_t argument_index;
    size_t common_length;
    const char *prefix;

    prefix_length = shell_completion_context(shell, &token_start,
                                               &argument_index, command_name);
    prefix = &shell->line[token_start];
    count = shell_collect_candidates(shell, argument_index, command_name,
                                     prefix, candidates);
    if (count == 0U) {
        shell_write_char(shell, '\a');
        shell->tab_pending = 0U;
        return;
    }

    if (count == 1U) {
        common_length = strlen(candidates[0]);
        if (common_length > prefix_length) {
            shell_append_completion(shell, candidates[0], prefix_length, common_length);
        }
        if ((shell->length < (YD_SHELL_LINE_SIZE - 1U)) &&
            ((shell->length == 0U) || (shell->line[shell->length - 1U] != ' '))) {
            shell->line[shell->length++] = ' ';
            shell->line[shell->length] = '\0';
            shell_write_char(shell, ' ');
        }
        shell->tab_pending = 0U;
        return;
    }

    common_length = shell_common_prefix(candidates, count);
    if (common_length > prefix_length) {
        shell_append_completion(shell, candidates[0], prefix_length, common_length);
        shell->tab_pending = 0U;
        return;
    }

    if (shell->tab_pending != 0U) {
        shell_list_candidates(shell, candidates, count);
        shell->tab_pending = 0U;
    } else {
        shell_write_char(shell, '\a');
        shell->tab_pending = 1U;
    }
}

static void shell_replace_line(yd_shell_t *shell, const char *line)
{
    while (shell->length > 0U) {
        yd_shell_puts(shell, "\b \b");
        shell->length--;
    }
    strncpy(shell->line, line, YD_SHELL_LINE_SIZE - 1U);
    shell->line[YD_SHELL_LINE_SIZE - 1U] = '\0';
    shell->length = (uint16_t)strlen(shell->line);
    shell->write((const uint8_t *)shell->line, shell->length);
}

static void shell_history_move(yd_shell_t *shell, uint8_t older)
{
    if (older != 0U) {
        if (shell->history_index < shell->history_count) {
            shell->history_index++;
        }
    } else if (shell->history_index > 0U) {
        shell->history_index--;
    }

    if (shell->history_index == 0U) {
        shell_replace_line(shell, "");
    } else {
        shell_replace_line(shell,
                           shell->history[shell->history_count - shell->history_index]);
    }
}

void yd_shell_poll(yd_shell_t *shell)
{
    uint8_t ch;

    while (shell->read(&ch) != 0) {
        if (shell->escape_state == 1U) {
            shell->tab_pending = 0U;
            shell->escape_state = (ch == '[') ? 2U : 0U;
            continue;
        }
        if (shell->escape_state == 2U) {
            shell->tab_pending = 0U;
            if (ch == 'A') {
                shell_history_move(shell, 1U);
            } else if (ch == 'B') {
                shell_history_move(shell, 0U);
            }
            shell->escape_state = 0U;
            continue;
        }
        if (ch == 0x1bU) {
            shell->tab_pending = 0U;
            shell->escape_state = 1U;
            continue;
        }
        if ((ch == '\r') || (ch == '\n')) {
            shell->tab_pending = 0U;
            if ((ch == '\n') && (shell->ignore_lf != 0U)) {
                shell->ignore_lf = 0U;
                continue;
            }
            shell->ignore_lf = (ch == '\r') ? 1U : 0U;
            yd_shell_puts(shell, "\r\n");
            shell_execute(shell);
            shell->length = 0U;
            shell->line[0] = '\0';
            yd_shell_prompt(shell);
        } else if ((ch == 0x08U) || (ch == 0x7fU)) {
            shell->tab_pending = 0U;
            if (shell->length > 0U) {
                shell->length--;
                shell->line[shell->length] = '\0';
                yd_shell_puts(shell, "\b \b");
            }
        } else if (ch == 0x03U) {
            shell->tab_pending = 0U;
            shell->length = 0U;
            shell->line[0] = '\0';
            yd_shell_puts(shell, "^C\r\n");
            yd_shell_prompt(shell);
        } else if (ch == 0x0cU) {
            shell->tab_pending = 0U;
            yd_shell_puts(shell, "\033[2J\033[H");
            yd_shell_prompt(shell);
            shell->write((const uint8_t *)shell->line, shell->length);
        } else if (ch == '\t') {
            shell_complete(shell);
        } else if ((ch >= 0x20U) && (ch <= 0x7eU)) {
            shell->tab_pending = 0U;
            if (shell->length < (YD_SHELL_LINE_SIZE - 1U)) {
                shell->history_index = 0U;
                shell->line[shell->length++] = (char)ch;
                shell->line[shell->length] = '\0';
                shell_write_char(shell, (char)ch);
            } else {
                shell_write_char(shell, '\a');
            }
        }
    }
}
