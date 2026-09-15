/** ===============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：易失性、常量、静态——三个关键词
 * 每个嵌入式面试都会问到的问题
 * 文件：01_C_Fundamentals/04_volatile_const_static.c
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ===============================================================
 * THEORY
 * ===============================================================
 *
 * 易失性：
 * 告诉编译器：“DO NOT 优化此读/写。
 * 该变量可以在正常程序流程之外更改
 *（硬件，ISR，其他CPU内核）。”
 * WITHOUT 易失性，编译器可能缓存在寄存器中
 * 并且永远不会重新阅读 - 您的 ISR 标志永远不会被看到。
 *
 * 常量：
 *“这个值不会改变。”编译器可以放在ROM中。
 * 在 MCU 上：const 全局变量位于闪存中的 .rodata（免费 SRAM）中。
 * const + volatile 一起：硬件状态寄存器
 * 会自行改变，但你永远不会写入它。
 *
 *静态：
 * 在函数作用域：变量在调用中仍然存在（存储
 * 在.data 或.bss 中，不在堆栈中）。
 * 在文件范围内：限制此翻译单元的可见性
 *（“私有”的嵌入等效项）。
 *
 * const 易失性uint32_t *:
 * 自己改变的只读硬件寄存器。
 * 示例：RX 数据寄存器 — 您读取它，硬件写入它。
 * =============================================================== */


/* ===============================================================
 * TASK 1 — 易失性：ISR 至主通信
 *
 * UART ISR 填充环形缓冲区并设置标志。
 * 主循环必须看到标志的变化。没有 volatile 会出现什么问题？
 * =============================================================== */

/* 标志 — ISR 设置它，main 读取它 */
/* TODO：将正确的限定符添加到此声明中 */
uint8_t g_data_ready = 0;

/* 环形缓冲区 — 由 ISR 写入，由 main 读取 */
/* TODO：添加正确的限定符 */
uint8_t g_rx_buf[64];
/* TODO：添加正确的限定符 */
uint8_t g_rx_head = 0;
uint8_t g_rx_tail = 0;

/* 模拟 ISR — 将是真实 MCU 上的 USART1_IRQHandler */
void simulated_uart_isr(uint8_t received_byte)
{
    g_rx_buf[g_rx_head % 64] = received_byte;
    g_rx_head++;
    g_data_ready = 1;
}

int main_loop_iteration(void)
{
    /* TODO：检查g_data_ready
     * 如果设置：从 g_rx_buf[g_rx_tail % 64] 读取一个字节，增加 g_rx_tail
     * 如果 g_rx_tail == g_rx_head: 清除 g_data_ready
     * 返回读取到的字节
     * 如果未设置：返回-1 */
    return -1;
}

/* ===============================================================
 * TASK 2 — 查找表和配置常量
 *
 * 在微控制器上，常量全局数组位于闪存中。
 * 这释放了宝贵的 SRAM。
 *
 * 实现 PWM 占空比到 DAC 输出查找表。
 * 该表应位于 ROM（闪存）中，而不是 SRAM 中。
 * ===============================================================*/

/*TODO：使用正确的限定符声明此表，以便它
 * 位于 MCU 上的 ROM — 256 个条目，0 到 255 */
uint8_t pwm_to_dac_table[256]; /* TODO：修复限定符，TODO：填写值0..255 */

uint8_t pwm_to_dac(uint8_t pwm_percent)
{
    /* TODO：将 pwm_percent 钳位到 0-99，索引表
     *（100% PWM = 255 DAC，线性） */
    (void)pwm_percent;
    return 0;
}

/* ===============================================================
 * TASK 3 — 用于状态保留和封装的静态
 * =============================================================== */

/* 一个简单的去抖过滤器——必须保留调用之间的状态 */
uint8_t debounce_button(uint8_t raw_pin_state)
{
    /* TODO：声明一个静态uint8_t计数器= 0
     * 声明一个静态 uint8_t stable_state = 0
     * 逻辑：
     * 如果 raw_pin_state == stable_state：重置计数器，返回 stable_state
     * else: 递增计数器
     * 如果计数器 >= 5：stable_state = raw_pin_state，计数器 = 0
     * 返回稳定状态 */
    (void)raw_pin_state;
    return 0;
}

/* 毫秒刻度计数器 — 按 SysTick ISR 递增 */
static volatile uint32_t g_tick_ms = 0;

void systick_isr(void) /* 模拟的 */
{
    g_tick_ms++;
}

uint32_t get_tick_ms(void)
{
    /* TODO：返回g_tick_ms
     * 问：该读取是否应该受到保护？为什么/为什么不在 32 位 ARM 上？ */
    return 0;
}

uint8_t has_elapsed_ms(uint32_t start_tick, uint32_t duration_ms)
{
    /* TODO：如果 (get_tick_ms() - start_tick) >= period_ms，则返回 1
     * 注意：即使计数器环绕，这也能正常工作！
     *（无符号减法在 C 中正确换行）
     * 这是在嵌入式中测量经过时间的正确方法。 */
    (void)start_tick; (void)duration_ms;
    return 0;
}

/* ===============================================================
 * TASK 4 — const 易失性一起：只读硬件寄存器
 *
 * 硬件定时器计数器寄存器：
 * - 不断变化（硬件递增→易失性）
 * - 你不应该从软件中写入它（常量）
 * =============================================================== */

/* 模拟定时器计数器寄存器 */
static volatile uint32_t _fake_timer_cnt = 0;

/* TODO：将timer_count_reg声明为指向
 * 一个const volatile uint32_t，指向_fake_timer_cnt */
/* const 易失性 uint32_t *timer_count_reg = ... */

uint32_t read_timer_count(void)
{
    /* TODO：取消引用timer_count_reg并返回值 */
    return 0;
}

/* 尝试编写 - 如果声明正确则不应编译 */
/* void BAD_write_timer(uint32_t v) { *timer_count_reg = v; } */

/* ===============================================================
 * TASK 5 — 文件范围内静态：模块私有状态
 *
 * 实现软件PWM 模块。内部状态应该
 * 无法从此文件外部访问。
 * =============================================================== */

/* TODO：将它们声明为文件私有（无法从其他 .c 文件访问） */
uint8_t  pwm_duty_pct = 0;
uint32_t pwm_period_ticks = 0;
uint32_t pwm_on_ticks     = 0;

void pwm_set_duty(uint8_t duty_pct, uint32_t period_ticks)
{
    /* TODO：保存period_ticks和duty_pct
     * TODO：计算 pwm_on_ticks = (duty_pct * period_ticks) / 100 */
    (void)duty_pct; (void)period_ticks;
}

/* 每个时钟周期从计时器 ISR 调用*/
uint8_t pwm_get_output(uint32_t current_tick)
{
    /*TODO：如果 (current_tick % pwm_period_ticks) < pwm_on_ticks，则返回 1
     * 否则返回0
     * 处理 pwm_period_ticks == 0（除以零保护） */
    (void)current_tick;
    return 0;
}

/* ===============================================================
 * TASK 6 — BUG HUNT
 *
 * 下面的函数应该使带有非阻塞定时器的 LED 闪烁。
 * 它有 3 个与 volatile/static/const 滥用相关的错误。
 * 找到并标记每一项。
 * =============================================================== */

uint32_t tick = 0;   /* 错误1：？？？ — 这是在轮询循环中读取的 */

void led_blink_BUGGY(void)
{
    uint32_t last_toggle = 0;
    uint8_t  led_state   = 0;
    const uint32_t BLINK_INTERVAL = 500;   /* 女士 */

    /* 错误2：？？？ —“last_toggle”是一个局部变量。
     * 每次调用这个函数时会发生什么？ */
    while (1) {
        if ((tick - last_toggle) >= BLINK_INTERVAL) {
            led_state   ^= 1;
            last_toggle  = tick;
            printf("LED: %s\n", led_state ? "ON" : "OFF");
        }
        /* 错误3：？？？ —“tick”在这里永远不会增加。在真实系统中
         * 它将按 SysTick ISR 递增。但限定符是什么
         * 缺少让编译器优化的声明
         * 这个循环变成了读取缓存寄存器的无限紧密循环？ */
    }
}

/* ===============================================================
 * INTERVIEW QUESTIONS
 * ===============================================================
 *
 * Q1：如果从 ISR 标志变量中删除 volatile 会发生什么？
 * 给出具体的编译器优化示例。
 * 答案：TODO
 *
 * Q2: 变量可以同时是 const 和 volatile 吗？举个真实的例子。
 * 答案：TODO
 *
 * Q3：静态全局和非静态全局有什么区别？
 * 为什么嵌入式样式指南更喜欢静态文件范围变量？
 * 答案：TODO
 *
 * Q4：uint32_t 计数器在 SysTick ISR 中递增并在 main 中读取。
 * 在 32 位 ARM Cortex-M4 上，读取是原子的吗？在 Cortex-M0 上？
 * 答案：TODO
 *
 * Q5：您在 STM32 项目中有一个 const uint8_t Lookup_table[512]。
 * 它实际居住在哪里？你如何验证这一点？
 * 答案：TODO*/
