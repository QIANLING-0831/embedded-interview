/** ANSWERS：04_Bare_Metal_Peripherals/01_gpio_registers.c
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR;
    volatile uint32_t IDR, ODR, BSRR, LCKR, AFRL, AFRH;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t AHB1ENR, APB1ENR, APB2ENR;
} RCC_TypeDef;

#define GPIO_MODE_INPUT   0b00u
#define GPIO_MODE_OUTPUT  0b01u
#define GPIO_MODE_AF      0b10u
#define GPIO_MODE_ANALOG  0b11u
#define GPIO_SPEED_HIGH   0b10u
#define GPIO_PULL_NONE    0b00u
#define GPIO_PULL_UP      0b01u

static GPIO_TypeDef _GPIOA={0}, _GPIOB={0};
static RCC_TypeDef  _RCC={0};
GPIO_TypeDef *GPIOA = &_GPIOA;
GPIO_TypeDef *GPIOB = &_GPIOB;
RCC_TypeDef  *RCC   = &_RCC;

/* ================================================================ TASK 1 */

void gpio_clock_enable(RCC_TypeDef *rcc, uint8_t port_index)
{
    rcc->AHB1ENR |= (1u << port_index);
}

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    gpio->MODER = (gpio->MODER & ~(0b11u << (pin*2))) | ((uint32_t)mode << (pin*2));
}

void gpio_set_speed(GPIO_TypeDef *gpio, uint8_t pin, uint8_t speed)
{
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(0b11u << (pin*2))) | ((uint32_t)speed << (pin*2));
}

void gpio_set_pull(GPIO_TypeDef *gpio, uint8_t pin, uint8_t pull)
{
    gpio->PUPDR = (gpio->PUPDR & ~(0b11u << (pin*2))) | ((uint32_t)pull << (pin*2));
}

void gpio_set_output_type(GPIO_TypeDef *gpio, uint8_t pin, uint8_t otype)
{
    if (otype) gpio->OTYPER |=  (1u << pin);
    else       gpio->OTYPER &= ~(1u << pin);
}

/* ================================================================ TASK 2 */

void gpio_write_pin(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* BSRR 原子设置/清除 — 无需读取-修改-写入 */
    if (value)
        gpio->BSRR = (1u << pin);          /* 设置 */
    else
        gpio->BSRR = (1u << (pin + 16u));  /* 重置 */
}

void gpio_toggle_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    gpio->ODR ^= (1u << pin);
}

uint8_t gpio_read_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    return (uint8_t)((gpio->IDR >> pin) & 1u);
}

void gpio_write_port(GPIO_TypeDef *gpio, uint16_t value)
{
    gpio->ODR = value;
}

uint16_t gpio_read_port(GPIO_TypeDef *gpio)
{
    return (uint16_t)(gpio->IDR & 0xFFFFu);
}

/* ================================================================ TASK 3 */

void gpio_set_af(GPIO_TypeDef *gpio, uint8_t pin, uint8_t af_num)
{
    if (pin < 8) {
        uint8_t shift = pin * 4u;
        gpio->AFRL = (gpio->AFRL & ~(0xFu << shift)) | ((uint32_t)af_num << shift);
    } else {
        uint8_t shift = (pin - 8u) * 4u;
        gpio->AFRH = (gpio->AFRH & ~(0xFu << shift)) | ((uint32_t)af_num << shift);
    }
}

/* ================================================================ TASK 4 */

typedef struct { uint8_t mode, otype, speed, pull, af; } GPIO_PinConfig;

void gpio_configure_pin(GPIO_TypeDef *gpio, uint8_t pin, const GPIO_PinConfig *cfg)
{
    gpio_set_mode(gpio, pin, GPIO_MODE_INPUT);   /* 重新配置期间暂时 INPUT */
    gpio_set_output_type(gpio, pin, cfg->otype);
    gpio_set_speed(gpio, pin, cfg->speed);
    gpio_set_pull(gpio, pin, cfg->pull);
    if (cfg->mode == GPIO_MODE_AF) gpio_set_af(gpio, pin, cfg->af);
    gpio_set_mode(gpio, pin, cfg->mode);         /* 最后设置最终模式 */
}

/* ================================================================ TASK 5 */

void init_usart2_pins(void)
{
    gpio_clock_enable(RCC, 0);   /* GPIOA = 端口索引 0 */

    GPIO_PinConfig tx_cfg = {GPIO_MODE_AF, 0 /*聚丙烯*/, GPIO_SPEED_HIGH, GPIO_PULL_NONE, 7 /*AF7=USART2*/};
    GPIO_PinConfig rx_cfg = {GPIO_MODE_AF, 0 /*聚丙烯*/, GPIO_SPEED_HIGH, GPIO_PULL_UP,   7 /*AF7=USART2*/};

    gpio_configure_pin(GPIOA, 2, &tx_cfg);   /* PA2 = USART2_TX */
    gpio_configure_pin(GPIOA, 3, &rx_cfg);   /* PA3 = USART2_RX */
}

/* ================================================================ TASK 6 — 错误搜寻 FIXED

Bug 1：在访问 GPIO 寄存器之前，RCC 时钟未启用。
   在 STM32 上，默认情况下禁用外设时钟以节省功耗。
   写入 GPIO 寄存器而不启用其时钟没有任何效果
   （或导致某些设备上出现总线故障）。
   FIX: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;在任何 GPIO 访问之前。

错误 2：gpio->MODER |= (GPIO_MODE_AF << (引脚*2))
   这会将新模式与先清除的现有位 WITHOUT 进行或运算。
   如果引脚处于 AF 模式 (0b10) 并且您在 0b10 中执行 OR → 没有变化，则正确。
   但如果引脚处于 OUTPUT 模式 (0b01) 并且您尝试设置 INPUT (0b00)：
   0b01 | 0b01 0b00 = 0b01 → 仍然输出！明确的步骤是强制性的。
   FIX: GPIO->MODER = (GPIO->MODER & ~(0b11u<<(引脚*2))) | (模式<<(引脚*2));

Bug 3：gpio->OTYPER &= ~(1u << pin) 设置 PUSH-PULL（位 = 0）。
   I2C SDA 必须是 OPEN-DRAIN（位 = 1） — 多个设备共享线路。
   推挽模式下，如果一台设备驱动HIGH，另一台设备驱动LOW
   同时→短路→硬件损坏。
   FIX: GPIO->OTYPER |= (1u << 引脚);  // 设置漏极开路*/

/*===============================================================
 * INTERVIEW QUESTION ANSWERS
 * ===============================================================

Q1：为什么使用BSRR而不是ODR？ BSRR 消除了什么竞争条件？

A：ODR 需要读-修改-写：读 ODR → 修改位 → 写 ODR。
   在读取和写入之间，ISR 可以触发并修改 ODR
   对于不同的引脚。当main的写入完成时，它会覆盖ISR的
   更改 → ISR 设置的引脚被清除。
   BSRR：单个 32 位写入原子设置或重置一个或多个引脚
   不影响任何其他引脚。无需读取 → 无竞争条件。
   位 [15:0] = 设置掩码，位 [31:16] = 重置掩码。一次写入=原子。

Q2：PA5 配置为 OUTPUT，但读取 IDR.5 始终返回 0。

答：可能的原因：
   (a) 外部硬件：有东西将引脚拉至 GND（短路）。
       如果输出正在驱动 HIGH，但 IDR 读取为 0，则该引脚被过驱动。
   (b) MODER 配置不正确 — 仍处于 INPUT 或 ANALOG 模式。
       检查 MODER[11:10] — 输出应为 0b01。
   (c) 时钟未启用 — GPIO 外设未提供时钟，寄存器写入被忽略。
   (d) ODR 位未设置 — MODER=输出，但 ODR.5=0 → 引脚为 LOW → IDR.5=0（正确！）。
   注：在输出模式下，IDR 反映的是实际引脚电压，而不是 ODR。
   即使 ODR.5=1，外部拉动 LOW 的输出引脚也会显示 IDR.5=0。

Q3：如果忘记启用 GPIO 外设时钟会怎样？

答：在 STM32 上，所有外设时钟默认都是门控的（低功耗）。
   写入 GPIO 寄存器而不启用时钟：
   - APB/AHB 总线写入可能会无错误地完成（无总线故障）。
   - 但写入被丢弃 - 外设没有时钟、寄存器
     不要锁存这些值。
   - GPIO 引脚保持默认状态（输入，无上拉）。
   无声的失败——初学者很常见的错误。始终首先启用时钟。

Q4：来自 3.3V MCU 的 5V I2C 总线上的开漏输出。

A：配置为开漏输出（OTYPER 位 = 1）。
   在开漏模式下：MCU 只能拉线 LOW（驱动 0）。
   要转到 HIGH，MCU 释放线路（不驱动任何东西），然后
   外部上拉电阻拉至 5V。
   MCU 的输入保护二极管可安全处理 5V HIGH 电平
   （只要引脚能够承受 5V 电压——请检查数据表！）。
   如果配置为推挽式并在 5V 总线上以 3.3V 驱动 HIGH：
   - 与驱动 LOW 的其他 5V 设备短路。
   - 3.3V HIGH 可能不符合 5V I2C VOH 规格 (0.7×VCC = 3.5V)。

Q5：GPIO 可以与 BSRR 同时设置多少个引脚？原子？

答：一个端口的所有 16 个引脚同时进行一次 32 位写入。
   BSRR[15:0] = 设置掩码（多个位 = 设置多个引脚）
   BSRR[31:16] = 复位掩码
   示例：gpio->BSRR = (1u<<3)|(1u<<7)|(1u<<(5+16)) → 设置引脚 3,7；清除引脚 5。
   是的，这是原子性的：单个 32 位 AHB 总线写入在 Cortex-M 上是原子性的。
   GPIO 外设在同一时钟周期锁存所有位。*/

int main(void)
{
    GPIOA->MODER = 0;
    GPIOA->ODR   = 0;
    GPIOA->BSRR  = 0;
    GPIOA->PUPDR = 0;
    GPIOA->AFRL  = 0;
    GPIOA->AFRH  = 0;

    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    assert((GPIOA->MODER & (0b11u << 10)) == (GPIO_MODE_OUTPUT << 10u));

    gpio_write_pin(GPIOA, 5, 1);
    assert(GPIOA->BSRR & (1u << 5));

    gpio_write_pin(GPIOA, 5, 0);
    assert(GPIOA->BSRR & (1u << 21));  /*位 16+5=21*/

    gpio_set_pull(GPIOA, 3, GPIO_PULL_UP);
    assert((GPIOA->PUPDR & (0b11u << 6)) == (GPIO_PULL_UP << 6u));

    gpio_set_af(GPIOA, 2, 7);
    assert((GPIOA->AFRL & (0xFu << 8)) == (7u << 8));

    gpio_set_af(GPIOA, 9, 7);
    assert((GPIOA->AFRH & (0xFu << 4)) == (7u << 4));

    init_usart2_pins();
    printf("All GPIO answers verified.\n");
    return 0;
}
