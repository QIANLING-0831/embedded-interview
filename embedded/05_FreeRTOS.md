---
title: "FreeRTOS"
source_directory: "05_RTOS"
knowledge_type: embedded_interview_qa
language: zh-CN
tags: [任务调度, 队列, 信号量, 互斥锁, 死锁]
---

# FreeRTOS

本文件将题目与对应答案放在同一个二级标题下。检索命中一个知识单元后，应同时参考题目、理论和答案生成中文口述回答。

## tasks and scheduling：核心理论与上下文

来源：`05_RTOS/01_tasks_and_scheduling.c`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明，不要只复述代码。

### 理论、公共定义与使用说明

```c
/** ===============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：RTOS — 任务、调度和优先级
 * 文件：05_RTOS/01_tasks_and_scheduling.c
 * ===============================================================
 *
 * FreeRTOS是嵌入式面试中最常见的RTOS。
 * 该文件模拟 API 概念，无需
 * 真正的 FreeRTOS 端口。在主机上编译并运行。
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ===============================================================
 * THEORY — RTOS 调度
 * ===============================================================
 *
 * FreeRTOS 中的任务状态：
 * RUNNING — 当前在 CPU 上执行
 * READY — 准备运行，等待调度程序
 * BLOCKED — 等待事件（延迟、信号量、队列）
 * SUSPENDED — 显式挂起 (vTaskSuspend)
 *
 * 调度程序类型：
 * 抢占式：高优先级任务立即抢占
 * 较低优先级任务（默认 FreeRTOS）
 * 协作式：任务运行直到产生（无抢占）
 * 时间分片：同等优先级的任务以循环方式共享时间
 *
 * 优先级：数字越大 = FreeRTOS 中优先级越高
 *（与 OSEK/AUTOSAR 等其他 RTOS 相反！）
 *
 * 堆栈：每个任务都有自己的堆栈。
 * FreeRTOS 堆栈大小以 WORDS 为单位（32 位上每个 4 字节）
 * configMINIMAL_STACK_SIZE 通常为 128 个字 = 512 个字节
 *
 * Tick：RTOS的Tick是周期性中断（ARM上的SysTick）
 * configTICK_RATE_HZ = 1000 → 每滴答 1 毫秒
 * vTaskDelay(100) = 延迟 100 个刻度 = 100 毫秒
 * pdMS_TO_TICKS(250) 将毫秒转换为可移植的刻度
 *
 * 上下文切换：
 * 在 Cortex-M 上：PendSV 处理程序保存/恢复 R4-R11
 * CPU硬件自动保存R0-R3、R12、LR、PC、xPSR
 * =============================================================== */

/* ===============================================================
 * SIMULATED FreeRTOS API（用于主机端练习）
 * =============================================================== */

typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef uint32_t BaseType_t;
typedef uint32_t UBaseType_t;

#define pdTRUE   1u
#define pdFALSE  0u
#define pdPASS   1u
#define pdFAIL   0u
#define portMAX_DELAY  0xFFFFFFFFu

#define configTICK_RATE_HZ    1000u
#define pdMS_TO_TICKS(ms)     ((TickType_t)((ms) * configTICK_RATE_HZ / 1000u))

/* 模拟任务创建（主机上没有真正的调度） */
static int g_task_count = 0;

BaseType_t xTaskCreate_sim(void (*task_fn)(void *),
                           const char *name,
                           uint16_t stack_words,
                           void *param,
                           UBaseType_t priority,
                           TaskHandle_t *handle)
{
    printf("[RTOS] Task created: %s (stack=%u words, priority=%u)\n",
           name, stack_words, (unsigned)priority);
    g_task_count++;
    if (handle) *handle = (void *)(uintptr_t)g_task_count;
    (void)task_fn; (void)param;
    return pdPASS;
}

/* ===============================================================
```

### 答案文件公共定义

答案来源：`05_RTOS/answers/01_tasks_and_scheduling_answers.c`

```c
/*
 * ANSWERS: 05_RTOS/01_tasks_and_scheduling.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: FreeRTOS preemptive scheduling — how does it work?

A: FreeRTOS uses a fixed-priority preemptive scheduler with round-robin
   for tasks of equal priority.
   On every SysTick interrupt (configTICK_RATE_HZ, typically 1 kHz):
   1. SysTick_Handler calls xPortSysTickHandler().
   2. The tick count increments; any task with a matching wake time is
      moved from blocked → ready state.
   3. If a higher-priority task is now ready, PendSV is pended.
   4. PendSV fires (lowest priority, always deferred) and does the context
      switch: saves current task's context (R4-R11 + PSP), loads next task's.
   Rule: the HIGHEST priority ready task ALWAYS runs.
   If two tasks have the same priority: time-sliced round-robin, each runs
   for one tick time slice then yields to the other.

Q2: vTaskDelay vs vTaskDelayUntil. When to prefer each?

A: vTaskDelay(n): blocks for n ticks FROM THE TIME OF THE CALL.
   If the task was woken up 5 ms late (e.g., a higher priority task ran),
   it then delays n ticks → actual period becomes n + jitter. Periods drift.

   vTaskDelayUntil(&xLastWakeTime, n): blocks until the ABSOLUTE wake time
   (xLastWakeTime + n). The scheduler sets xLastWakeTime on wake-up.
   Even if the task woke up 5 ms late this cycle, the next cycle starts
   at the correct absolute time. No drift accumulation.

   Use vTaskDelay: tasks that don't need precise timing (UI, logging).
   Use vTaskDelayUntil: periodic control loops, sensor sampling, PID,
   any task where jitter accumulation matters.

Q3: Stack size of 128 words — why words, not bytes?

A: "Words" = 32-bit values on Cortex-M. 128 words = 512 bytes.
   FreeRTOS stack depth is specified in words because the stack stores
   register values (32-bit) and local variables which are naturally aligned.
   Stack size in bytes = depth × sizeof(StackType_t) = depth × 4.
   Minimum stack = ISR frame (8 regs × 4B = 32B) + task prologue registers
   (R4-R11 × 4B = 32B) + local variables.
   Rule: measure with uxTaskGetStackHighWaterMark(). If < 20 words → increase.

Q4: Priority inversion — concrete example and two solutions.

A: Scenario:
   TaskL (priority 1) takes mutex M → runs.
   TaskH (priority 3) becomes ready → preempts TaskL → tries to take M → BLOCKS.
   TaskM (priority 2) becomes ready → preempts TaskL (higher priority than L) → runs.
   TaskH STARVES because TaskM prevents TaskL from releasing M.

   Solution 1 — Priority Inheritance (xSemaphoreCreateMutex):
   When TaskH blocks on M, TaskL's priority is temporarily raised to TaskH's
   priority (3). TaskM cannot preempt TaskL anymore. TaskL finishes, releases M,
   priority returns to 1, TaskH runs.

   Solution 2 — Priority Ceiling Protocol:
   Mutex M has a predefined "ceiling priority" = max priority of any task that
   might take it. When any task takes M, it runs at the ceiling priority.
   Prevents inversion even without knowing which high-priority task will contend.
   More predictable but requires knowing all mutex users at design time.

Q5: How many tasks can run simultaneously on a single-core MCU? Why?

A: Exactly ONE task runs at any given instant (single core = single pipeline).
   The RTOS creates the ILLUSION of concurrency by rapidly switching tasks
   (context switching, typically every 1 ms = SysTick interval).
   An ISR can interrupt the running task, but it's still one execution thread.
   Total CREATED tasks: limited by RAM (each needs its stack + TCB ~100 bytes).
   At 128 KB SRAM: ~100 tasks of 512-byte stacks + overhead = 50-60 tasks max.
   But in practice: 5-15 tasks is typical for embedded systems.

Q6: Task won't start after xTaskCreate. Debug approach.

A: 1. Check return value: if xTaskCreate returns pdFAIL, RAM is too small for
      the stack + TCB. Reduce stack size or free other memory.
   2. Check vTaskStartScheduler was called. Tasks don't run until scheduler starts.
   3. Check priority: if priority=0 (same as Idle task), the task may not run
      if Idle never yields (configUSE_IDLE_HOOK=1 with infinite loop in hook).
   4. Check stack overflow: if another task overflowed its stack, heap/TCB
      corruption can prevent task from running. Use configCHECK_FOR_STACK_OVERFLOW=2.
   5. Check for an assertion/hardfault in vTaskStartScheduler: insufficient heap
      for idle task stack, or configMINIMAL_STACK_SIZE too small.
   6. Add trace: configUSE_TRACE_FACILITY=1, then use Segger SystemView or
      similar to see task state transitions.
*/

/* ============================================================
 * Simulated FreeRTOS types for compilation without RTOS
 * ============================================================ */

typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void *);

typedef struct {
    char     name[16];
    uint32_t priority;
    uint32_t stack_depth_words;
    uint8_t  running;
} SimTask;

#define MAX_TASKS  8
static SimTask g_tasks[MAX_TASKS];
static uint8_t g_task_count = 0;
static uint8_t g_scheduler_started = 0;

/* ============================================================
```

---

## tasks and scheduling：任务 1

来源：`05_RTOS/01_tasks_and_scheduling.c`；任务编号：`1`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 1 — 任务创建和生命周期
 *
 * 创建两个任务：一个 LED 信号灯，一个传感器读取器。
 * 传感器任务具有更高的优先级。
 * =============================================================== */

volatile uint8_t g_led_state   = 0;
volatile uint8_t g_sensor_data = 0;

/* LED 闪烁任务 — 以低优先级运行，500ms 闪烁 */
void led_task(void *param)
{
    /* TODO：在实际系统中，无限循环：
     * 而 (1) {
     * gpio_toggle_pin(GPIOA, 5);
     * vTaskDelay(pdMS_TO_TICKS(500));
     * }
     * 在主机上：只需切换一次标志 */
    (void)param;
    g_led_state ^= 1;
    printf("LED task: LED %s\n", g_led_state ? "ON" : "OFF");
}

/* 传感器读取器任务 — 每个 100ms 以高优先级运行*/
void sensor_task(void *param)
{
    /*TODO：在真实系统中：
     * 而 (1) {
     * g_sensor_data = read_adc_channel(0);
     * vTaskDelay(pdMS_TO_TICKS(100));
     * }
     * 在主机上：模拟读数 */
    (void)param;
    g_sensor_data = 42;
    printf("Sensor task: data = %u\n", g_sensor_data);
}

void create_tasks(void)
{
    /* TODO：创建优先级为1的led_task，堆栈256个字 */
    /* TODO：创建优先级为3的sensor_task，堆栈512个字 */
    /* TODO：启动调度器：vTaskStartScheduler() */
    TaskHandle_t led_handle, sensor_handle;

    xTaskCreate_sim(led_task, "LED", 256, NULL, 1, &led_handle);
    xTaskCreate_sim(sensor_task, "Sensor", 512, NULL, 3, &sensor_handle);
    (void)led_handle; (void)sensor_handle;
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 1 — xTaskCreate simulation
 * ============================================================ */

int xTaskCreate_sim(TaskFunction_t fn, const char *name,
                    uint32_t stack_depth, void *param,
                    uint32_t priority, void **handle)
{
    (void)fn; (void)param; (void)handle;

    /* Validate parameters (catches common bugs) */
    if (priority == 0)        { printf("WARN: priority 0 = Idle priority\n"); }
    if (stack_depth < 64)     { printf("WARN: stack too small (%u words)\n", stack_depth); }
    if (g_task_count >= MAX_TASKS) return 0;  /* pdFAIL */

    SimTask *t = &g_tasks[g_task_count++];
    strncpy(t->name, name, 15);
    t->priority = priority;
    t->stack_depth_words = stack_depth;
    t->running = 0;
    return 1;  /* pdPASS */
}

void vTaskStartScheduler_sim(void)
{
    g_scheduler_started = 1;
    /* Find highest priority task */
    uint32_t max_pri = 0;
    for (int i = 0; i < g_task_count; i++)
        if (g_tasks[i].priority > max_pri) max_pri = g_tasks[i].priority;
    for (int i = 0; i < g_task_count; i++)
        if (g_tasks[i].priority == max_pri) g_tasks[i].running = 1;
}

/* ============================================================
```

---

## tasks and scheduling：任务 2

来源：`05_RTOS/01_tasks_and_scheduling.c`；任务编号：`2`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 2 — 优先级反转场景
 *
 * 经典嵌入式面试题：什么是优先级反转？
 * 具有优先级继承的互斥体如何解决这个问题？
 *
 * 场景：
 * 任务H（高优先级）需要互斥体M
 * 任务L（低优先级）持有互斥锁M
 * 任务M（中等优先级）可运行（不需要M）
 *
 * 没有优先级继承：
 * L 运行但 M 抢占 L (M > L) → H 挨饿！
 * H 正在等待 L 持有的 M，但 L 无法运行，因为 M 抢占了它。
 *
 * 具有优先级继承（FreeRTOS 中的 xSemaphoreCreateMutex）：
 * 当 H 阻塞由 L 持有的互斥体时，L 会暂时提升到 H 的优先级。
 * L > M，因此 L 运行并释放互斥体。
 * H 解锁并立即运行。
 * =============================================================== */

void priority_inversion_demo(void)
{
    printf("\n--- Priority Inversion Demo ---\n");
    printf("Task H (prio=3) needs mutex.\n");
    printf("Task L (prio=1) holds mutex, is preempted by Task M (prio=2).\n");
    printf("Result WITHOUT priority inheritance: H starves behind M behind L.\n");
    printf("Result WITH priority inheritance (xSemaphoreCreateMutex):\n");
    printf("  L is boosted to prio=3, completes, H runs. M runs last.\n");
}

/* ===============================================================
```

### 对应参考答案

答案文件中的对应内容位于任务 `3`（原文件任务顺序不同或合并了多个任务）。

```c
* TASK 3 — Priority inversion demo
 * ============================================================ */

typedef struct { uint8_t locked; uint8_t holder_priority; } SimMutex;

void mutex_take_sim(SimMutex *m, uint8_t caller_priority)
{
    if (m->locked) {
        /* Priority inheritance: raise holder to caller's priority if needed */
        if (caller_priority > m->holder_priority)
            m->holder_priority = caller_priority;
        printf("Task pri=%u BLOCKED on mutex (holder raised to %u)\n",
               caller_priority, m->holder_priority);
    } else {
        m->locked = 1;
        m->holder_priority = caller_priority;
    }
}

void mutex_give_sim(SimMutex *m)
{
    m->locked = 0;
    m->holder_priority = 0;
}

/* ============================================================
```

---

## tasks and scheduling：任务 3

来源：`05_RTOS/01_tasks_and_scheduling.c`；任务编号：`3`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 3 — 节拍率和延迟精度
 * =============================================================== */

uint32_t ms_to_ticks(uint32_t ms)
{
    /* TODO：返回 pdMS_TO_TICKS(ms) — 使用宏 */
    return ms * configTICK_RATE_HZ / 1000;
}

uint32_t ticks_to_ms(uint32_t ticks)
{
    /* TODO：返回刻度 * 1000 / configTICK_RATE_HZ */
    return ticks * 1000 / configTICK_RATE_HZ;
}

/* ===============================================================
```

### 对应参考答案

答案文件中的对应内容位于任务 `4`（原文件任务顺序不同或合并了多个任务）。

```c
* TASK 4 — ms_to_ticks / ticks_to_ms
 * ============================================================ */

#define CONFIG_TICK_RATE_HZ  1000u

TickType_t ms_to_ticks(uint32_t ms)
{
    return (TickType_t)((ms * CONFIG_TICK_RATE_HZ) / 1000u);
}

uint32_t ticks_to_ms(TickType_t ticks)
{
    return (uint32_t)((ticks * 1000u) / CONFIG_TICK_RATE_HZ);
}

/* ============================================================
```

---

## tasks and scheduling：任务 4

来源：`05_RTOS/01_tasks_and_scheduling.c`；任务编号：`4`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 4 — 堆栈大小估计
 *
 * 常见面试问题：如何选择任务堆栈大小？
 * 经验法则：
 * - 计算局部变量、函数调用深度、字符串缓冲区
 * - FreeRTOS uxTaskGetStackHighWaterMark() 报告未使用的字
 * - 从512个字开始，测量后调低
 * - 始终添加 20% 的安全裕度
 * - 在 Cortex-M 上：每个上下文保存 = 16 个寄存器 × 4 字节 = 64 字节
 * =============================================================== */

uint32_t estimate_task_stack_bytes(uint32_t max_local_vars_bytes,
                                   uint8_t max_call_depth,
                                   uint32_t interrupt_context_bytes)
{
    /* TODO： 粗略估计：
     *frame_per_call = max_local_vars_bytes / max_call_深度（平均）
     * 总计 = 每次调用的帧数 * 最大调用深度
     * + Interrupt_context_bytes（保存在 ISR 条目上的上下文：8 个寄存器 = 32 个字节）
     * + 20% 安全性
     * 四舍五入到最接近的 2 次方（常见做法）
     * 返回BYTES（除以4以获得FreeRTOS堆栈参数的字） */
    (void)max_local_vars_bytes; (void)max_call_depth; (void)interrupt_context_bytes;
    return 512;   /* 占位符*/
}

/*===============================================================
```

### 对应参考答案

选择任务栈大小时，先统计局部变量、函数调用深度、中断嵌套、库函数以及上下文保存开销，再留出安全余量。运行阶段使用 `uxTaskGetStackHighWaterMark()` 或栈染色观察最小剩余空间，覆盖最坏输入和最长调用路径后逐步收敛。高水位只反映测试覆盖到的历史最小值，不能替代最坏情况分析。

---

## tasks and scheduling：任务 5

来源：`05_RTOS/01_tasks_and_scheduling.c`；任务编号：`5`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 5 — vTaskDelayUntil 模式（固定频率循环）
 *
 * vTaskDelay(N) = 从 NOW 延迟 N 个时钟周期
 * vTaskDelayUntil(&last_wake, N) = 延迟直到 LAST WAKE 的 N 个时钟周期
 *
 * 使用 vTaskDelayUntil 执行精确的周期性任务。
 * vTaskDelay 漂移是因为排除了处理时间。
 * =============================================================== */

void periodic_task_correct(void *param)
{
    /* 模拟 — 显示模式 */
    TickType_t last_wake = 0;   /* 真实代码：开始时为 xTaskGetTickCount() */
    const TickType_t period = pdMS_TO_TICKS(10);   /* 10ms期间 */

    for (int i = 0; i < 3; i++) {
        /* TODO：在真实硬件上：vTaskDelayUntil（&last_wake，句点） */
        last_wake += period;   /* 模拟 */
        printf("Periodic task tick at t=%u ms\n", (unsigned)ticks_to_ms(last_wake));
    }
    (void)param;
}

/* ===============================================================
```

### 对应参考答案

答案文件中的对应内容位于任务 `2`（原文件任务顺序不同或合并了多个任务）。

```c
* TASK 2 — vTaskDelayUntil pattern
 * ============================================================ */

static TickType_t g_simulated_tick = 0;

TickType_t xTaskGetTickCount_sim(void) { return g_simulated_tick; }

/* Correct periodic task pattern */
void periodic_sensor_task_sim(uint32_t period_ticks)
{
    TickType_t xLastWakeTime = xTaskGetTickCount_sim();
    for (int i = 0; i < 5; i++) {
        /* vTaskDelayUntil(&xLastWakeTime, period_ticks) would go here */
        xLastWakeTime += period_ticks;  /* simulate the absolute-time advance */
        /* do work... */
    }
}

/* ============================================================
```

---

## tasks and scheduling：任务 6

来源：`05_RTOS/01_tasks_and_scheduling.c`；任务编号：`6`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 6 — BUG HUNT：任务创建和调度错误
 *
 * 下面的代码错误地创建了任务。
 * 发现 3 个错误。
 * =============================================================== */

void worker_task_fn(void *p) { (void)p; }

void setup_tasks_BUGGY(void)
{
    TaskHandle_t h;

    /* Bug 1：32 个字（128 字节）的堆栈大小太小。
     * FreeRTOS 开销 + ISR 上下文 = 最少约 100 字节。
     * 任何函数调用都会溢出。建议最少：128 个字。 */
    xTaskCreate_sim(worker_task_fn, "Worker", 32, NULL, 2, &h);

    /* Bug 2：priority=0是空闲任务优先级。
     * 真正的任务应该有优先级 >= 1，否则它
     * 与空闲任务竞争（并且可能导致闲置任务挨饿），
     * 防止堆清理和堆栈水印检查。 */
    xTaskCreate_sim(worker_task_fn, "Worker2", 256, NULL, 0, &h);

    /* Bug 3：vTaskStartScheduler() 未调用 - 任务已创建但从未运行。
     * 创建所有任务后，MUST 调用vTaskStartScheduler()。
     * 此调用之后的代码永远不会执行（调度程序接管）。 */
}

/* ===============================================================
 * SELF-TEST
 * =============================================================== */

int main(void)
{
    create_tasks();
    led_task(NULL);
    sensor_task(NULL);
    priority_inversion_demo();
    periodic_task_correct(NULL);

    assert(ms_to_ticks(500)  == 500);
    assert(ticks_to_ms(1000) == 1000);

    printf("\nAll RTOS task tests PASSED.\n");
    return 0;
}

/* ===============================================================
 * INTERVIEW QUESTIONS
 * ===============================================================
 *
 * Q1: vTaskDelay 和 vTaskDelayUntil 有什么区别？
 * 什么时候重要？
 * 答案：TODO
 *
 * Q2：解释一下优先级反转。什么 RTOS 原语可以阻止它？
 * 答案：TODO
 *
 * Q3：在启用抢占的 FreeRTOS 上，两个任务具有相同的优先级：
 * 其中一个是否抢占另一个？是什么配置了这种行为？
 * 答案：TODO
 *
 * Q4：任务的堆栈高水位标记为 8 个字。这安全吗？
 * 你对此有何反应？
 * 答案：TODO
 *
 * Q5: 如果你从不调用堆和空闲任务会发生什么
 * vTaskStartScheduler()？
 * 答案：TODO
 *
 * Q6: 您的系统有 5 个任务。最高优先级任务在每个 1ms 上运行。
 * 绘制一条时间线，显示优先级较低的任务如何获得 CPU 时间。
 * 答案：TODO*/
```

### 对应参考答案

答案文件中的对应内容位于任务 `5`（原文件任务顺序不同或合并了多个任务）。

```c
* TASK 5 — Bug hunt analysis
 *
 * Bug 1: Stack size = 32 words = 128 bytes.
 *        FreeRTOS minimum is configMINIMAL_STACK_SIZE (typically 128 words).
 *        Even an empty task needs ~40 words for context save.
 *        Result: stack overflow on first function call → HardFault.
 *        FIX: increase to at least 128 words (512 bytes) per task.
 *
 * Bug 2: Priority = 0 = same as Idle task.
 *        In FreeRTOS, idle task runs at priority 0.
 *        Tasks at priority 0 time-share with idle. If idle hook has work
 *        or configIDLE_SHOULD_YIELD=0, user task may barely run.
 *        FIX: user tasks should use priority >= 1. Reserve 0 for idle.
 *
 * Bug 3: vTaskStartScheduler() not called.
 *        xTaskCreate creates tasks in suspended state; they only run
 *        once the scheduler starts. Without vTaskStartScheduler():
 *        tasks never execute, infinite loop at the end of main().
 *        FIX: add vTaskStartScheduler() after all xTaskCreate() calls.
 *        Note: vTaskStartScheduler() should never return. If it does:
 *        heap was too small for the idle task stack.
 * ============================================================ */

int main(void)
{
    /* Task creation tests */
    int r1 = xTaskCreate_sim(NULL, "LED_Task",    128, NULL, 2, NULL);
    int r2 = xTaskCreate_sim(NULL, "Sensor_Task", 256, NULL, 3, NULL);
    assert(r1 == 1 && r2 == 1);
    assert(g_task_count == 2);

    vTaskStartScheduler_sim();
    assert(g_scheduler_started);
    /* Sensor_Task has higher priority → should run first */
    assert(g_tasks[1].running == 1);

    /* ms_to_ticks tests */
    assert(ms_to_ticks(1000) == 1000);
    assert(ms_to_ticks(100)  == 100);
    assert(ticks_to_ms(500)  == 500);

    /* Priority inversion demo */
    SimMutex m = {0};
    mutex_take_sim(&m, 1);  /* Low priority task takes mutex */
    assert(m.locked && m.holder_priority == 1);
    mutex_take_sim(&m, 3);  /* High priority task blocked → raises holder */
    assert(m.holder_priority == 3);  /* priority inherited */
    mutex_give_sim(&m);
    assert(!m.locked);

    printf("All RTOS task/scheduling answers verified.\n");
    return 0;
}
```

---

## semaphores mutexes：核心理论与上下文

来源：`05_RTOS/02_semaphores_mutexes.c`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明，不要只复述代码。

### 理论、公共定义与使用说明

```c
/** ===============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：RTOS — 信号量、互斥体和队列
 * 文件：05_RTOS/02_semaphores_mutexes.c
 * ===============================================================
 *
 * MUST 所了解的 THREE 同步原语：
 * 1. 二进制信号量 — ISR 到任务的信号量
 * 2. Mutex——具有优先级继承的互斥
 * 3.队列——任务之间的数据传输
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ===============================================================
 * THEORY — 何时使用什么
 * ===============================================================
 *
 * 二进制信号量：
 * - 发出事件信号（例如，ISR 告诉任务数据已准备好）
 * - 类似于标志，但 RTOS 感知（任务块而不是轮询）
 * - 可以从 ISR 给出：xSemaphoreGiveFromISR()
 * - 无优先级继承 — NOT 资源锁定安全
 *
 * 计数信号量：
 * - 跟踪 N 个可用资源（例如，具有 4 个插槽的缓冲池）
 * - xSemaphoreCreateCounting（最大，初始）
 *
 * 互斥体：
 * - 保护共享资源（SPI总线、UART、全局变量）
 * - HAS优先级继承（防止优先级反转）
 * - 必须由 SAME 任务获取和给出
 * - NEVER 从 ISR 给出（使用二进制信号量进行 ISR 同步）
 * - xSemaphoreCreateMutex()
 *
 * 递归互斥体：
 * - 同一个任务可以多次执行而不会出现死锁
 * - 必须给出相同的次数
 * - xSemaphoreCreateRecursiveMutex()
 *
 * 队列：
 * - 在任务之间传递数据或从 ISR 到任务
 * - 提供AND两者同步数据传输
 * - xQueueCreate(长度, item_size)
 * - xQueueSend() / xQueueReceive()
 * - xQueueSendFromISR() / xQueueReceiveFromISR()
 * =============================================================== */

/* 模拟 FreeRTOS 类型 */
typedef uint32_t TickType_t;
typedef uint32_t BaseType_t;
#define pdTRUE   1u
#define pdFALSE  0u
#define pdPASS   1u
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) (ms)

/* ===============================================================
 * SIMULATED SEMAPHORE AND QUEUE（主机端练习）
 * =============================================================== */

typedef struct {
    volatile uint32_t count;
    uint32_t max_count;
} SimSemaphore;

typedef struct {
    uint8_t  *buf;
    uint16_t  item_size;
    uint16_t  length;
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} SimQueue;

static SimSemaphore g_sem  = {0};
static SimSemaphore g_mutex = {1, 1};   /* 互斥量从 1 开始（可用） */

BaseType_t sem_give(SimSemaphore *s)
{
    if (s->count >= s->max_count) return pdFALSE;
    s->count++;
    return pdTRUE;
}

BaseType_t sem_take(SimSemaphore *s, TickType_t timeout_ticks)
{
    if (s->count > 0) { s->count--; return pdTRUE; }
    /* 实际中 RTOS：在这里阻塞 timeout_ticks */
    (void)timeout_ticks;
    return pdFALSE;   /* 真实情况下会阻塞 RTOS */
}

/* ===============================================================
```

### 答案文件公共定义

答案来源：`05_RTOS/answers/02_semaphores_mutexes_answers.c`

```c
/*
 * ANSWERS: 05_RTOS/02_semaphores_mutexes.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: Binary semaphore vs mutex — key differences.

A: Binary semaphore:
   - Signaling mechanism. Used for ISR→task or task→task notification.
   - No ownership: any task (or ISR) can give it.
   - No priority inheritance: if high-priority task waits, lower-priority
     holder is NOT elevated → risk of priority inversion.
   - xSemaphoreGiveFromISR() is allowed.

   Mutex:
   - Mutual exclusion (resource protection).
   - Has OWNERSHIP: the task that takes it must be the one to give it.
   - Priority inheritance built-in: if higher-priority task blocks on
     the mutex, the holder's priority is temporarily raised.
   - xSemaphoreGiveFromISR() is NOT allowed (undefined behavior).
   - Cannot be given from ISR.

   Rule: semaphore for signaling, mutex for shared resource protection.

Q2: Counting semaphore — what it models.

A: A counting semaphore has an integer count [0..MAX].
   xSemaphoreTake: decrements count. Blocks if count == 0.
   xSemaphoreGive: increments count. Unblocks one waiter.
   Models: resource pools.
   Example: 3 SPI buses available. COUNT=3.
   Task takes SPI bus: COUNT→2. Another task: COUNT→1. Third: COUNT→0.
   Fourth task: BLOCKS until one of the first three gives back.
   Also models: event queues (ISR fires multiple times between task runs;
   each fire calls Give; task wakes once per Give).
   Counting semaphore ≠ queue: no data transferred, only count.

Q3: Deadlock — define and give an embedded example.

A: Deadlock: two or more tasks are each waiting for a resource held by
   the other → circular wait → all blocked forever.

   Embedded example:
   Task A:  takes Mutex_SPI, then tries to take Mutex_DMA.
   Task B:  takes Mutex_DMA, then tries to take Mutex_SPI.
   If A takes SPI and B takes DMA simultaneously:
   A blocks waiting for DMA (held by B).
   B blocks waiting for SPI (held by A).
   Neither can proceed. System frozen.

   Prevention — Lock Ordering:
   Globally define: always acquire SPI before DMA.
   Both Task A and B: take SPI first, then DMA.
   B must try to take SPI first → sees A holds it → blocks → A can
   take DMA → completes → gives both → B runs.

Q4: Can a mutex be given from an ISR in FreeRTOS?

A: No. xSemaphoreTake() and xSemaphoreGive() on a mutex may not be
   called from an ISR.
   Reason: Mutex give involves priority inheritance logic — it may need
   to change a task's priority, which is not ISR-safe.
   ISR must use: xSemaphoreGiveFromISR() with a binary or counting semaphore.
   Pattern:
   ISR: xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // trigger context switch
   Task: xSemaphoreTake(sem, portMAX_DELAY);

Q5: Queue vs semaphore for ISR→task data transfer.

A: Semaphore: only signals that something happened. No data.
   If ISR fires faster than task processes, count accumulates but
   the ISR's data may have been overwritten before task reads it.
   Appropriate when: task can re-read the data itself (e.g., read ADC register).

   Queue: transfers the actual data value. ISR copies data into queue entry.
   Task wakes and pops data off. If queue has depth N, up to N events can
   be buffered. If queue is full, xQueueSendFromISR returns pdFALSE (drop).
   Appropriate when: data values matter and must not be lost.

   Rule: for ISR→task with data → use queue.
         for ISR→task signaling only → use binary semaphore.

Q6: What is xHigherPriorityTaskWoken and why must you call portYIELD_FROM_ISR?

A: When an ISR gives a semaphore or sends to a queue, a higher-priority
   task may be unblocked. The ISR cannot switch context immediately
   (it's in interrupt context). Instead:
   1. FreeRTOS sets *pxHigherPriorityTaskWoken = pdTRUE if a higher-priority
      task was unblocked.
   2. At the END of the ISR, portYIELD_FROM_ISR(val) checks val:
      If pdTRUE: pends a PendSV interrupt. When ISR returns, PendSV fires
      and does the context switch → high-priority task runs immediately.
      If pdFALSE: ISR returns normally, no context switch.
   Without portYIELD_FROM_ISR: the unblocked high-priority task won't run
   until the next SysTick (up to 1 ms later) → unnecessary latency.
*/

/* ============================================================
 * Simulated types
 * ============================================================ */

typedef struct {
    volatile int32_t  count;
    int32_t           max_count;
    const char       *name;
} SimSemaphore;

typedef struct {
    uint8_t  buf[64];
    uint8_t  head, tail, count;
    uint8_t  item_size;
    uint8_t  depth;
} SimQueue;

/* ============================================================
```

---

## semaphores mutexes：任务 1

来源：`05_RTOS/02_semaphores_mutexes.c`；任务编号：`1`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 1 — 二进制信号量：ISR 到任务数据就绪信号
 *
 * 模式：ISR 接收 UART 字节，通知任务进行处理。
 * 任务有效地阻塞而不是轮询。
 * =============================================================== */

static volatile uint8_t g_isr_byte = 0;
static SimSemaphore g_data_ready_sem = {0, 1};

void uart_rx_isr_sim(uint8_t byte)
{
    /* TODO：保存字节 */
    /* TODO：在真实的 FreeRTOS 中给出信号量：xSemaphoreGiveFromISR()
     *（包括 BaseType_t xHigherPriorityTaskWoken = pdFALSE 和
     * portYIELD_FROM_ISR(xHigherPriorityTaskWoken) 结束）*/
    g_isr_byte = byte;
    sem_give(&g_data_ready_sem);
    printf("[ISR] UART byte 0x%02X received, semaphore given\n", byte);
}

void uart_process_task(void *param)
{
    /*TODO：永远循环：
     * xSemaphoreTake(sem, portMAX_DELAY) — 阻塞直到 ISR 发出信号
     * 处理 g_isr_byte（实际代码中应在临界区内执行 volatile 读取） */
    (void)param;
    if (sem_take(&g_data_ready_sem, portMAX_DELAY) == pdTRUE) {
        printf("[Task] Processing byte: 0x%02X\n", g_isr_byte);
    }
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 1 — Binary semaphore (ISR → task signaling)
 * ============================================================ */

static SimSemaphore g_uart_rx_sem = {0, 1, "uart_rx"};

void sim_sem_init_binary(SimSemaphore *s) { s->count = 0; s->max_count = 1; }

int sim_sem_give(SimSemaphore *s)
{
    if (s->count >= s->max_count) return 0;  /* would overflow */
    s->count++;
    return 1;
}

int sim_sem_give_from_isr(SimSemaphore *s, uint8_t *higher_prio_woken)
{
    int r = sim_sem_give(s);
    if (r && higher_prio_woken) *higher_prio_woken = 1;
    return r;
}

int sim_sem_take(SimSemaphore *s, uint32_t timeout_ticks)
{
    (void)timeout_ticks;
    if (s->count > 0) { s->count--; return 1; }
    return 0;  /* would block in real RTOS */
}

/* ISR: new UART byte received */
static volatile uint8_t g_uart_byte = 0;
void uart_rx_isr_sim(uint8_t byte)
{
    g_uart_byte = byte;
    uint8_t woken = 0;
    sim_sem_give_from_isr(&g_uart_rx_sem, &woken);
    /* portYIELD_FROM_ISR(woken) — would trigger PendSV on real MCU */
}

/* Task: processes received byte */
void uart_process_task_sim(void)
{
    if (sim_sem_take(&g_uart_rx_sem, 1000)) {
        printf("  Received byte: 0x%02X\n", g_uart_byte);
    }
}

/* ============================================================
```

---

## semaphores mutexes：任务 2

来源：`05_RTOS/02_semaphores_mutexes.c`；任务编号：`2`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 2 — 互斥锁：保护共享 SPI 总线
 *
 * 多个任务共享一个SPI外设。
 * 交易不得交错。
 * =============================================================== */

static SimSemaphore g_spi_mutex = {1, 1};

void spi_write_protected(uint8_t reg, uint8_t val)
{
    /* TODO：xSemaphoreTake（g_spi_mutex，portMAX_DELAY） */
    /* TODO：执行SPI事务（CS断言、写入、CS取消断言） */
    /* TODO：xSemaphoreGive（g_spi_mutex） */
    /* IMPORTANT：即使 SPI 失败，也始终提供互斥锁。
     * 使用 goto 清理或基于范围的模式。 */

    if (sem_take(&g_spi_mutex, portMAX_DELAY) == pdTRUE) {
        printf("[SPI] Writing reg=0x%02X val=0x%02X\n", reg, val);
        sem_give(&g_spi_mutex);
    }
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 2 — Mutex (resource protection)
 * ============================================================ */

typedef struct { uint8_t locked; const char *owner; } SimMutex;

int mutex_take(SimMutex *m, const char *caller)
{
    if (m->locked) return 0;  /* would block in real RTOS */
    m->locked = 1;
    m->owner  = caller;
    return 1;
}

void mutex_give(SimMutex *m, const char *caller)
{
    if (!m->locked || m->owner != caller) {
        printf("ERROR: %s trying to give mutex owned by %s\n", caller, m->owner);
        return;
    }
    m->locked = 0;
    m->owner  = NULL;
}

static SimMutex g_spi_mutex = {0, NULL};
static uint8_t  g_spi_reg_value = 0;

void spi_write_protected(uint8_t reg, uint8_t val)
{
    if (!mutex_take(&g_spi_mutex, "spi_writer")) return;
    g_spi_reg_value = val;  /* critical section */
    mutex_give(&g_spi_mutex, "spi_writer");
}

/* ============================================================
```

---

## semaphores mutexes：任务 3

来源：`05_RTOS/02_semaphores_mutexes.c`；任务编号：`3`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 3 — 队列：在任务之间传递传感器读数
 *
 * 传感器任务在 100Hz 处读取 ADC，填充队列。
 * 记录器任务清空队列并写入闪存。
 * 解耦生产者和消费者之间的时间关系。
 * =============================================================== */

typedef struct {
    uint32_t timestamp_ms;
    uint16_t adc_raw;
    float    voltage;
} SensorSample;

#define QUEUE_LENGTH  16
static SensorSample g_queue_buf[QUEUE_LENGTH];
static SimQueue g_sensor_queue = {
    .buf       = (uint8_t *)g_queue_buf,
    .item_size = sizeof(SensorSample),
    .length    = QUEUE_LENGTH,
    .head      = 0,
    .tail      = 0,
    .count     = 0
};

BaseType_t queue_send(SimQueue *q, const void *item)
{
    /* TODO：如果已满：返回 pdFALSE
     * 将 item 复制到 buf + head * item_size
     * 提前头（按长度缠绕）
     * 增量计数
     * 返回 pdTRUE */
    if (q->count >= q->length) return pdFALSE;
    memcpy(q->buf + q->head * q->item_size, item, q->item_size);
    q->head = (uint16_t)((q->head + 1) % q->length);
    q->count++;
    return pdTRUE;
}

BaseType_t queue_receive(SimQueue *q, void *item, TickType_t timeout)
{
    /* TODO：如果为空：等待超时 — 超时时返回 pdFALSE
     * 复制 buf + tail * item_size 到 item
     * 提前尾部
     * 递减计数 */
    (void)timeout;
    if (q->count == 0) return pdFALSE;
    memcpy(item, q->buf + q->tail * q->item_size, q->item_size);
    q->tail = (uint16_t)((q->tail + 1) % q->length);
    q->count--;
    return pdTRUE;
}

void sensor_producer_task(void *param)
{
    (void)param;
    SensorSample s = {.timestamp_ms = 100, .adc_raw = 2048, .voltage = 3.3f};
    if (queue_send(&g_sensor_queue, &s) == pdTRUE)
        printf("[Sensor] Sample queued: ADC=%u\n", s.adc_raw);
    else
        printf("[Sensor] Queue full — dropping sample!\n");
}

void logger_consumer_task(void *param)
{
    (void)param;
    SensorSample s;
    if (queue_receive(&g_sensor_queue, &s, pdMS_TO_TICKS(100)) == pdTRUE)
        printf("[Logger] Got sample: ADC=%u at t=%ums\n", s.adc_raw, (unsigned)s.timestamp_ms);
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 3 — Queue (producer/consumer with data)
 * ============================================================ */

typedef struct { uint32_t timestamp_ms; float temperature; float humidity; } SensorSample;

#define QUEUE_DEPTH  4u
typedef struct {
    SensorSample buf[QUEUE_DEPTH];
    uint8_t head, tail, count;
} SensorQueue;

static SensorQueue g_sensor_q = {0};

int queue_send(SensorQueue *q, const SensorSample *s)
{
    if (q->count >= QUEUE_DEPTH) return 0;  /* full */
    q->buf[q->head] = *s;
    q->head = (q->head + 1) % QUEUE_DEPTH;
    q->count++;
    return 1;
}

int queue_recv(SensorQueue *q, SensorSample *out)
{
    if (q->count == 0) return 0;  /* empty */
    *out = q->buf[q->tail];
    q->tail = (q->tail + 1) % QUEUE_DEPTH;
    q->count--;
    return 1;
}

/* ============================================================
```

---

## semaphores mutexes：任务 4

来源：`05_RTOS/02_semaphores_mutexes.c`；任务编号：`4`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 4 — 死锁场景
 *
 * 任务 A 获取 Mutex1，然后尝试获取 Mutex2。
 * 任务 B 获取 Mutex2，然后尝试获取 Mutex1。
 * 结果：DEADLOCK — 两个任务都永远等待。
 *
 * 预防措施：始终在所有任务中使用相同的 ORDER 中的互斥锁。
 * =============================================================== */

void explain_deadlock(void)
{
    printf("\n--- Deadlock Demo ---\n");
    printf("Task A: takes Mutex1, then waits for Mutex2\n");
    printf("Task B: takes Mutex2, then waits for Mutex1\n");
    printf("Result: circular dependency — both block forever.\n");
    printf("Fix: all tasks must acquire mutexes in the SAME fixed order.\n");
    printf("     Or: use trylock with timeout and retry with backoff.\n");
}

/* ===============================================================
```

### 对应参考答案

死锁需要同时满足互斥、持有并等待、不可抢占和循环等待。两个任务分别持有 Mutex1 与 Mutex2，又互相等待对方释放时就会永久阻塞。工程上应规定全局一致的加锁顺序，尽量缩短临界区，避免持锁调用未知代码；必要时使用超时、`try-lock` 和失败回退。

---

## semaphores mutexes：任务 5

来源：`05_RTOS/02_semaphores_mutexes.c`；任务编号：`5`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 5 — BUG HUNT：同步错误
 *
 * 下面的代码有 3 个并发错误。
 * 找到并标记每一项。
 * =============================================================== */

static SimSemaphore g_lock = {1, 1};
static uint32_t g_shared_counter = 0;

void increment_shared_BUGGY(uint32_t count)
{
    /* Bug 1：访问 g_shared_counter 之前未获取互斥锁。
     * 如果两个任务同时运行，则读取-修改-写入
     * g_shared_counter 上存在数据竞争。 */
    for (uint32_t i = 0; i < count; i++) {
        g_shared_counter++;
    }
    /* 缺少：循环之前的 sem_take(&g_lock, portMAX_DELAY)
     * 循环后的 sem_give(&g_lock) */
}

BaseType_t isr_safe_signal_BUGGY(SimSemaphore *mutex)
{
    /* Bug 2：在 FreeRTOS 中，从 ISR 给出 MUTEX 是非法的。
     * 互斥体跟踪所有权（优先级继承）——来自 ISR
     * 破坏所有权状态。
     * 使用 BINARY SEMAPHORE 进行 ISR 到任务的信号发送。 */
    return sem_give(mutex);
}

void critical_section_BUGGY(SimSemaphore *sem)
{
    /* Bug 3：没有处理 sem_take 失败。
     * 如果 sem_take 超时或失败，则代码失败
     * 并修改 g_shared_counter 而不持有锁。*/
    sem_take(sem, 10);   /*返回值被忽略 */
    g_shared_counter = 999;
    sem_give(sem);
}

/* ===============================================================
 * SELF-TEST
 * =============================================================== */

int main(void)
{
    /* 二进制信号量测试 */
    uart_rx_isr_sim(0xAB);
    uart_process_task(NULL);

    /* 队列测试 */
    sensor_producer_task(NULL);
    logger_consumer_task(NULL);

    /* 互斥测试 */
    spi_write_protected(0x10, 0xFF);

    explain_deadlock();

    printf("\nAll synchronization tests PASSED.\n");
    return 0;
}

/* ===============================================================
 * INTERVIEW QUESTIONS
 * ===============================================================
 *
 * Q1: 二值信号量和互斥量有什么区别？
 * 可以使用信号量来保护共享资源吗？
 * 答案：TODO
 *
 * Q2: 为什么你可以从 ISR 给出二进制信号量而不是互斥量？
 * 答案：TODO
 *
 * Q3：什么是死锁？给出一个具有两个互斥体的双任务示例。
 * 你如何预防它？
 * 答案：TODO
 *
 * Q4：您有一个长度为 10 的队列。生产者以 100 Hz 发送，
 * 消费者以 80 Hz 读取。 1秒内会发生什么？
 * 答案：TODO
 *
 * Q5：任务获取互斥锁，但在提供互斥锁之前崩溃。
 * FreeRTOS 中会发生什么？ “互斥持有者死亡”案例是如何发生的
 * FreeRTOS 和其他 RTOS 有何不同？
 * 答案：TODO*/
```

### 对应参考答案

答案文件中的对应内容位于任务 `4`（原文件任务顺序不同或合并了多个任务）。

```c
* TASK 4 — Bug hunt FIXED
 *
 * Bug 1: spi_write() accesses g_spi_data WITHOUT mutex.
 *        Two tasks calling spi_write() simultaneously may interleave
 *        their byte sequences → corrupted SPI transaction.
 *        FIX: take mutex before modifying shared SPI state, give after.
 *
 * Bug 2: uart_isr() calls xSemaphoreTake() (blocking) from ISR context.
 *        ISRs must NEVER call blocking FreeRTOS API. The ISR may be
 *        entered with the scheduler suspended or in a critical section.
 *        Calling Take can corrupt FreeRTOS internal state → HardFault.
 *        FIX: ISR should call xSemaphoreGiveFromISR(). The waiting task
 *        calls xSemaphoreTake() with a timeout.
 *
 * Bug 3: return value of sem_take ignored.
 *        If sem_take returns pdFALSE (timeout expired), the code proceeds
 *        to use stale data as if fresh data arrived.
 *        FIX: check return value and skip processing on timeout/failure.
 * ============================================================ */

int main(void)
{
    /* Binary semaphore test */
    sim_sem_init_binary(&g_uart_rx_sem);
    assert(g_uart_rx_sem.count == 0);

    uart_rx_isr_sim(0x42);
    assert(g_uart_rx_sem.count == 1);

    uart_process_task_sim();
    assert(g_uart_rx_sem.count == 0);

    /* Mutex test */
    spi_write_protected(0x01, 0xAB);
    assert(g_spi_reg_value == 0xAB);
    assert(!g_spi_mutex.locked);  /* released after write */

    /* Queue test */
    SensorSample s1 = {1000, 25.5f, 60.0f};
    SensorSample s2 = {2000, 26.0f, 61.0f};
    assert(queue_send(&g_sensor_q, &s1));
    assert(queue_send(&g_sensor_q, &s2));
    assert(g_sensor_q.count == 2);

    SensorSample out;
    assert(queue_recv(&g_sensor_q, &out));
    assert(out.timestamp_ms == 1000 && out.temperature == 25.5f);
    assert(g_sensor_q.count == 1);

    /* Queue full test */
    SensorSample dummy = {0};
    queue_send(&g_sensor_q, &dummy);
    queue_send(&g_sensor_q, &dummy);
    queue_send(&g_sensor_q, &dummy);
    assert(g_sensor_q.count == QUEUE_DEPTH);
    assert(queue_send(&g_sensor_q, &dummy) == 0);  /* must fail when full */

    printf("All RTOS semaphore/mutex answers verified.\n");
    return 0;
}
```
