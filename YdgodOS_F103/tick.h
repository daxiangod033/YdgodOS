/**
 * @file    tick.h
 * @brief    Ydgod OS 心跳模块
 * @target  STM32F103C8
 * @author  Y.d
 */

#ifndef _TICK_H_
#define _TICK_H_

#include "stm32f10x.h"
// 时间片 宏定义
#define TIME_SLICE_1MS   (SystemCoreClock / 1000)

#define TIME_SLICE_100US (SystemCoreClock / 10000)


// 时间片模式
#define TIME_SLICE_MODE TIME_SLICE_100US


// TICK TO ms 转换因子
#if TIME_SLICE_MODE == TIME_SLICE_1MS

#define TICK_TO_MS 1

#elif TIME_SLICE_MODE == TIME_SLICE_100US

#define TICK_TO_MS 10

#endif



#include "task.h"

// 初始化系统时钟
uint32_t os_tick_init(void);

// 获取系统时钟
uint32_t os_get_tick(void);

// 延时函数
void os_delay_ms(uint32_t ms);

// 时钟中断处理函数
void os_tick_handler(void);

#endif // !_TASK_H_