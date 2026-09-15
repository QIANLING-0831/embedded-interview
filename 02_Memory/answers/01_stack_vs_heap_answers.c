/** ANSWERS：02_内存/01_stack_vs_heap.c
 * =============================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ===============================================================
 * TASK 1 — 可变分类答案
 * ===============================================================
 *
 * int g_counter = 42;          → .data（全局初始化，启动时从闪存复制）
 * int g_uninitialized;         → .bss（全局清零，启动时清零）
 * const int g_config_value=100;→ .rodata / flash（const global — 保留在 ROM 中）
 *
 * 无效任务1_分类（无效）{
 * int local_var = 5;         → 堆栈（局部变量，位于堆栈帧中）
 * 静态 int 持续 = 0;    → .data（静态本地——调用后仍然存在，NOT 在堆栈上）
 * int *dyn = malloc(64);     → 堆栈上的指针； HEAP 上指向的内存
 * }
 */

int       g_counter       = 42;       /* .data */
int       g_uninitialized;            /* .bss  */
const int g_config_value  = 100;      /* .rodata（闪存） */

void task1_classify(void)
{
    int        local_var = 5;         /* 堆栈 */
    static int persist   = 0;         /* .data — 静态！ */
    int       *dyn       = malloc(64);/* dyn → 栈，*dyn → 堆 */
    (void)local_var; (void)persist;
    if (dyn) free(dyn);
}

/* ===============================================================
 * TASK 2 — 迭代实现
 * =============================================================== */

uint32_t fib_iterative(uint32_t n)
{
    if (n <= 1) return n;
    uint32_t prev = 0, curr = 1;
    for (uint32_t i = 2; i <= n; i++) {
        uint32_t next = prev + curr;
        prev = curr;
        curr = next;
    }
    return curr;
}

uint32_t power_iterative(uint32_t base, uint32_t exp)
{
    uint32_t result = 1;
    while (exp--) result *= base;
    return result;
}

/* ===============================================================
 * TASK 3 — 内存池
 * =============================================================== */

#define POOL_BLOCK_SIZE  32u
#define POOL_NUM_BLOCKS  16u

typedef struct {
    uint8_t  storage[POOL_NUM_BLOCKS][POOL_BLOCK_SIZE];
    uint8_t  used[POOL_NUM_BLOCKS];
    uint8_t  num_free;
} MemPool;

void pool_init(MemPool *pool)
{
    memset(pool->storage, 0, sizeof(pool->storage));
    memset(pool->used,    0, sizeof(pool->used));
    pool->num_free = POOL_NUM_BLOCKS;
}

void *pool_alloc(MemPool *pool)
{
    for (uint8_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (!pool->used[i]) {
            pool->used[i] = 1;
            pool->num_free--;
            return pool->storage[i];
        }
    }
    return NULL;
}

void pool_free(MemPool *pool, void *ptr)
{
    uint8_t *p = (uint8_t *)ptr;
    for (uint8_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (pool->storage[i] == p) {
            if (!pool->used[i]) return;  /* 双免保护 */
            pool->used[i] = 0;
            pool->num_free++;
            return;
        }
    }
}

uint8_t pool_num_free(const MemPool *pool) { return pool->num_free; }

/* ===============================================================
 * TASK 4 — 堆栈水印
 * =============================================================== */

#define FAKE_STACK_SIZE  256u
static uint8_t fake_stack[FAKE_STACK_SIZE];

void stack_paint(void)
{
    memset(fake_stack, 0xA5, FAKE_STACK_SIZE);
}

uint32_t stack_get_high_water_mark(void)
{
    /* 计算 END 中仍然是 0xA5 的字节数（从未触及） */
    uint32_t unused = 0;
    for (int i = (int)FAKE_STACK_SIZE - 1; i >= 0; i--) {
        if (fake_stack[i] == 0xA5u) unused++;
        else break;
    }
    return unused;
}

/* ===============================================================
 * TASK 5 — 链接器部分属性 ANSWERS
 * ===============================================================
 *
 * __attribute__((section(".ccm"))) uint8_t fast_dma_buffer[1024];
 * → 将缓冲区放置在 STM32F4 上的核心耦合内存中（0x10000000 处为 64 KB）。
 * → CPU 访问速度比 SRAM1 快； DMA CANNOT 访问 CCM。
 *
 * __attribute__((section(".ramfunc"))) void flash_unlock_sequence(void)
 * → 将函数放置在 SRAM 中，以便在闪存擦除期间从 RAM 执行。
 * → 在闪存编程期间，从闪存执行是未定义的行为。
 * → 启动代码必须将 .ramfunc 部分从闪存复制到 SRAM。
 */

__attribute__((section(".ccm_sim")))
uint8_t fast_dma_buffer[1024];         /* 真实目标上的“.ccm” */

__attribute__((section(".ramfunc_sim")))
void flash_unlock_sequence(void) {}    /* 真实目标上的“.ramfunc”*/

/*===============================================================
 * TASK 6 — Bug 搜寻 FIXED
 *
 * Bug 1: malloc(sizeof(Packet)) 之后没有 NULL 检查
 * 如果堆已满，p 为 NULL。 p->data = malloc(len) → 崩溃（NULL deref）。
 * FIX：如果（！p）返回NULL；
 *
 * Bug 2：malloc(0) 是实现定义的（可能返回 NULL 或唯一指针）。
 * 使用 NULL src/dst 调用 memcpy 是 UB。
 * FIX: if (len == 0) { p->data = NULL; p->len = 0；返回p； }
 * 或: malloc(len > 0 ? len : 1);
 *
 * Bug 3（销毁中）：在访问 p->data 之前没有对 p 进行 NULL 检查。
 * 如果p是NULL：p->数据崩溃。
 * FIX: if (!p) 返回;
 *
 * Bug 4: 释放后 p->data = NULL(p)。写入已释放的内存是 UB。
 * p 所在位置的内存可能已被其他分配重用。
 * FIX：调用者应清空其指针：caller_ptr = NULL;
 * 或者传递指针到指针： void destroy(Packet **pp)
 * { 自由((*pp)->数据);免费（*页）； *pp = NULL； }
 * =============================================================== */

typedef struct { uint8_t *data; uint16_t len; } Packet;

Packet *create_packet_fixed(const uint8_t *src, uint16_t len)
{
    Packet *p = malloc(sizeof(Packet));
    if (!p) return NULL;                    /* 错误1修复 */
    if (len == 0) {
        p->data = NULL; p->len = 0;
        return p;                           /* 错误2修复 */
    }
    p->data = malloc(len);
    if (!p->data) { free(p); return NULL; }
    memcpy(p->data, src, len);
    p->len = len;
    return p;
}

void destroy_packet_fixed(Packet **pp)
{
    if (!pp || !*pp) return;               /* 错误3修复 */
    free((*pp)->data);
    free(*pp);
    *pp = NULL;                            /* Bug 4修复：调用者的指针为空*/
}

/*===============================================================
 * INTERVIEW QUESTION ANSWERS
 * ===============================================================

Q1：为什么安全标准（IEC 61508、MISRA）禁止动态分配？

A: (1) 非确定性时序：malloc() 运行时取决于堆状态 — 可以
       需要微秒或毫秒，这在实时系统中是不可接受的。
   (2) 碎片：经过多次分配/释放循环后，空闲内存存在于
       不连续的块。即使总计，大量分配也可能会失败
       可用字节> 请求的大小。
   (3)无故障恢复：嵌入式系统不能显示“内存不足”对话框。
       malloc 失败 → NULL → 如果不检查则崩溃。
   (4) 难以分析：静态分析工具无法证明内存安全
       动态分配。
   替代方案：启动时静态分配，固定大小的内存池。

Q2：.data 和 .bss 之间的区别？为什么.bss不占用闪存空间？

答：.data：初始化的全局变量（int x = 5）。值 5 必须位于闪存中，因此
   启动代码可以复制到SRAM。 Flash 包含：[.data 的初始值]。
   .bss：零初始化的全局变量（int y;）。整个 .bss 区域归零
   通过启动代码 - 无需在闪存中存储零（零是隐式的）。
   Flash 仅存储 .bss 的起始地址和大小，因此启动代码知道
   多少到零。保存与 .bss 大小成比例的闪存。

Q3：FreeRTOS 堆栈水印 = 12 个字。安全的？

答：12 个字 = 剩余 48 个字节。是否安全取决于上下文：
   - 如果任务永远不会更深地递归或创建更大的局部变量：可能没问题。
   - 48 字节很紧张——使用本地数组的单个嵌套函数调用
     在异常情况下可能会溢出。
   规则：水印应大于总堆栈的 10%，或至少 64 字节。
   措施：将堆栈大小增加 50% 并重新测量。切勿以 < 20 个字的长度发货。

问题 4：堆栈溢出如何损坏裸机上的堆？

A：堆栈向下增长。堆向上增长。在典型的嵌入式布局上：
   [.bss][HEAP→][...免费...][←STACK]
   如果堆栈增长超过堆栈区域的底部，则进入
   自由空间，然后是堆。 malloc() 内部元数据（块头、
   空闲列表指针）被堆栈帧覆盖。
   结果：下一个 malloc() 或 free() 调用会损坏堆 → 崩溃或静默
   出现在完全不相关的代码路径中的内存损坏。

问题 5：什么是内存碎片？为什么它在长时间运行的系统中很重要？

答：经过多次大小不同的 malloc/free 循环后：
   免费：[4KB][2KB][4KB][2KB][4KB][2KB]（总共18KB免费）
   请求：malloc(6KB) → FAILS（不存在单个 6KB 块）
   这就是碎片——空闲内存存在，但变成了无法使用的碎片。
   嵌入式：运行数月/数年的设备（工业、医疗）将
   最终将其堆碎片化到分配失败的程度，甚至
   尽管技术上有足够的内存。必须重新启动系统 —
   对于关键系统来说是不可接受的。
   解决方案：内存池（所有块大小相同→无碎片）。

Q6：4KB SRAM，10 个并发传感器读数，每个读数 64 字节。设计？

A：使用静态内存池：
   静态uint8_t传感器_池[10][64]；  // 640 字节 = 4KB 的 15.6%
   静态 uint8_t pool_used[10] = {0}；
   alloc()：扫描pool_used是否有空闲槽，返回指针。平均时间为 O(1)。
   free()：清除pool_used位。
   剩余 3456 字节：堆栈（每个任务 512B × 5 个任务 = 2560B）+ .bss + 堆。
   切勿在具有 4KB SRAM 的系统中使用 malloc()。*/

int main(void)
{
    assert(fib_iterative(0)  == 0);
    assert(fib_iterative(1)  == 1);
    assert(fib_iterative(10) == 55);
    assert(fib_iterative(20) == 6765);

    assert(power_iterative(2, 10) == 1024);
    assert(power_iterative(3, 3)  == 27);

    MemPool pool;
    pool_init(&pool);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS);

    void *a = pool_alloc(&pool);
    void *b = pool_alloc(&pool);
    assert(a && b && a != b);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 2);

    pool_free(&pool, a);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);
    pool_free(&pool, a);   /*双重释放——必须被忽略 */
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);

    stack_paint();
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE);
    fake_stack[255] = 0x00;   /* 在顶部模拟堆栈使用情况*/
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE - 1);

    uint8_t src[] = {1,2,3,4};
    Packet *pkt = create_packet_fixed(src, 4);
    assert(pkt && pkt->data && pkt->len == 4);
    assert(pkt->data[2] == 3);
    destroy_packet_fixed(&pkt);
    assert(pkt == NULL);

    printf("All memory answers verified.\n");
    return 0;
}
