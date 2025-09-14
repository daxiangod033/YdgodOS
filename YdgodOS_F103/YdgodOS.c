#include "YdgodOS.h"
#include <stddef.h> // 定义 NULL
// 全局系统节拍计数器（毫秒）
volatile uint32_t os_tick = 0;
volatile uint32_t os_tick_ms = 0;


// 任务数组
Task tasks[MAX_TASKS];
// 任务数量
int task_num = 0;

uint32_t os_tick_init(void)
{
    // 使用 CMSIS 函数配置 SysTick
    // SystemCoreClock 应为 72000000 (72MHz)


    if (SysTick_Config(TIME_SLICE_MODE))
    {
        return 1; // 初始化失败
    }

    // 注意：
    // SysTick_Config() 默认使用 HCLK（即 72MHz）作为时钟源
    // 并自动使能计数器和中断

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

void createTask(void *_fun, char _name[], uint32_t _frequency)
{
    Task task;
    task.fun = _fun;
    task.frequency = _frequency;
    strcpy(task.name, _name);
    if (_frequency == 0)
        return;
    task.reload = 1000*TICK_TO_MS /_frequency;
    task.run_time = 0;
    task.state = WAITING;
    task.tick = task.reload;
    tasks[task_num++] = task;
}

void startScheduler()
{
    // os_tick_init();
    int t = 0;
    uint32_t tick = 0;
    if (task_num == 0)
        return;
    while (1)
    {

        for (t = 0; t < task_num; t++) // 任务数组，运行任务
        {
            if (tasks[t].state == READY) // 运行就绪状态任务
            {

                tasks[t].state = RUNNING; // 更改成运行状态
                // 开始计时
                tick = os_get_tick();
                if (tasks[t].fun != NULL) 
                    tasks[t].fun();       // 任务启动
                // 结束计时
                tasks[t].run_time = os_get_tick() - tick;
                if (tasks[t].state != SUSPENDED) // 防止任务运行时被挂起
                {
                    tasks[t].state = WAITING; // 更改成未运行状态
                }
            }
        }
    }
}