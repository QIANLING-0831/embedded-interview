---
title: "中断与 ISR"
source_directory: "03_Interrupts_and_ISR"
knowledge_type: embedded_interview_qa
language: zh-CN
tags: [ISR, 环形缓冲区, 临界区, DMA]
---

# 中断与 ISR

本文件将题目与对应答案放在同一个二级标题下。检索命中一个知识单元后，应同时参考题目、理论和答案生成中文口述回答。

## isr design：核心理论与上下文

来源：`03_Interrupts_and_ISR/01_isr_design.c`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明，不要只复述代码。

### 理论、公共定义与使用说明

```c
/** ===============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：中断设计——规则、模式、陷阱
 * 文件：03_Interrupts_and_ISR/01_isr_design.c
 * ===============================================================
 *
 * ISR（中断服务例程）是 THE 经过最多面试测试的
 * 嵌入式固件主题。把这些规则牢记在心。
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ===============================================================
 * THEORY — ISR 设计的黄金法则
 * ===============================================================
 *
 * RULE 1：ISR 必须是 SHORT。
 * 长 ISR 会阻塞其他中断。经验法则：< 1μs
 *   200MHz MCU.做最少的事情：设置一个标志，推送到队列，切换 pin。
 * 繁重处理属于主循环或 RTOS 任务。
 *
 * RULE 2：ISR 内部无阻塞。
 * 无 printf()、无 malloc()、无 delay_ms()、无互斥锁、
 * 没有等待完成的 I2C/SPI 事务。
 *
 * RULE 3：与 ISR 共享的变量必须是易失性的。
 * 如果没有 volatile，编译器会缓存在寄存器中并
 * main() 从未看到 ISR 的写入。
 *
 * RULE 4：保护共享多字节数据。
 * 在 32 位 ARM 上，uint32_t 读取是原子的。 uint64_t 为 NOT。
 * 结构是 NOT 原子结构。使用临界区或双缓冲。
 *
 * RULE 5：清除ISR中的中断标志FIRST。
 *（在大多数 MCU 上）— 在进行任何处理之前。这可以防止
 * 缺少在处理第一个事件时到达的第二个事件。
 * 例外：某些外设需要读取数据寄存器来清除标志。
 *
 * RULE 6：在不保存 FPU 上下文的情况下，ISR 中没有浮点。
 * 在ARM Cortex-M4F/M7上，FPU寄存器默认保存为NOT
 *   on interrupt entry (lazy stacking). If your ISR uses float,
 * 启用 LSPEN 或手动保存/恢复。
 * =============================================================== */

/* ===============================================================
```

### 答案文件公共定义

答案来源：`03_Interrupts_and_ISR/answers/01_isr_design_answers.c`

```c
/** ANSWERS: 03_Interrupts_and_ISR/01_isr_design.c
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ===============================================================
```

---

## isr design：任务 1

来源：`03_Interrupts_and_ISR/01_isr_design.c`；任务编号：`1`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 1 — 这个 ISR 有什么问题吗？
 *
 * 查看下面的假 ISR 并识别所有违规行为。
 * 然后实现正确的版本。
 * =============================================================== */

/* 模拟外设寄存器 */
static volatile uint32_t UART_SR   = 0;   /* status: bit0=RXNE */
static volatile uint8_t  UART_DR   = 0;   /* data register */
#define UART_SR_RXNE  (1u << 0)

/* Bad ISR — has multiple problems */
char rx_line[128];
int  rx_pos = 0;

void UART_IRQHandler_BAD(void)
{
    /* 问题 1：全局非易失性 — 编译器可能会缓存 rx_pos */
    if (UART_SR & UART_SR_RXNE) {
        char c = (char)UART_DR;
        /* 问题 2：对 rx_pos 没有边界检查 — 缓冲区溢出 */
        rx_line[rx_pos++] = c;

        if (c == '\n') {
            /* 问题 3：ISR 内部的 printf — 块，内部使用堆 */
            printf("Received: %s\n", rx_line);
            rx_pos = 0;
        }
        /* 问题 4：此外设类型的中断标志未清除 */
    }
}

/* Correct version — implement this */
#define RX_BUF_SIZE  128u

volatile uint8_t  g_rx_buf[RX_BUF_SIZE];
volatile uint8_t  g_rx_head = 0;
volatile uint8_t  g_rx_tail = 0;
volatile uint8_t  g_rx_overrun = 0;
volatile uint8_t  g_line_ready  = 0;

void UART_IRQHandler_CORRECT(void)
{
    /* TODO: check UART_SR_RXNE flag*/
    /*TODO：读取 UART_DR — 这会清除大多数 UART 上的 RXNE */
    /* TODO：写入前检查缓冲区已满情况
     * (头+1) % RX_BUF_SIZE == 尾部 → 超限 */
    /* TODO：写入字节到g_rx_buf[g_rx_head]，提前头 */
    /* TODO：如果字节=='\n'，则设置g_line_ready = 1 */
    /* NOTE：无 printf、无 malloc、无阻塞 */
}

/* 主循环处理缓冲区 */
int process_received_line(char *out_buf, uint8_t out_max)
{
    /* TODO：检查g_line_ready
     * 如果设置：将字节从环形缓冲区（尾部到头部）排入 out_buf
     * 直到达到 '\n' 或 out_max
     * 空终止，清除 g_line_ready
     * 返回长度
     * 如果未设置：返回-1 */
    (void)out_buf; (void)out_max;
    return -1;
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 1 — 使用环形缓冲区修正 ISR
 * =============================================================== */

static volatile uint32_t UART_SR   = 0x20;  /* RXNE套装 */
static volatile uint8_t  UART_DR   = 0;
#define UART_SR_RXNE  (1u << 0)

#define RX_BUF_SIZE  128u

volatile uint8_t  g_rx_buf[RX_BUF_SIZE];
volatile uint8_t  g_rx_head    = 0;
volatile uint8_t  g_rx_tail    = 0;
volatile uint8_t  g_rx_overrun = 0;
volatile uint8_t  g_line_ready  = 0;

void UART_IRQHandler_CORRECT(void)
{
    if (!(UART_SR & UART_SR_RXNE)) return;   /* 规则 5：检查标志 */
    uint8_t c = UART_DR;                      /* 读取 DR 会清除真实硬件上的 RXNE */

    uint8_t next_head = (uint8_t)((g_rx_head + 1) % RX_BUF_SIZE);
    if (next_head == g_rx_tail) {
        g_rx_overrun = 1;                     /* 缓冲区已满——丢弃字节 */
        return;
    }
    g_rx_buf[g_rx_head] = c;
    g_rx_head = next_head;

    if (c == '\n') g_line_ready = 1;
}

int process_received_line(char *out_buf, uint8_t out_max)
{
    if (!g_line_ready) return -1;

    uint8_t len = 0;
    while (g_rx_tail != g_rx_head && len < out_max - 1u) {
        char c = (char)g_rx_buf[g_rx_tail];
        g_rx_tail = (uint8_t)((g_rx_tail + 1) % RX_BUF_SIZE);
        out_buf[len++] = c;
        if (c == '\n') break;
    }
    out_buf[len] = '\0';
    g_line_ready = 0;
    return (int)len;
}

/* ===============================================================
```

---

## isr design：任务 2

来源：`03_Interrupts_and_ISR/01_isr_design.c`；任务编号：`2`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 2 — 用于 ISR/任务通信的环形缓冲区
 *
 * 无锁单生产者单消费者（SPSC）环形缓冲区。
 * ISR = 生产者（写入），主循环 = 消费者（读取）。
 * 此模式出现在 UART、SPI、CAN 接收路径中。
 * =============================================================== */

#define RING_BUF_MASK  0x3Fu   /* size 必须是 2 的幂，mask = size-1 */
#define RING_BUF_SIZE  (RING_BUF_MASK + 1)  /* 64 */

typedef struct {
    volatile uint8_t  buf[RING_BUF_SIZE];
    volatile uint8_t  head;   /* 由制作人撰写（ISR） */
    volatile uint8_t  tail;   /* 由消费者编写（主要） */
} RingBuf;

void ring_init(RingBuf *rb)
{
    /* TODO：将结构清零 */
    (void)rb;
}

int ring_push(RingBuf *rb, uint8_t byte)
{
    /* TODO：检查是否已满：((rb->head + 1) & RING_BUF_MASK) == rb->tail
     * 如果已满：返回-1（丢弃字节）
     * 将字节写入buf[rb->head & RING_BUF_MASK]
     * 提前头部：rb->head = (rb->head + 1) & RING_BUF_MASK
     * 返回0
     *
     * NOTE：在8位MCU上，头增量必须是原子的。
     * 在 Cortex-M 上，这个单个 uint8_t 写入是原子的。 */
    (void)rb; (void)byte;
    return -1;
}

int ring_pop(RingBuf *rb, uint8_t *out)
{
    /* TODO：检查是否为空：rb->head == rb->tail → 返回-1
     * 将 buf[rb->tail & RING_BUF_MASK] 读入 *out
     * 提前尾部
     * 返回0 */
    (void)rb; (void)out;
    return -1;
}

uint8_t ring_available(const RingBuf *rb)
{
    /* TODO：返回可供读取的字节数
     * = (头-尾) & RING_BUF_MASK */
    (void)rb;
    return 0;
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 2 — SPSC 环形缓冲区
 * =============================================================== */

#define RING_BUF_MASK  0x3Fu
#define RING_BUF_SIZE  (RING_BUF_MASK + 1)

typedef struct {
    volatile uint8_t  buf[RING_BUF_SIZE];
    volatile uint8_t  head;
    volatile uint8_t  tail;
} RingBuf;

void ring_init(RingBuf *rb)
{
    memset((void*)rb->buf, 0, RING_BUF_SIZE);
    rb->head = 0;
    rb->tail = 0;
}

int ring_push(RingBuf *rb, uint8_t byte)
{
    uint8_t next_head = (rb->head + 1u) & RING_BUF_MASK;
    if (next_head == rb->tail) return -1;   /* 满 */
    rb->buf[rb->head] = byte;
    rb->head = next_head;
    return 0;
}

int ring_pop(RingBuf *rb, uint8_t *out)
{
    if (rb->head == rb->tail) return -1;   /* 空的 */
    *out = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1u) & RING_BUF_MASK;
    return 0;
}

uint8_t ring_available(const RingBuf *rb)
{
    return (rb->head - rb->tail) & RING_BUF_MASK;
}

/* ===============================================================
```

---

## isr design：任务 3

来源：`03_Interrupts_and_ISR/01_isr_design.c`；任务编号：`3`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 3 — 关键部分模式
 *
 * 当您从 MUST 访问多字节共享结构时
 * ISR 和 main 一样，你需要一个关键部分。
 * =============================================================== */

/* 模拟禁用/启用中断原语 */
static uint8_t g_irq_enabled = 1;
static void __disable_irq(void) { g_irq_enabled = 0; }
static void __enable_irq(void)  { g_irq_enabled = 1; }

typedef struct {
    uint32_t timestamp;
    float    temperature;
    float    pressure;
    uint8_t  valid;
} SensorSnapshot;

volatile SensorSnapshot g_sensor;  /* 由 ISR 写入，由 main 读取 */

/* ISR 写入新快照 */
void sensor_isr(uint32_t ts, float temp, float pressure)
{
    /* 在 Cortex-M 上，结构体写入是 NOT 原子性 — main 可能会读取
     * 写了一半的结构。使用双缓冲技巧或临界区。 */

    /* TODO：写入前禁用IRQ以防止抢占
     * ISR 由更高优先级的 ISR 读取 g_sensor */
    /* TODO：将时间戳、温度、压力、valid=1写入g_sensor */
    /* TODO：重新启用IRQ*/
    (void)ts; (void)temp; (void)pressure;
}

/*Main读取一致的快照 */
SensorSnapshot sensor_get_snapshot(void)
{
    SensorSnapshot local;
    /* TODO：禁用IRQ */
    /* TODO：local = g_sensor（完整结构副本） */
    /* TODO：启用IRQ */
    /* 返回本地 */
    return local;
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 3 — SensorSnapshot 的关键部分
 * =============================================================== */

static uint8_t g_irq_enabled = 1;
static void __disable_irq(void) { g_irq_enabled = 0; }
static void __enable_irq(void)  { g_irq_enabled = 1; }

typedef struct {
    uint32_t timestamp;
    float    temperature;
    float    pressure;
    uint8_t  valid;
} SensorSnapshot;

volatile SensorSnapshot g_sensor;

void sensor_isr(uint32_t ts, float temp, float pressure)
{
    __disable_irq();        /* 防止更高优先级的ISR读取部分写入 */
    g_sensor.timestamp   = ts;
    g_sensor.temperature = temp;
    g_sensor.pressure    = pressure;
    g_sensor.valid       = 1;
    __enable_irq();
}

SensorSnapshot sensor_get_snapshot(void)
{
    SensorSnapshot local;
    __disable_irq();
    local = g_sensor;       /* 关键部分内的原子结构副本 */
    __enable_irq();
    return local;
}

/* ===============================================================
```

---

## isr design：任务 4

来源：`03_Interrupts_and_ISR/01_isr_design.c`；任务编号：`4`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 4 — DMA 完成回调模式
 *
 * DMA 传输完成 → ISR 触发 → 设置标志。
 * 主循环开始下一次传输。
 * 双缓冲区：当DMA填充缓冲区B时，主处理缓冲区A。
 * =============================================================== */

#define DMA_BUF_SIZE  256u

uint8_t g_dma_buf_a[DMA_BUF_SIZE];
uint8_t g_dma_buf_b[DMA_BUF_SIZE];

volatile uint8_t g_dma_buf_ready = 0;  /* 0=无、1=buf_a、2=buf_b */
volatile uint8_t g_dma_active_buf = 0; /* 当前正在填充哪个缓冲区 DMA */

void DMA_IRQHandler(void)
{
    /* TODO：将g_dma_buf_ready设置为JUST完成的缓冲区
     * TODO：将g_dma_active_buf切换到其他缓冲区
     * TODO：在新的活动缓冲区上重新启动 DMA
     *（模拟：只需在 0 和 1 之间切换 g_dma_active_buf） */
}

uint8_t *dma_get_ready_buffer(uint16_t *len)
{
    /* TODO：如果g_dma_buf_ready == 0：*len=0，则返回NULL
     * if g_dma_buf_ready == 1: *len=DMA_BUF_SIZE，清除标志，返回g_dma_buf_a
     * if g_dma_buf_ready == 2: *len=DMA_BUF_SIZE，清除标志，返回g_dma_buf_b */
    (void)len;
    return NULL;
}

/* ===============================================================
```

### 对应参考答案

```c
* TASK 4 — DMA 双缓冲区
 * =============================================================== */

#define DMA_BUF_SIZE  256u

uint8_t g_dma_buf_a[DMA_BUF_SIZE];
uint8_t g_dma_buf_b[DMA_BUF_SIZE];

volatile uint8_t g_dma_buf_ready  = 0;   /* 1=buf_a 就绪，2=buf_b 就绪 */
volatile uint8_t g_dma_active_buf = 0;   /* 0=填充buf_a，1=填充buf_b */

void DMA_IRQHandler(void)
{
    /* 指示哪个缓冲区刚刚完成 */
    g_dma_buf_ready = (g_dma_active_buf == 0) ? 1u : 2u;
    /* 开关活动缓冲器 */
    g_dma_active_buf = (g_dma_active_buf == 0) ? 1u : 0u;
    /* 在实际代码中：在此处的新活动缓冲区上重新启动 DMA 传输 */
}

uint8_t *dma_get_ready_buffer(uint16_t *len)
{
    if (g_dma_buf_ready == 0) { *len = 0; return NULL; }
    uint8_t *buf = (g_dma_buf_ready == 1) ? g_dma_buf_a : g_dma_buf_b;
    *len = DMA_BUF_SIZE;
    g_dma_buf_ready = 0;
    return buf;
}

/* ===============================================================
```

---

## isr design：任务 5

来源：`03_Interrupts_and_ISR/01_isr_design.c`；任务编号：`5`

### 面试助手回答要求

先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。

### 题目与待实现代码

```c
* TASK 5 — BUG HUNT：ISR 计时错误
 *
 * 下面的代码使用两个定时器捕获 ISR 来测量脉冲宽度。
 * 它有 3 个错误。找到并标记每一个。
 * =============================================================== */

volatile uint32_t g_rise_tick = 0;
volatile uint32_t g_fall_tick = 0;
volatile uint8_t  g_pulse_ready = 0;
uint32_t          g_pulse_width_ticks;   /* 错误1：？？？ */

void rising_edge_isr(uint32_t current_tick)
{
    g_rise_tick = current_tick;
    g_pulse_ready = 0;
}

void falling_edge_isr(uint32_t current_tick)
{
    g_fall_tick = current_tick;

    /*错误2：？？？ */
    g_pulse_width_ticks = g_fall_tick - g_rise_tick;  /* 非易失性，可能已过时 */

    g_pulse_ready = 1;
}

uint32_t get_pulse_width(void)
{
    /* 错误3：？？？ */
    while (!g_pulse_ready) {}   /* busy-wait — 阻塞主干线，消耗电力。
                                 * 应采用事件驱动或使用 RTOS 块。 */
    g_pulse_ready = 0;
    return g_pulse_width_ticks;
}

/* ===============================================================
 * INTERVIEW QUESTIONS
 * ===============================================================
 *
 * Q1：列出 NEVER 在 ISR 内必须做的 5 件事以及原因。
 * 答案：TODO
 *
 * Q2：在ARM Cortex-M4上，自动保存哪些寄存器
 * 在中断入口处？ NOT 是哪些？
 * 答案：TODO
 *
 * Q3：什么是杂散中断？你如何应对防守型球员？
 * 答案：TODO
 *
 * Q4：您需要在 ISR 和 32 位 MCU 上的 main 之间共享 uint64_t。
 * 读取它是原子的吗？你会做什么来保护它？
 * 答案：TODO
 *
 * Q5: 可屏蔽中断和不可屏蔽中断有什么区别？
 * 给出 ARM Cortex-M 上的每个示例。
 * 答案：TODO
 *
 * Q6：解释 ARM Cortex-M 上的尾部链接。为什么会减少
 * 多个 IRQ 待处理时的中断延迟？
 * 答案：TODO*/
```

### 对应参考答案

```c
* TASK 5 — Bug 搜寻 FIXED
 *
 * 错误 1 和 2：g_pulse_width_ticks 不是易失性的。
 * 编译器可能会将写入缓存在寄存器中，并且永远不会刷新到 RAM。
 * Main 读取过时的值。 FIX：易失性uint32_t g_pulse_width_ticks；
 *
 * Bug 3：get_pulse_width() 中忙等待（while (!g_pulse_ready){}）。
 * 这会阻塞主循环，浪费 CPU 周期，阻止其他任务
 * 正在运行，并且可能会导致低优先级处理不足。
 * FIX：使用RTOS事件标志或二进制信号量；如果没有准备好则返回-1
 * 并让调用者重试（非阻塞轮询模式）。
 * =============================================================== */

volatile uint32_t g_rise_tick = 0;
volatile uint32_t g_fall_tick = 0;
volatile uint8_t  g_pulse_ready = 0;
volatile uint32_t g_pulse_width_ticks;   /* FIXED：现在不稳定 */

void rising_edge_isr(uint32_t current_tick)  { g_rise_tick = current_tick; g_pulse_ready = 0; }
void falling_edge_isr(uint32_t current_tick) {
    g_fall_tick = current_tick;
    g_pulse_width_ticks = g_fall_tick - g_rise_tick;
    g_pulse_ready = 1;
}

int get_pulse_width_nonblocking(uint32_t *width_out)
{
    /* FIXED：非阻塞——调用者决定如果没有准备好做什么*/
    if (!g_pulse_ready) return -1;
    *width_out = g_pulse_width_ticks;
    g_pulse_ready = 0;
    return 0;
}

/*===============================================================
 * INTERVIEW QUESTION ANSWERS
 * ===============================================================

Q1：NEVER 在 ISR 内必须做的 5 件事以及原因。

A: 1. printf() / sprintf() — 内部使用 malloc 作为缓冲区，调用锁
      I/O 流上的（互斥体）。如果 main 持有锁，可能会死锁。
   2. malloc() / free() — 堆函数采用互斥锁；堆状态可能是
      ISR 触发时不一致；不确定的运行时间。
   3. delay_ms() / vTaskDelay() — 阻塞 ISR 停止所有其他 IRQ
      相同或较低的优先级。系统出现挂起。
   4. xSemaphoreTake()（阻止）—可以尝试阻止 ISR 内部，其中
      在 FreeRTOS 中是未定义的行为；请改用 xSemaphoreGiveFromISR()。
   5. 浮点运算（无 FPU 上下文保存）— 在 Cortex-M4F 上，
      FPU 寄存器（S0-S15、FPSCR）在中断入口处保存
      默认（延迟堆叠）。使用 FP 的 ISR 会损坏 main 的 FP 寄存器。

Q2：Cortex-M4 中断入口自动保存哪些寄存器？

A：硬件自动将8个寄存器压入当前堆栈（MSP或PSP）：
   R0、R1、R2、R3 — 参数/返回寄存器（调用者保存）
   R12 — 暂存寄存器
   LR (R14) — 链接寄存器（返回地址）
   PC (R15) — 中断指令的程序计数器
   xPSR — 处理器状态寄存器（标志、ISR 编号、Thumb 状态）
   这 8 个寄存器 × 4 字节 = 32 字节最小堆栈帧。
   NOT 自动保存：R4-R11（被调用者保存 - 编译器将它们保存在序言中
   如果 ISR 使用它们）。 FPU 寄存器 S0-S15 延迟保存（FPCCR.LSPEN）。

Q3：什么是杂散中断？你如何在防守上处理它？

答：触发了一个没有可识别源的虚假中断 — ISR 向量
   已输入，但状态寄存器未显示待处理标志。
   原因：IRQ 线路上的电气噪声、清除标志的竞争条件、
   软件在标志被清除之前重新启用中断。
   防御性处理：始终检查每个 ISR 顶部的标志：
     if (!(PERIPH->SR & EXPECTED_FLAG)) 返回；  // 虚假的——忽略
   切勿假设 ISR 因预期原因而被触发。

Q4：ISR 和 32 位 MCU 上的 main 之间共享 uint64_t。原子？

答：不需要。64 位读取需要两个 32 位总线事务（LDRD 或两个 LDR）。
   在第一次和第二次读取之间，ISR 可以触发并更新两半。
   结果：main 读取 old_high + new_low — 一个损坏的值。
   解决方案：
   (a) 临界区：禁用中断、读取、启用中断。
   (b) 双拷贝模式：读取直到两个连续读取一致。
   (c) 顺序计数器：ISR 在写入之前/之后递增计数器；
       main 读取直到计数器没有改变（seqlock 模式）。

Q5：Cortex-M 上可屏蔽中断和不可屏蔽中断的区别。

A：可屏蔽（IRQ）：可以通过 PRIMASK (CPSID I / __disable_irq()) 全局禁用
   或选择性地使用BASEPRI。所有外设中断（UART、TIM、DMA 等）
   可屏蔽。 FreeRTOS 使用 BASEPRI 将 IRQ 屏蔽到阈值以下。
   不可屏蔽中断（NMI）：无法通过软件禁用。总是有回应。
   用途：时钟故障监视器、NMI 模式下的看门狗、灾难性硬件故障。
   同样不可屏蔽：HardFault、Reset。
   HardFault：由内存访问违规、无效指令触发。
   在 Cortex-M33 (TrustZone) 上：SecureFault 也无法被非安全代码屏蔽。

Q6：ARM Cortex-M 上的尾链是什么？为什么它会减少延迟？

答：当 CPU 完成一个 ISR 且另一个 IRQ 处于待处理状态（同等或更低优先级）时，
   而不是完全取消堆栈（恢复 8 个寄存器）并为下一个 ISR 重新堆栈
   （再次保存8个寄存器），它直接从ISR出口到下一个ISR入口。
   节省大约 12 个周期（相比之下，unstack+stack 节省约 30 个周期）。
   堆栈帧保持不变（SP 不变）。 EXC_RETURN机构
   识别待处理的 IRQ 并直接“链接”到它。
   实际影响：在 168 MHz 时，每个链接中断可节省 12 个周期 = ~71 ns。
   对于具有多个同时 IRQ 的系统（CAN 邮箱、ADC 过采样）至关重要。*/

int main(void)
{
    /*环形缓冲区测试 */
    RingBuf rb;
    ring_init(&rb);
    assert(ring_available(&rb) == 0);

    for (uint8_t i = 0; i < 10; i++) ring_push(&rb, i);
    assert(ring_available(&rb) == 10);

    uint8_t out;
    ring_pop(&rb, &out);
    assert(out == 0);
    assert(ring_available(&rb) == 9);

    /* 临界区测试 */
    sensor_isr(1000, 25.5f, 1013.25f);
    SensorSnapshot snap = sensor_get_snapshot();
    assert(snap.valid == 1 && snap.timestamp == 1000);

    /* DMA双缓冲测试 */
    g_dma_active_buf = 0;
    DMA_IRQHandler();
    assert(g_dma_buf_ready == 1);
    assert(g_dma_active_buf == 1);
    uint16_t len;
    uint8_t *buf = dma_get_ready_buffer(&len);
    assert(buf == g_dma_buf_a && len == DMA_BUF_SIZE);
    assert(g_dma_buf_ready == 0);

    /* 脉宽测试*/
    rising_edge_isr(1000);
    falling_edge_isr(1500);
    uint32_t width;
    assert(get_pulse_width_nonblocking(&width) == 0);
    assert(width == 500);

    printf("All ISR design answers verified.\n");
    return 0;
}
```
