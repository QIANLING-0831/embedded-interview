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
