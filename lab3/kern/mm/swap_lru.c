#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>

/* [wikipedia]The simplest Page Replacement Algorithm(PRA) is a FIFO algorithm. The first-in, first-out
 * page replacement algorithm is a low-overhead algorithm that requires little book-keeping on
 * the part of the operating system. The idea is obvious from the name - the operating system
 * keeps track of all the pages in memory in a queue, with the most recent arrival at the back,
 * and the earliest arrival in front. When a page needs to be replaced, the page at the front
 * of the queue (the oldest page) is selected. While FIFO is cheap and intuitive, it performs
 * poorly in practical application. Thus, it is rarely used in its unmodified form. This
 * algorithm experiences Belady's anomaly.
 *
 * Details of FIFO PRA
 * (1) Prepare: In order to implement FIFO PRA, we should manage all swappable pages, so we can
 *              link these pages into pra_list_head according the time order. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list
 *              implementation. You should know howto USE: list_init, list_add(list_add_after),
 *              list_add_before, list_del, list_next, list_prev. Another tricky method is to transform
 *              a general list struct to a special struct (such as struct page). You can find some MACRO:
 *              le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.
 */

extern list_entry_t pra_list_head;
uintptr_t required_addr; // 通过addr提前告知需要的地址
/*
 * (2) _fifo_init_mm: init pra_list_head and let  mm->sm_priv point to the addr of pra_list_head.
 *              Now, From the memory control struct mm_struct, we can access FIFO PRA
 */
static int
_lru_init_mm(struct mm_struct *mm)
{
    /*LAB3 EXERCISE 5: 2212602*/
    // 初始化pra_list_head为空链表
    // cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
    list_init(&pra_list_head);
    mm->sm_priv = &pra_list_head;

    return 0;
}
/*
 * (3)_fifo_map_swappable: According FIFO PRA, we should link the most recent arrival page at the back of pra_list_head qeueue
 */
static int
_lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    list_entry_t *entry = &(page->pra_page_link);

    assert(entry != NULL && head != NULL);
    // record the page access situlation
    /*LAB3 EXERCISE 5: 2212602*/
    // 查找原来的页面链表中是否有该page，有则删除。
    // 将页面page插入到页面链表pra_list_head的末尾
    list_entry_t *ptr = head->prev;
    list_entry_t *le = head->next;
    while (le != head) {
        struct Page *p = le2page(le, pra_page_link);
        le = le->next;
    }
    ptr = head->prev;
    while (ptr != head)
    {
        struct Page *list_page = le2page(ptr, pra_page_link);
        if (list_page->pra_vaddr == addr)
        {
            list_del(ptr);
            break;
        }
        ptr = ptr->prev;
    }
    list_add(head, entry);
    return 0;
}
/*
 *  (4)_fifo_swap_out_victim: According FIFO PRA, we should unlink the  earliest arrival page in front of pra_list_head qeueue,
 *                            then set the addr of addr of this page to ptr_page.
 */
static int
_lru_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    /* Select the victim */
    //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
    //(2)  set the addr of addr of this page to ptr_page
    list_entry_t *entry = list_prev(head);
    entry = list_prev(head);
    if (entry != head)
    {
        list_del(entry);
        *ptr_page = le2page(entry, pra_page_link);
    }
    else
    {
        *ptr_page = NULL;
    }
    return 0;
}

void access_memory(uintptr_t addr, unsigned char value)
{
    *(unsigned char *)addr = value;
    addr = (addr + 0xFFF) & ~0xFFF; // Round up to the nearest multiple of 0x1000
    _lru_map_swappable(check_mm_struct, addr, get_page(check_mm_struct->pgdir, addr, 0), 0);
}

static int
_lru_check_swap(void)
{
#ifdef ucore_test
    int score = 0, totalscore = 5;
    cprintf("%d\n", &score);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x2000 = 0x0b;
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    assert(pgfault_num == 4);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 5);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 5);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
#else
    access_memory(0x3000, 0x0c);
    cprintf("page fault num: %d\n", pgfault_num);
    assert(pgfault_num == 4);
    access_memory(0x1000, 0x0a);
    assert(pgfault_num == 4);
    access_memory(0x4000, 0x0d);
    assert(pgfault_num == 4);
    access_memory(0x2000, 0x0b);
    assert(pgfault_num == 4);
    access_memory(0x5000, 0x0e);
    assert(pgfault_num == 5);
    access_memory(0x2000, 0x0b);
    assert(pgfault_num == 5);
    access_memory(0x1000, 0x0a);
    assert(pgfault_num == 5);
    access_memory(0x2000, 0x0b);
    assert(pgfault_num == 5);
    access_memory(0x3000, 0x0c);
    assert(pgfault_num == 6);
    access_memory(0x4000, 0x0d);
    assert(pgfault_num == 7);
    access_memory(0x5000, 0x0e);
    assert(pgfault_num == 8);
    assert(*(unsigned char *)0x1000 == 0x0a);
    access_memory(0x1000, 0x0a);
    assert(pgfault_num == 9);
#endif
    return 0;
}

static int
_lru_init(void)
{
    return 0;
}

static int
_lru_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_lru_tick_event(struct mm_struct *mm)
{
    return 0;
}

struct swap_manager swap_manager_lru =
    {
        .name = "lru swap manager",
        .init = &_lru_init,
        .init_mm = &_lru_init_mm,
        .tick_event = &_lru_tick_event,
        .map_swappable = &_lru_map_swappable,
        .set_unswappable = &_lru_set_unswappable,
        .swap_out_victim = &_lru_swap_out_victim,
        .check_swap = &_lru_check_swap,
};
