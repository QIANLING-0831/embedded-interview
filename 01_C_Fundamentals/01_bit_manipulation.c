/** ===============================================================
 * EMBEDDED INTERVIEW PREP – github.com/Amir7698/embedded-interview-prep
 * 主题：位操作
 * 文件：01_C_Fundamentals/01_bit_manipulation.c
 * 级别：初级 → 高级
 * ===============================================================
 *
 * 每个嵌入式采访都会涉及位操作。
 * 掌握这些，你在白板上就不再空白。
 *
 * HOW 至 USE：
 * 1. 阅读每项任务的理论部分。
 * 2. 实现每个TODO而不看答案/。
 * 3.编译：gcc -Wall -Wextra -o bit_manip 01_bit_manipulation.c
 * 4.与answers/01_bit_manipulation_answers.c比较
 * ===============================================================*/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/*===============================================================
 * THEORY——你必须知道的6个冷操作
 * ===============================================================
 *
 * SET 位 N : reg |= (1u << N)
 * CLEAR 位 N : reg &= ~(1u << N)
 * TOGGLE 位 N：reg ^= (1u << N)
 * READ 位 N : (reg >> N) & 1u
 * MASK : reg & 0xFF (保留低8位)
 * FIELD 写入：reg = (reg & ~MASK) | ((val << SHIFT) & MASK)
 *
 * 为什么是 1u 而不是 1？
 * 1 是有符号整数。在 32 位机器上 (1 << 31) 是 UB。
 * 1u 是无符号的——对于位宽内的所有移位都是安全的。
 * 使用 UINT32_C(1) 或 (uint32_t)1 作为寄存器级代码。
 * ===============================================================*/


/*===============================================================
 * TASK 1 – 基本寄存器操作
 * GPIO 输出数据寄存器（ODR）控制 16 个 LED。
 * 实现下面的四个原语。
 * =============================================================== */

void gpio_set_pin(uint16_t *reg, uint8_t pin)
{
    /* TODO：在 *reg 中设置位“pin” */
    (void)reg; (void)pin;
}

void gpio_clear_pin(uint16_t *reg, uint8_t pin)
{
    /* TODO：清除*reg中的位“pin” */
    (void)reg; (void)pin;
}

void gpio_toggle_pin(uint16_t *reg, uint8_t pin)
{
    /* TODO：*reg 中的切换位“pin” */
    (void)reg; (void)pin;
}

uint8_t gpio_read_pin(uint16_t reg, uint8_t pin)
{
    /* TODO：如果“pin”位被设置，则返回 1，否则返回 0*/
    (void)reg; (void)pin;
    return 0;
}

/*===============================================================
 * TASK 2——位域提取和插入
 *
 * CAN状态寄存器布局（32位）：
 * [31:24] = 保留
 * [23:16] = 错误计数器（8 位）
 * [15: 8] = 接收消息计数（8 位）
 * [ 7: 0] = tx 消息计数（8 位）
 * =============================================================== */

#define CAN_TX_SHIFT   0
#define CAN_TX_MASK    0x000000FFu
#define CAN_RX_SHIFT   8
#define CAN_RX_MASK    0x0000FF00u
#define CAN_ERR_SHIFT  16
#define CAN_ERR_MASK   0x00FF0000u

uint8_t can_get_tx_count(uint32_t status_reg)
{
    /* TODO：提取位[7:0] */
    (void)status_reg;
    return 0;
}

uint8_t can_get_rx_count(uint32_t status_reg)
{
    /* TODO：提取位[15:8] */
    (void)status_reg;
    return 0;
}

uint8_t can_get_error_counter(uint32_t status_reg)
{
    /* TODO：提取位[23:16] */
    (void)status_reg;
    return 0;
}

uint32_t can_set_tx_count(uint32_t status_reg, uint8_t count)
{
    /* TODO：将“计数”写入位[7:0]，保留所有其他位*/
    (void)count;
    return status_reg;
}

/*===============================================================
 * TASK 3 – 字节序检测和字节交换
 *
 * 字节顺序：
 * 小尾数：LSB 位于最低地址。 （x86，默认为 ARM）
 * 大尾数：MSB 位于最低地址。 （网络字节顺序）
 *
 * CAN、以太网、SOME/IP 标头为大端字节序。
 * 大多数 MCU 都是小端字节序。
 * 您WILL在解析协议帧时需要交换字节。
 * =============================================================== */

int is_little_endian(void)
{
    /* TODO：如果本机是小端则返回1，如果大端则返回0
     * 提示：将 uint16_t 0x0001 地址转换为 uint8_t* 并检查第一个字节 */
    return -1;
}

uint16_t swap16(uint16_t x)
{
    /* TODO：交换 16 位值的字节顺序
     * 0xAABB → 0xBBAA
     * 提示：使用移位和 OR，不使用 stdlib */
    (void)x;
    return 0;
}

uint32_t swap32(uint32_t x)
{
    /* TODO：交换 32 位值的字节顺序
     * 0xAABBCCDD → 0xDDCCBBAA */
    (void)x;
    return 0;
}

/* ===============================================================
 * TASK 4 – 计数和查找位
 * 这些出现在 CRC、校验和和协议代码中。
 * =============================================================== */

uint8_t count_set_bits(uint32_t x)
{
    /* TODO：返回x中1位的数量（popcount）
     * 使用__builtin_popcount实现WITHOUT
     * 经典：Brian Kernighan 算法： while(x) { x &= x-1;计数++； } */
    (void)x;
    return 0;
}

int8_t find_highest_set_bit(uint32_t x)
{
    /* TODO：返回最高设置位的位置（0索引）
     * 如果 x == 0，则返回 -1
     * 示例：x=0b1010 → 返回 3
     * 实现 WITHOUT __builtin_clz */
    (void)x;
    return -1;
}

uint32_t reverse_bits(uint32_t x)
{
    /* TODO：反转x的所有32位
     * 0b10110000_00000000_00000000_00000000→
     * 0b00000000_00000000_00000000_00001101
     * 用于CRC反射算法 */
    (void)x;
    return 0;
}

/* ===============================================================
 * TASK 5 – 打包寄存器字段（真实的 MCU 场景）
 *
 * USART 控制寄存器（32 位），STM32 样式：
 * 位 13：UE – USART 启用
 * 位 12：M – 字长（0=8bit，1=9bit）
 * 位 10：PCE – 奇偶校验控制使能
 * Bit 9 : PS – 奇偶校验选择（0=偶数，1=奇数）
 * 位 2:0：保留（必须为 0）
 * =============================================================== */

#define USART_CR1_UE    (1u << 13)
#define USART_CR1_M     (1u << 12)
#define USART_CR1_PCE   (1u << 10)
#define USART_CR1_PS    (1u <<  9)

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t BRR;
    volatile uint32_t SR;
    volatile uint32_t DR;
} USART_TypeDef;

void usart_enable(USART_TypeDef *usart)
{
    /* TODO：设置CR1中的UE位 */
    (void)usart;
}

void usart_configure_8n1(USART_TypeDef *usart)
{
    /* TODO：8位字（M=0），无奇偶校验（PCE=0）
     * 清除M、PCE、PS位； UE此处必须改为NOT */
    (void)usart;
}

void usart_configure_8e1(USART_TypeDef *usart)
{
    /* TODO：8位字，偶校验（PCE=1，PS=0） */
    (void)usart;
}

void usart_configure_9o1(USART_TypeDef *usart)
{
    /* TODO：9位字，奇校验（M=1，PCE=1，PS=1） */
    (void)usart;
}

/* ===============================================================
 * TASK 6 – BUG HUNT
 *
 * 下面的函数应该设置 16 位寄存器的位 15:12
 * 为值“val”（0-15），保留所有其他位。
 * 它有 3 个错误。找到并标记每一个。
 * =============================================================== */

uint16_t set_field_BUGGY(uint16_t reg, uint8_t val)
{
    uint16_t mask  = 0xF000;
    uint8_t  shift = 12;

    /* 错误1：？？？ */
    reg = reg & mask;               /* 应该清除该字段，而不是保留它*/

    /*错误2：？？？ */
    uint16_t field = val << shift;  /* val 是 uint8_t — 移位 12 导致 UB
                                     * 因为8位类型不能保存bit 12 */

    /* 错误3：？？？ */
    return reg | field;             /* 掩码应该是〜掩码以首先清除字段 */
}

/* ===============================================================
 * SELF-TEST – 运行以检查您的实现
 * =============================================================== */

static void test_bit_manipulation(void)
{
    /* 任务1 */
    uint16_t reg = 0x0000;
    gpio_set_pin(&reg, 3);
    assert(reg == 0x0008);
    gpio_set_pin(&reg, 7);
    assert(reg == 0x0088);
    gpio_clear_pin(&reg, 3);
    assert(reg == 0x0080);
    gpio_toggle_pin(&reg, 0);
    assert(reg == 0x0081);
    assert(gpio_read_pin(reg, 0) == 1);
    assert(gpio_read_pin(reg, 1) == 0);

    /* 任务2 */
    uint32_t can_status = 0x00AB1234u;
    assert(can_get_tx_count(can_status)    == 0x34);
    assert(can_get_rx_count(can_status)    == 0x12);
    assert(can_get_error_counter(can_status) == 0xAB);
    uint32_t updated = can_set_tx_count(can_status, 0xFF);
    assert((updated & 0xFF) == 0xFF);
    assert((updated >> 8) == (can_status >> 8));

    /* 任务3 */
    assert(swap16(0xAABB) == 0xBBAA);
    assert(swap32(0xAABBCCDD) == 0xDDCCBBAA);

    /* 任务4 */
    assert(count_set_bits(0b10110101) == 5);
    assert(count_set_bits(0) == 0);
    assert(count_set_bits(0xFFFFFFFF) == 32);
    assert(find_highest_set_bit(0b1010) == 3);
    assert(find_highest_set_bit(0)      == -1);
    assert(find_highest_set_bit(1)      ==  0);

    printf("All bit manipulation tests PASSED.\n");
}

int main(void)
{
    test_bit_manipulation();
    return 0;
}

/* ===============================================================
 * INTERVIEW QUESTIONS – 大声回答这些问题，然后检查答案/
 * ===============================================================
 *
 * Q1: (1 << 31) 和 (1u << 31) 有什么区别？
 * 答案：TODO
 *
 * Q2：如何在裸机 MCU 上自动设置一个位而不需要
 * 如果 ISR 可能抢占，会破坏其他位吗？
 * 答案：TODO
 *
 * Q3: 寄存器要求您将 0 写入位 [3:0] 并保留所有位
 * 其他位不变。写一行字。
 * 答案：TODO
 *
 * Q4：ARM Cortex-M 上的位带区域是什么？为什么它有用？
 * 答案：TODO
 *
 * Q5：如何在 256 字节的 ROM 中实现 16 位 CRC 表？
 * 答案：TODO*/
