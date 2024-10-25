// YOUR CODE: 2212602

#include <buddy2_fit_pmm.h>
#include <list.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// 这块没法malloc分配，写个内存池
#define MAX_BUDDY2_SIZE 4096 * 16
unsigned alloc_pool[MAX_BUDDY2_SIZE];

#define ALLOC(size) ((void *)alloc_pool)
#define IS_POWER_OF_2(i) ((i) & ((i) - 1)) == 0
#define LEFT_CHILD(i) (2 * i + 1)
#define RIGHT_CHILD(i) (2 * i + 2)
#define PARENT(i) ((i - 1) / 2)

struct Buddy2
{
    unsigned size;
    unsigned longest[1];
} *buddy2;
struct Page *basePage;

typedef struct Buddy2 Buddy2;

/**
 * @brief 初始化一个Buddy2对象
 *
 * @note
 * 借助柔性数组的特性，通过手动分配内存来实现。
 * 由于malloc函数不被支持，手动模拟一个内存池来实现分配，经过测试的大小为4096*16。
 *
 * @param size Buddy2对象的页框数
 */
Buddy2 *buddy2_new(int size)
{
    Buddy2 *self;
    unsigned node_size;
    int i;

    if (size < 1 || !IS_POWER_OF_2(size))
        return NULL;

    self = (Buddy2 *)ALLOC(2 * size * sizeof(unsigned));
    self->size = size;
    node_size = size * 2;

    for (i = 0; i < 2 * size - 1; ++i)
    {
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2;
        self->longest[i] = node_size;
    }
    return self;
}

/**
 * @brief 分配一个大小为size的内存块
 * 使用buddy算法分配内存，返回内存块的偏移量
 * @note 如果size不是2的幂次方，则向上取整
 * @param self Buddy2对象
 * @param size 内存块的大小
 */
int buddy2_alloc(Buddy2 *self, int size)
{
    unsigned index = 0;
    unsigned node_size;
    unsigned offset = 0;

    if (self == NULL)
        return -1;

    if (size <= 0)
        size = 1;
    else if (!IS_POWER_OF_2(size))
        size = fix_size(size);

    if (self->longest[index] < size)
        return -1;

    for (node_size = self->size; node_size != size; node_size /= 2)
    {
        if (self->longest[2 * index + 1] >= size)
            index = LEFT_CHILD(index);
        else
            index = RIGHT_CHILD(index);
    }

    self->longest[index] = 0;
    offset = (index + 1) * node_size - self->size;

    // 回溯的作用是：通过更新整个路径上的节点，确保无法分配时能正确返回
    while (index)
    {
        index = PARENT(index);
        self->longest[index] = MAX(self->longest[LEFT_CHILD(index)], self->longest[RIGHT_CHILD(index)]);
    }

    return offset;
}

/**
 * @brief 释放一个内存块
 *
 * @param self Buddy2对象
 * @param offset 内存块的偏移量
 */
void buddy2_free(Buddy2 *self, int offset)
{
    unsigned node_size, index = 0;
    unsigned left_longest, right_longest;

    assert(self && offset >= 0 && offset < self->size);

    node_size = 1;
    index = offset + self->size - 1;

    for (; self->longest[index]; index = PARENT(index))
    {
        node_size *= 2;
        if (index == 0)
            return;
    }

    self->longest[index] = node_size;

    while (index)
    {
        index = PARENT(index);
        node_size *= 2;

        left_longest = self->longest[LEFT_CHILD(index)];
        right_longest = self->longest[RIGHT_CHILD(index)];

        if (left_longest + right_longest == node_size)
            self->longest[index] = node_size;
        else
            self->longest[index] = MAX(left_longest, right_longest);
    }
}

/**
 * @brief 将size向上取整为2的幂次方
 *
 * @param size
 * @return int
 */
int fix_size(int size)
{
    int i = 1;
    while (i < size)
        i <<= 2;
    return i;
}

/**
 * @brief 初始化函数
 * @note 由于未使用free_list，因此不需要做任何初始化动作
 */
void buddy2_fit_init(void)
{
}

/**
 * @brief 初始化内存映射
 *
 * @note
 * 默认标记每个页框都可使用，
 * 且大小为2的幂次方，
 * 具体大小取决于树上的位置
 *
 * @param base 内存映射的起始地址
 * @param n 内存映射的页框数
 */
void buddy2_fit_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);

    if (!IS_POWER_OF_2(n))
        n = fix_size(n >> 2);
    buddy2 = buddy2_new(n);
    assert(buddy2 != NULL);

    int i;
    int node_size = n << 2;
    struct Page *p = base;
    for (i = 0; p != base + n; ++i, ++p)
    {
        assert(PageReserved(p));
        p->flags = 0;
        set_page_ref(p, 0);
        p->property = buddy2->longest[i];
        SetPageProperty(p);
    }
    base->property = n;
    SetPageProperty(base);

    basePage = base;
}

/**
 * @brief dfs方式便历树并清除页框可用数
 * @note
 * 递归遍历树，清除每个节点的可用页框数，直到叶子节点，
 * 这样做的原因在于所有的节点都标记可用，因而在使用了非叶节点时需要屏蔽其所有子节点
 * @param base 内存映射的起始地址
 * @param offset 当前节点的偏移量
 * @param size 内存映射的页框数
 */
void dfsClearPageProperty(struct Page *base, int offset, int size)
{
    if (offset >= size)
        return;

    ClearPageProperty(base + offset);
    dfsClearPageProperty(base, LEFT_CHILD(offset), size);
    dfsClearPageProperty(base, RIGHT_CHILD(offset), size);
}

/**
 * @brief dfs方式便历树并设置页框可用数
 * @param base 内存映射的起始地址
 * @param offset 当前节点的偏移量
 * @param size 内存映射的页框数
 */
void dfsSetPageProperty(struct Page *base, int offset, int size)
{
    if (offset >= size)
        return;

    base[offset].property = buddy2->longest[offset];
    SetPageProperty(base + offset);
    dfsSetPageProperty(base, LEFT_CHILD(offset), size);
    dfsSetPageProperty(base, RIGHT_CHILD(offset), size);
}

/**
 * @brief 分配n个页框
 *
 * @param n 页框数
 * @return struct Page*
 */
struct Page *buddy2_fit_alloc_pages(size_t n)
{
    assert(n > 0);
    struct Page *page = NULL;
    int offset = buddy2_alloc(buddy2, n);

    if (offset == -1)
        return NULL;

    page = &basePage[offset];

    dfsClearPageProperty(basePage, offset, buddy2->size);

    return page;
}

/**
 * @brief 释放n个页框
 *
 * @note 释放n个页框，同时重设页框的属性
 * @param base 起始页框
 * @param n 页框数
 */
void buddy2_fit_free_pages(struct Page *base, size_t n)
{
    assert(n > 0);
    struct Page *p = base;
    int offset = p - basePage;
    buddy2_free(buddy2, offset);
    for (; p != base + n; p++)
    {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }

    // 从偏移位置，即变化的节点开始恢复属性
    // dfsSetPageProperty(basePage, offset, buddy2->size);

    base->property = 0;
    SetPageProperty(base);
}

/**
 * @brief 返回空闲页框数
 * @note 此处仅做适配
 * @return size_t
 */
size_t buddy2_fit_nr_free_pages(void)
{
    return buddy2->size;
}

/**
 * @brief 检查函数
 * @note 用于检查是否能正常分配和释放内存
 */
void basic_check(void)
{
    // 分配4个Page，大小分别为4096*2, 4096, 2048, 2048，共计4*4096=16384
    struct Page *p0, *p1, *p2, *p3;
    p0 = p1 = p2 = p3 = NULL;
    // cprintf("HELLO");
    assert((p0 = alloc_pages(4096 * 2)) != NULL);
    assert((p1 = alloc_pages(4096)) != NULL);
    assert((p2 = alloc_pages(2048)) != NULL);
    assert((p3 = alloc_pages(2048)) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2 && p0 != p3 && p1 != p3 && p2 != p3);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);
    assert(page2pa(p3) < npage * PGSIZE);

    // for (size_t i = 0; i < npage - nbase; i++)
    // {
    //     cprintf("%d\n", buddy2->longest[i]);
    // }
    assert(alloc_page() == NULL);

    free_pages(p0, 4096 * 2);
    free_pages(p1, 4096);
    free_pages(p2, 2048);
    free_pages(p3, 2048);
    // cprintf("%d\n", buddy2->longest[0]);
    assert(fix_size(16384) == buddy2->size);

    assert((p0 = alloc_pages(4096 * 2)) != NULL);
    assert((p1 = alloc_pages(4096)) != NULL);
    assert((p2 = alloc_pages(2048)) != NULL);
    assert((p3 = alloc_pages(2048)) != NULL);

    assert(alloc_page() == NULL);

    struct Page *p;
    // assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    free_pages(p0, 4096 * 2);
    free_pages(p1, 4096);
    free_pages(p2, 2048);
    free_pages(p3, 2048);
}

/**
 * @brief 检查函数
 * @note 用于检查是否能正常分配和释放内存
 * 由于未使用free_list以及相关的变量，因而无法做更多的检查
 */
void buddy2_fit_check(void)
{
    basic_check();
}

const struct pmm_manager buddy2_fit_pmm_manager = {
    .name = "buddy2_fit_pmm_manager",
    .init = buddy2_fit_init,
    .init_memmap = buddy2_fit_init_memmap,
    .alloc_pages = buddy2_fit_alloc_pages,
    .free_pages = buddy2_fit_free_pages,
    .nr_free_pages = buddy2_fit_nr_free_pages,
    .check = buddy2_fit_check,
};