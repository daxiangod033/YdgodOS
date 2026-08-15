/**
 * @file    task.h
 * @brief    Ydgod OS 任务管理模块
 * @target  STM32F103C8
 * @author  Y.d
 */
#ifndef _TASK_H_
#define _TASK_H_

#include <stddef.h> // 定义 NULL

#include "tick.h"


// 任务数
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
    uint32_t id;//句柄
} Task;



/* 保存 CM3 的全部寄存器：
 * r0..r12, sp(r13), lr(r14), pc(r15), xPSR */
typedef struct {
    uint32_t r0, r1,  r2,  r3;
    uint32_t r4, r5,  r6,  r7;
    uint32_t r8, r9, r10, r11;
    uint32_t r12;       /* ip  */
    uint32_t sp;        /* r13 */
    uint32_t lr;        /* r14 */
    uint32_t pc;        /* r15 */
    uint32_t xpsr;      /*  PSR */
} CPU_Context;

/* 保存当前 CPU 全部寄存器到 *ctx */
void CPU_ContextSave(CPU_Context *ctx);

/* 从 *ctx 恢复全部 CPU 寄存器 */
void CPU_ContextRestore(CPU_Context *ctx) __attribute__((noreturn));



// 任务表
extern Task tasks[];


// 创建任务
void createTask(void *_fun, char _name[], uint32_t _reload);

// 启动调度器
void startScheduler();

// 任务延时
void task_delay(uint32_t tick);

// 获取当前任务句柄
uint32_t get_task_id();

#endif // !_TASK_H_