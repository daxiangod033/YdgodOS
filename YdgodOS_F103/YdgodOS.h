/**
 * @file    YdgodOS.h
 * @brief   Ydgod 操作系统 1.0
 * @target  STM32F103C8
 * @author  Y.d
 */


#ifndef __YDGODOS_H
#define __YDGODOS_H
#include "stm32f10x.h"

#define TIME_SLICE_1MS (SystemCoreClock / 1000)
#define TIME_SLICE_100US (SystemCoreClock / 10000)

// 时间片模式
#define TIME_SLICE_MODE TIME_SLICE_100US   
// TICK 转换成ms
#define TICK_TO_MS 10


extern int task_num;

// 最大任务数
#define MAX_TASKS 16

enum TaskState
{
    READY, // 就绪态
    RUNNING,   // 运行态
    WAITING,   // 等待态
    SUSPENDED, // 挂起态
};

typedef struct
{
    enum TaskState state;//状态
    void (*fun)(void); ;//任务函数
    char name[16];//任务名字
    uint32_t tick;//任务执行滴答
    uint32_t reload;//重新执行间隔
    uint32_t run_time;//任务执行时间
    uint32_t frequency;//任务频率
    uint32_t id;//句柄
} Task;

extern Task tasks[];

// 初始化系统时钟
uint32_t os_tick_init(void);

// 获取系统时钟
uint32_t os_get_tick(void);

// 延时函数
void os_delay_ms(uint32_t ms);

// 时钟中断处理函数
void os_tick_handler(void);


// 创建任务
void createTask(void *_fun, char _name[], uint32_t _frequency);

// 启动调度器
void startScheduler();
#endif