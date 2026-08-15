#include "tick.h"

// 全局系统节拍计数器（tick）
volatile uint32_t os_tick = 0;
// 全局系统节拍计数器（毫秒）
volatile uint32_t os_tick_ms = 0;



uint32_t os_tick_init(void)
{
    // 使用 CMSIS 函数配置 SysTick
    // SystemCoreClock 应为 72000000 (72MHz)

    //配置SysTick 每中断一次为 1 tick

    if (SysTick_Config(TIME_SLICE_MODE))
    {
        return 1; // 初始化失败
    }

    // SysTick_Config() 默认使用 HCLK（即 72MHz）作为时钟源

    return 0; // 成功
}

uint32_t os_get_tick(void)
{
    return os_tick;
}

void os_delay_ms(uint32_t ms)
{
    uint32_t start = os_get_tick()/TICK_TO_MS;
    while ((os_get_tick() - start) < ms)
    {
        // 空等待（忙等待）
    }
}

// 函数声明
void os_tick_handler(void)
{
    int t = 0;
    if (task_num == 0)
        return;
    // 遍历整个任务数组，更新任务的计时
    for (t = 0; t < task_num; t++)
    {
        if (tasks[t].state == WAITING) // 判断任务是否被挂起
        {
            if (tasks[t].tick++ == tasks[t].reload) // 是否达到任务间隔
            {
                tasks[t].state = READY; // 任务进入就绪状态
                tasks[t].tick = 0;      // 计时器重载
            }
        }
    }
}

void SysTick_Handler(void)
{
    // 增加系统节拍计数
    os_tick++;

    // 更新任务tick
    os_tick_handler();
}