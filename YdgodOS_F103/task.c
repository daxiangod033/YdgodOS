/**
 * @file    task.c
 * @brief    Ydgod OS 任务模块
 * @target  STM32F103C8
 * @author  Y.d
 */

#include "task.h"

// 任务数组
Task tasks[MAX_TASKS];
// 任务数量
int task_num = 0;
// 当前任务索引
int t = 0;

void createTask(void *_fun, char _name[], uint32_t _reload)
{
    Task task;
    task.fun = _fun;
    // task.frequency = _frequency;
    strcpy(task.name, _name);
    // if (_frequency == 0)
    //     return;
    task.reload       = _reload;
    task.run_time     = 0;
    task.state        = WAITING;
    task.tick         = task.reload;
    tasks[task_num++] = task;
}

void startScheduler()
{
    // os_tick_init();

    uint32_t tick = 0;
    if (task_num == 0)
        return;
    while (1) {
        // 任务数组，运行任务
        for (t = 0; t < task_num; t++) {

            // 运行就绪状态任务
            if (tasks[t].state == READY) {
                // 更改成运行状态
                tasks[t].state = RUNNING;
                // 开始计时
                tick = os_get_tick();
                if (tasks[t].fun != NULL)
                    // 任务启动
                    tasks[t].fun();
                // 结束计时
                tasks[t].run_time = os_get_tick() - tick;
                // 防止任务运行时被挂起
                if (tasks[t].state != SUSPENDED) { // 更改成未运行状态
                    tasks[t].state = WAITING;
                }
            }
        }
    }
}














void task_delay(uint32_t tick)
{
    uint32_t task_id = get_task_id();
    tasks[task_id].tick = tick;
}

uint32_t get_task_id()
{
    return t;
}




/* 保存现场 */
void CPU_ContextSave(CPU_Context *ctx)
{
    /* 先把 r0 保存到结构体，再决定要不要写-back */
    __asm__ volatile (
        /* 步骤 1：把 r0 直接存到成员，避免 stmia 里出现 r0! 且含 r0 */
        "str  r0, [%[base], #0]      \n"   /* ctx->r0 = r0 */

        /* 步骤 2：剩下的 r1-r12 用普通 stmia 不带写-back */
        "stm  %[base], {r1-r12}      \n"

        /* 步骤 3：sp、lr、pc、xpsr 用 r1 中转 */
        "mov  r1, sp                 \n"
        "str  r1, [%[base], #13*4]   \n"   /* ctx->sp  */
        "mov  r1, lr                 \n"
        "str  r1, [%[base], #14*4]   \n"   /* ctx->lr  */
        "mov  r1, pc                 \n"
        "add  r1, r1, #4             \n"   /* 返回地址 */
        "str  r1, [%[base], #15*4]   \n"   /* ctx->pc  */
        "mrs  r1, xpsr               \n"
        "str  r1, [%[base], #16*4]   \n"   /* ctx->xpsr */
        :
        : [base] "r" (ctx)
        : "r1", "memory"
    );
}

/* 恢复现场（noreturn） */
void CPU_ContextRestore(CPU_Context *ctx) __attribute__((noreturn));
void CPU_ContextRestore(CPU_Context *ctx)
{
    __asm__ volatile (
        /* 先把 r4-r11 恢复，它们不是特殊寄存器 */
        "ldm  %[base], {r4-r11}      \n"

        /* 把 sp 搬到 r1，再 mov 到 sp（ldm 不能直接加载 sp） */
        "ldr  r1, [%[base], #13*4]   \n"
        "mov  sp, r1                 \n"

        /* 把 lr 搬到 r1，再 mov 到 lr */
        "ldr  r1, [%[base], #14*4]   \n"
        "mov  lr, r1                 \n"

        /* 把 pc 搬到 r1，最后用 bx 跳过去（同时恢复 PSR） */
        "ldr  r1, [%[base], #15*4]   \n"   /* pc 值 */
        "ldr  r0, [%[base], #16*4]   \n"   /* xpsr 值 */
        "msr  xpsr, r0               \n"   /* 恢复 PSR   */
        "bx   r1                     \n"   /* 跳过去，永不返回 */
        :
        : [base] "r" (ctx)
        : "r0", "r1", "memory"
    );
    __builtin_unreachable();
}