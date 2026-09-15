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
