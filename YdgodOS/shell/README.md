# YdgodOS Shell

`yd_shell` 是与硬件和 RTOS 解耦的小型命令行组件。平台只需提供一个非阻塞读字节回调、一个写数据回调和命令表。

当前 STM32F103 示例提供命令解析、行回显、退格、Linux 风格 Tab 补全、历史记录、`Ctrl-C`、`Ctrl-L`、提示符，以及内置的 `help`、`clear`、`echo`、`history` 命令。

Tab 补全规则：

- 唯一匹配时，一次 Tab 补全整个名称并追加空格，例如 `inf<Tab>` 变成 `info `。
- 多个匹配时，先尽可能补到公共前缀。
- 已无更多公共前缀时，连续按两次 Tab 列出匹配候选，随后恢复提示符和当前输入。
- `help <Tab>` 会补全命令名；`led <Tab>` 会补全 `auto/on/off/toggle` 参数。
- 上、下方向键可以浏览最近 4 条命令。

命令通过 `yd_shell_command_t.complete` 注册自己的参数补全回调；没有文件系统时不会尝试补全文件路径。
