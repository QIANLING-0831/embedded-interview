/** ===============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：GPIO — 寄存器级编程（STM32 风格）
 * 文件：04_Bare_Metal_Peripherals/01_gpio_registers.c
 * ===============================================================
 *
 *“没有 HAL，没有 CubeMX。显示寄存器。”
 * 这就是初级嵌入式工程师与高级嵌入式工程师的区别。
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ===============================================================
 * THEORY — STM32 GPIO 寄存器映射
 * ===============================================================
 *
 * 每个 GPIO 端口有 10 个寄存器（10×4 字节 = 40 字节）。
 *
 * MODER (0x00)：模式 — 每个引脚 2 位
 * 00 = 输入，01 = 输出，10 = 复用功能，11 = 模拟
 *
 * OTYPER (0x04)：输出类型 — 每个引脚 1 位
 * 0 = 推挽式，1 = 漏极开路
 *
 * OSPEEDR (0x08)：输出速度 — 每个引脚 2 位
 * 00=低，01=中，10=高，11=非常高
 *
 * PUPDR (0x0C)：上拉/下拉 — 每个引脚 2 位
 * 00=无，01=上拉，10=下拉
 *
 * IDR (0x10)：输入数据寄存器（只读，每个引脚 1 位）
 * ODR (0x14)：输出数据寄存器（每个引脚 1 位）
 *
 * BSRR (0x18)：位设置/复位寄存器 — ATOMIC 操作
 * 位 [15:0] = SET（写入 1 到设置引脚，0 = 无效）
 * 位 [31:16] = RESET（向清除引脚写入 1，0 = 无效）
 *
 * AFRL (0x20)：复用功能低电平（引脚 0-7，每个 4 位）
 * AFRH (0x24)：复用功能高位（引脚 8-15，每个 4 位）
 *
 * 时钟使能：RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN；
 * MUST 可以通过 BEFORE 访问任何 GPIO 寄存器。
 * =============================================================== */


/* 模拟GPIO和RCC寄存器进行主机端练习 */
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFRL;
    volatile uint32_t AFRH;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t AHB1ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} RCC_TypeDef;

/* 位定义 */
#define RCC_AHB1ENR_GPIOAEN  (1u << 0)
#define RCC_AHB1ENR_GPIOBEN  (1u << 1)
#define RCC_AHB1ENR_GPIOCEN  (1u << 2)

/* GPIO 模式常数 */
#define GPIO_MODE_INPUT    0b00u
#define GPIO_MODE_OUTPUT   0b01u
#define GPIO_MODE_AF       0b10u
#define GPIO_MODE_ANALOG   0b11u

/* GPIO 速度常数 */
#define GPIO_SPEED_LOW     0b00u
#define GPIO_SPEED_MEDIUM  0b01u
#define GPIO_SPEED_HIGH    0b10u
#define GPIO_SPEED_VHIGH   0b11u

/* GPIO 拉力常数 */
#define GPIO_PULL_NONE     0b00u
#define GPIO_PULL_UP       0b01u
#define GPIO_PULL_DOWN     0b10u

/* 模拟实例 */
static GPIO_TypeDef  _GPIOA = {0};
static GPIO_TypeDef  _GPIOB = {0};
static RCC_TypeDef   _RCC   = {0};
GPIO_TypeDef *GPIOA = &_GPIOA;
GPIO_TypeDef *GPIOB = &_GPIOB;
RCC_TypeDef  *RCC   = &_RCC;


/* ===============================================================
 * TASK 1 — GPIO 使能和模式配置
 * =============================================================== */

void gpio_clock_enable(RCC_TypeDef *rcc, uint8_t port_index)
{
    /* TODO：设置rcc->AHB1ENR中port_index的位
     * 端口索引：0=GPIOA、1=GPIOB、2=GPIOC 等
     * 位位置 = port_index */
    (void)rcc; (void)port_index;
}

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    /* TODO：MODER 在位位置每个引脚有 2 位（引脚 * 2）
     * 1. 清零：gpio->MODER &= ~(0b11u << (引脚 * 2))
     * 2. 设置：gpio->MODER |= (模式 << (引脚 * 2)) */
    (void)gpio; (void)pin; (void)mode;
}

void gpio_set_speed(GPIO_TypeDef *gpio, uint8_t pin, uint8_t speed)
{
    /* TODO：OSPEEDR 每个引脚有 2 位 — 与 MODER 模式相同 */
    (void)gpio; (void)pin; (void)speed;
}

void gpio_set_pull(GPIO_TypeDef *gpio, uint8_t pin, uint8_t pull)
{
    /* TODO：PUPDR 每个引脚有 2 位 — 相同模式 */
    (void)gpio; (void)pin; (void)pull;
}

void gpio_set_output_type(GPIO_TypeDef *gpio, uint8_t pin, uint8_t otype)
{
    /* TODO：OTYPER 在“引脚”位置每个引脚有 1 位
     * otype: 0=推挽式, 1=漏极开路*/
    (void)gpio; (void)pin; (void)otype;
}

/*===============================================================
 * TASK 2 — GPIO 读写
 * =============================================================== */

void gpio_write_pin(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* TODO：使用 BSRR 进行原子设置/清除 — 无读-修改-写
     * 如果值 == 1: gpio->BSRR = (1u << 引脚)
     * 如果值 == 0: gpio->BSRR = (1u << (pin + 16)) */
    (void)gpio; (void)pin; (void)value;
}

void gpio_toggle_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO：切换 ODR 位 — 此处可接受读取-修改-写入
     * 因为在这个模拟环境中没有 ISR。
     * 在真实硬件上：使用 BSRR 读取 ODR 进行原子切换：
     * if (ODR & (1<<引脚)) BSRR = 1<<(引脚+16);否则 BSRR = 1<< 引脚； */
    (void)gpio; (void)pin;
}

uint8_t gpio_read_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO：IDR 的“引脚”位置返回位 */
    (void)gpio; (void)pin;
    return 0;
}

void gpio_write_port(GPIO_TypeDef *gpio, uint16_t value)
{
    /* TODO：通过 ODR 一次写入所有 16 个引脚 */
    (void)gpio; (void)value;
}

uint16_t gpio_read_port(GPIO_TypeDef *gpio)
{
    /* TODO：读取IDR的所有16个引脚 */
    (void)gpio;
    return 0;
}

/* ===============================================================
 * TASK 3 — 备用功能配置
 *
 * 每个引脚都可以路由到外设（UART、SPI、I2C 等）
 * 通过选择备用功能编号 (AF0–AF15)。
 *
 * AFRL：引脚0-7，每个4位，从位（引脚* 4）开始
 * AFRH：引脚 8-15，每个 4 位，从位开始 ((pin-8) * 4)
 * =============================================================== */

void gpio_set_af(GPIO_TypeDef *gpio, uint8_t pin, uint8_t af_num)
{
    /* TODO：如果引脚 < 8：修改 AFRL
     * 否则：修改 AFRH （使用引脚 - 8 作为位偏移量）
     * 每个 AF 字段为 4 位宽 */
    (void)gpio; (void)pin; (void)af_num;
}

/* ===============================================================
 * TASK 4 — 全引脚配置助手
 * 在一次调用中配置一个引脚——真实代码中的常见模式。
 * =============================================================== */

typedef struct {
    uint8_t mode;
    uint8_t otype;
    uint8_t speed;
    uint8_t pull;
    uint8_t af;
} GPIO_PinConfig;

void gpio_configure_pin(GPIO_TypeDef *gpio, uint8_t pin, const GPIO_PinConfig *cfg)
{
    /* TODO：按以下顺序调用所有设置器：
     * 1. gpio_set_mode（先设置为 INPUT，然后再更改其他）
     * 2.gpio_set_output_type
     * 3.gpio_set_speed
     * 4.gpio_set_pull
     * 5. 如果模式== GPIO_MODE_AF: gpio_set_af */
    (void)gpio; (void)pin; (void)cfg;
}

/* ===============================================================
 * TASK 5 — 真实世界：在 STM32F4 上配置 USART2 引脚
 *
 * USART2：
 * PA2 = TX (AF7)
 * PA3 = RX (AF7)
 * 两者：AF模式，高速，推拉，无拉
 * =============================================================== */

void init_usart2_pins(void)
{
    /* TODO：使能GPIOA时钟 */
    /* TODO：配置PA2为AF7，输出，高速，推挽，无拉 */
    /* TODO：配置PA3为AF7，输出，高速，推挽，上拉（用于RX） */
}

/* ===============================================================
 * TASK 6 — BUG HUNT：GPIO 配置错误
 *
 * 以下函数尝试配置 I2C SDA 引脚（漏极开路）。
 * 它有 3 个错误。找到并标记每一个。
 * =============================================================== */

void configure_i2c_sda_BUGGY(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* 错误1：？？？ */
    /* 访问 GPIO 寄存器之前未启用 RCC 时钟 */

    /*错误2：？？？ */
    gpio->MODER |= (GPIO_MODE_AF << (pin * 2));   /* 应先CLEAR，然后设置*/

    /* 错误3：？？？ */
    gpio->OTYPER &= ~(1u << pin);   /*设置推挽 — I2C SDA MUST 为漏极开路 */
                                     /* 应该是：gpio->OTYPER |=（1u << 引脚） */

    gpio_set_af(gpio, pin, 4);   /* AF4 = I2C on STM32F4 — 这是正确的 */
    gpio_set_pull(gpio, pin, GPIO_PULL_UP);   /* 正确 — I2C 需要上拉 */
}

/* ===============================================================
 * SELF-TEST
 * =============================================================== */

static void test_gpio(void)
{
    /* 重置 */
    GPIOA->MODER = 0;
    GPIOA->ODR   = 0;
    GPIOA->BSRR  = 0;

    /* 测试模式配置 */
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    assert((GPIOA->MODER & (0b11u << 10)) == (GPIO_MODE_OUTPUT << 10));

    /* 通过 BSRR 测试原子集 */
    gpio_write_pin(GPIOA, 5, 1);
    /* 应设置 BSRR 位 5 */
    assert(GPIOA->BSRR & (1u << 5));

    /* 测试拉动配置 */
    gpio_set_pull(GPIOA, 3, GPIO_PULL_UP);
    assert((GPIOA->PUPDR & (0b11u << 6)) == (GPIO_PULL_UP << 6));

    printf("All GPIO tests PASSED.\n");
}

int main(void)
{
    test_gpio();
    return 0;
}

/* ===============================================================
 * INTERVIEW QUESTIONS
 * ===============================================================
 *
 * Q1：为什么 GPIO 输出使用 BSRR 而不是 ODR？什么比赛条件
 * BSRR 消除吗？
 * 答案：TODO
 *
 * Q2: 您将 PA5 配置为 OUTPUT，但读取 IDR.5 始终返回 0。
 * 可能出了什么问题？
 * 答案：TODO
 *
 * Q3: 如果忘记启用 GPIO 外设时钟会发生什么情况
 * 在写入寄存器之前？
 * 答案：TODO
 *
 * Q4：您需要从 3.3V MCU 到 5V I2C 总线上有一个开漏输出。
 * 如何配置它以及为什么？
 * 答案：TODO
 *
 * Q5: STM32 上可以同时设置多少个 GPIO 引脚
 * 使用BSRR？这个操作真的是原子操作吗？
 * 答案：TODO*/
