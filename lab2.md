
### 练习

对实验报告的要求：
 - 基于markdown格式来完成，以文本方式为主
 - 填写各个基本练习中要求完成的报告内容
 - 完成实验后，请分析ucore_lab中提供的参考答案，并请在实验报告中说明你的实现与参考答案的区别
 - 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
 - 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

#### 练习0：填写已有实验

本实验依赖实验1。请把你做的实验1的代码填入本实验中代码中有“LAB1”的注释相应部分并按照实验手册进行进一步的修改。具体来说，就是跟着实验手册的教程一步步做，然后完成教程后继续完成完成exercise部分的剩余练习。

#### 练习1：理解first-fit 连续物理内存分配算法（思考题）
first-fit 连续物理内存分配算法作为物理内存分配一个很基础的方法，需要同学们理解它的实现过程。请大家仔细阅读实验手册的教程并结合`kern/mm/default_pmm.c`中的相关代码，认真分析default_init，default_init_memmap，default_alloc_pages， default_free_pages等相关函数，并描述程序在进行物理内存分配的过程以及各个函数的作用。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：

答：first-fit连续物理内存分配算法是一种十分简单的内存分配方法，它根据内存空闲块的起始位置从低地址到高地址进行扫描，并在找到第一个满足请求大小的空闲块时进行分配。我们在下方列出了first-fit算法较为具体的实现过程和工作原理：
- **准备工作---空闲块链式管理**：在此算法下，操作系统应维护一个空闲块链表或数组，以记录每个空闲内存块的起始地址和大小，其中链表内的节点按内存地址从低到高排序。
- **产生内存需求---扫描空闲块链表**：当需要分配一块大小为requested_size的内存时，指针会从空闲块链表的头开始扫描，依次检查每个空闲块的大小是否能够满足该请求。
- **满足内存需求---找到合适的空闲块**：在扫描过程中，如果找到一个空闲块current_block的大小 current_block.size大于等于requested_size时，则视为找到合适的空闲块。
- **处理内存块---进行分配**：如果找到的空闲块大小刚好等于requested_size，则直接将该空闲块从链表中移除；如果找到的空闲块大小大于 requested_size，则将该空闲块依所需内存大小分割成两部分，并将大小为requested_size的内存移除，将另一部分继续保留在链表中，并在必要时与邻近的内存块合并。
- **完成分配---返回分配结果**：分配成功后，返回被分配内存块的起始地址。
- **分配失败---返回失败状态**：若是扫描到链表尾部仍然没有找到满足的空闲块，则返回分配失败的状态。
  
下面伴着对first fit算法原理的初步理解，我们对`kern/mm/default_pmm.c`中的相关代码进行了认真的阅读与分析，初步理解了其中对于first fit算法的实现过程与各个函数的作用，下面我们一一分析：
##### 1. default_init：初始化整个内存分配系统的状态
```C
default_init(void) {
  list_init(&free_list);
  nr_free = 0;
  }
 ```
该函数对用于存储空闲内存块的链表free_list进行初始化，并将记录链表中所有内存块总页数量的变量nr_free设置为0。

##### 2. default_init_memmap：将一段连续的内存页初始化为一个空闲块，并插入到空闲链表free_list中。
```C++
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}
```
- 函数首先接受参数：一个struct Page *类型的基地址base和一个大小为n的变量。
- 在确认传入的n大于0后，函数遍历从base开始的n个页，确保每一页最初处于"保留"状态，同时清除当前页的flags和property字，并将每个页的引用计数设为 0，完成初始化。
- 然后对第一个页进行修改，设置其PG_property标志位，以表示其是头部页，同时设置第一个页的property字段，以表示该空闲块大小为n。
- 最后便是依据空闲块链表是否为空，进行新空闲块的插入，操作模式我们都很熟悉了
  
##### 3. default_alloc_pages：根据first-fit算法找到合适的空闲块并进行分配。
```C
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```
- 函数接受参数n作为内存分配的目标，并对n的大小进行检查，大于0且小于总内存页数nr_free
- 然后指针指向链表，开始进行遍历，对每个内存块，首先使用使用函数le2page从链表节点le获取对应的内存页结构p，并检查当前内存块p的property项是否大于或等于请求内存大小n。如果是，则认为找到了合适的内存块，将page设为p并停止向下遍历
- 若在上述步骤后找到一个合适的内存块，则首先将其移出链表，然后检查page的大小是否大于n，若大于n，则计算剩余部分的起始地址 p，并在更新剩余部分的property字段后，将剩余部分p插入到链表原位置中
- 最后返回找到并分配的内存块的指针 page。
  
##### 4. default_free_pages：释放分配的内存，并尝试合并相邻的空闲块，以减少内存碎片。
```C
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```
- 函数接受参数：一个struct Page *类型的基地址base和一个大小为n的变量，释放base开始，大小为n的内存变量，函数会首先检查n大于0，然后遍历要释放的每一页的PageReserved和PageProperty，确保这些页没有被标记为"保留"且不是“头部页”，最后重置每一页的flags为0，并将页的引用计数设为0，表示该页现为空闲状态
- 然后函数像前述函数一样进行头部页相关属性设置，并更新全局页数nr_free
- 然后函数将空闲块插入，步骤仍同前
- 最后函数尝试将新插入的块与前后块进行合并，在获取获取base块前面的节点le后，进行头结点的排除，此时若发现本块与前/后块地址相邻，则更新前/本块p的 property字段，使其包含base/后块的大小，接着清除base/后块的PG_property标志，表示base/后块第一页不再是一个块头部页，最后将base/后块从链表中删除
  

#### 练习1.5：你的first fit算法是否有进一步的改进空间？
##### 1. 内存碎片问题
first fit算法从空闲链表的头部开始查找合适的块，往往会在内存的前部频繁进行分配。长时间运行后，可能会在低地址区域留下大量的小碎片，使得高地址的较大块无法被充分利用。这种内存碎片会导致内存利用率下降。

改进方法：
- **next fit算法**：改进first fit算法的遍历策略，不总是从链表头部开始，而是从上次分配的地方继续查找下一个合适的空闲块。Next-Fit 能在一定程度上减少低地址区域的碎片。
- **best fit算法**：不同于first fit总是寻找第一个合适的块，Best-Fit 算法寻找最接近所需大小的块进行分配，从而减少碎片的产生。尽管 Best-Fit 算法在搜索上稍慢，但它在减少内存碎片方面通常比first fit更有效。
- **buddy system**：将内存划分为大小为2的幂次的块，在释放时将相同大小且相邻的块合并，形成更大的内存块，从而减少碎片。

##### 2. 线性搜索导致的时间复杂度问题
first fit需要线性遍历空闲链表来查找合适的内存块，链表越长，搜索时间越长。当系统中空闲块的数量增加时，这种线性查找的效率就会下降。

改进方法:
- **分区空闲链表**：将空闲链表根据块的大小划分为多个链表，按照大小分类，如小块链表、中块链表、大块链表。这样可以在适当的链表中快速查找合适的内存块，避免线性遍历整个链表。
- **使用更高效的数据结构**：例如使用平衡树,如AVL树或红黑树,来管理空闲块，这样能够快速查找和插入合适的块，查找效率由线性降低到对数级别。
- **快速查找算法**：比如在空闲链表中维护一个指向"最佳起始位置"的指针，可以利用类似指针回溯的方法，快速找到最近一个合适大小的块，减少遍历。

##### 3. 释放时合并空闲块的复杂度
在first fit算法中，释放内存时需要对空闲链表进行插入和合并操作，这些操作可能会导致性能下降，特别是当内存碎片较多且链表较长时。

改进方法:
- **双链表策略**：可以将空闲块分为"刚分配完尚未合并"的链表和"已经合并好"的链表。新释放的块先加入到未合并的链表中，定期对未合并链表进行批量合并，减少在释放时的合并操作。
- **延迟合并**：不在每次释放时立即合并相邻的块，而是在达到一定阈值后或者在内存分配时再进行合并，这样可以减少释放操作的开销。

##### 4. 不灵活的分配策略
first fit只寻找第一个合适的块进行分配，而不考虑更合适的内存块的使用场景。比如，可能会导致大块内存被小请求分配占用，而当稍后需要分配更大块的内存时却没有合适的空闲块。

改进方法:
- **启发式的块选择策略**：在first fit算法的基础上引入启发式策略，优先选择靠近中间或高地址区域的空闲块，减少低地址区域的碎片。
- **动态块大小调整**：允许在空闲块链表中动态调整块的大小，根据当前的内存使用情况将大的块拆分为小的块，或者将小的块合并为大的块，以适应不同的分配需求。

##### 5. 内存碎片的回收和整理
在长期运行的系统中，即使有碎片合并机制，也可能产生大量小碎片，特别是当存在频繁的内存分配和释放时。这些碎片最终可能导致内存不足的问题。

改进方法:
- **周期性碎片整理**：操作系统可以在空闲时或内存压力较大时主动整理碎片，将小碎片合并为大块空闲块，类似于磁盘的碎片整理。
- **内存压缩技术**：使用内存压缩技术将分散的空闲块压缩成连续的块，从而提高内存的可用性。

##### 总结

first fit算法简单高效，但它在内存碎片、查找效率、释放合并等方面存在不足。通过引入 **next fit、**best fit**、**buddy system**、**更高效的数据结构**、**启发式策略** 和 **碎片整理技术**，可以在一定程度上解决这些问题，提升内存的利用率和分配效率。不同的改进方案有不同的应用场景和优缺点，具体的选择取决于应用程序的内存分配模式和系统的设计目标。

#### 练习2：实现 Best-Fit 连续物理内存分配算法（需要编程）
在完成练习一后，参考kern/mm/default_pmm.c对First Fit算法的实现，编程实现Best Fit页面分配算法，算法的时空复杂度不做要求，能通过测试即可。
请在实验报告中简要说明你的设计实现过程，阐述代码是如何对物理内存进行分配和释放。

答：best fit算法即是在first fit算法的基础上，于空闲块链表中寻找满足所需内存大小的最小内存块，其余步骤与first fit算法大致相同，下面我们将展示所实现的best fit算法

首先是对best_fit_init_memmap函数的填补，由于best fit算法只是在链表遍历并找出空闲块时方法与first fit不同，所以这里代码同first fit相同
```C
static void
best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));

        /*LAB2 EXERCISE 2: 2212602*/ 
        // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            /*LAB2 EXERCISE 2: 2212602*/ 
            // 编写代码
            // 1、当base < page时，找到第一个大于base的页，将base插入到它前面，并退出循环
            // 2、当list_next(le) == &free_list时，若已经到达链表结尾，将base插入到链表尾部
            if(base < page){
                list_add_before(le, &(base->page_link));
                break;
            }
            if(list_next(le) == &free_list){
                list_add(le, &(base->page_link));
            }
        
        }
    }
}
```

然后是对函数best_fit_free_pages的填补，这里仍然与first fit相同，因为两算法在释放内存块时没有差别，只在寻找内存块时有差异
```C
static void
best_fit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    /*LAB2 EXERCISE 2: 2212602*/ 
    // 编写代码
    // 具体来说就是设置当前页块的属性为释放的页块数、并将当前页块标记为已分配状态、最后增加nr_free的值

    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        /*LAB2 EXERCISE 2: 2212602*/ 
         // 编写代码
        // 1、判断前面的空闲页块是否与当前页块是连续的，如果是连续的，则将当前页块合并到前面的空闲页块中
        // 2、首先更新前一个空闲页块的大小，加上当前页块的大小
        // 3、清除当前页块的属性标记，表示不再是空闲页块
        // 4、从链表中删除当前页块
        // 5、将指针指向前一个空闲页块，以便继续检查合并后的连续空闲页块
        if (p + p->property == base)
        {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```

主要进行填补、修改的函数是best_fit_alloc_pages，此处不再中途退出while循环，而是使用min_size记录当前遍历过的所有内存块中，满足条件，且减去所需内存块大小最小的差值。
```C
static struct Page *
best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    size_t min_size = nr_free + 1;
    /*LAB2 EXERCISE 2: 2212602*/ 
    // 下面的代码是first-fit的部分代码，请修改下面的代码改为best-fit
    // 遍历空闲链表，查找满足需求的空闲页框
    // 如果找到满足需求的页面，记录该页面以及当前找到的最小连续空闲页框数量
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n && p->property - n < min_size)
        {
            page = p;
            min_size = p->property - n;
        }
    }

    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```
以上便是我们实现best fit算法的展示
#### 练习2.5：你的 Best-Fit 算法是否有进一步的改进空间？

答：
##### 1. 线性搜索导致的时间复杂度问题
best fit需要线性遍历空闲链表来查找合适的内存块，链表越长，搜索时间越长。当系统中空闲块的数量增加时，这种线性查找的效率就会下降。

改进方法:
- **分区空闲链表**：将空闲链表根据块的大小划分为多个链表，按照大小分类，如小块链表、中块链表、大块链表。这样可以在适当的链表中快速查找合适的内存块，避免线性遍历整个链表。
- **使用更高效的数据结构**：例如使用平衡树,如AVL树或红黑树,来管理空闲块，这样能够快速查找和插入合适的块，查找效率由线性降低到对数级别。
- **快速查找算法**：比如在空闲链表中维护一个指向"最佳起始位置"的指针，可以利用类似指针回溯的方法，快速找到最近一个合适大小的块，减少遍历。

##### 2. 释放时合并空闲块的复杂度
在best fit算法中，释放内存时需要对空闲链表进行插入和合并操作，这些操作可能会导致性能下降，特别是当内存碎片较多且链表较长时。

改进方法:
- **双链表策略**：可以将空闲块分为"刚分配完尚未合并"的链表和"已经合并好"的链表。新释放的块先加入到未合并的链表中，定期对未合并链表进行批量合并，减少在释放时的合并操作。
- **延迟合并**：不在每次释放时立即合并相邻的块，而是在达到一定阈值后或者在内存分配时再进行合并，这样可以减少释放操作的开销。

##### 3. 内存碎片的回收和整理
在长期运行的系统中，即使有碎片合并机制，也可能产生大量小碎片，特别是当存在频繁的内存分配和释放时。这些碎片最终可能导致内存不足的问题。

改进方法:
- **周期性碎片整理**：操作系统可以在空闲时或内存压力较大时主动整理碎片，将小碎片合并为大块空闲块，类似于磁盘的碎片整理。
- **内存压缩技术**：使用内存压缩技术将分散的空闲块压缩成连续的块，从而提高内存的可用性。

##### 4.使用适应性分配算法
可以将best fit与其他算法（如first fit或next fit）结合使用，以动态调整分配策略。例如，当内存碎片程度较高时，可以暂时切换到first-fit策略以加快分配速度，并降低寻找合适块的开销。

#### 扩展练习Challenge：buddy system（伙伴系统）分配算法（需要编程）

Buddy System算法把系统中的可用存储空间划分为存储块(Block)来进行管理, 每个存储块的大小必须是2的n次幂(Pow(2, n)), 即1, 2, 4, 8, 16, 32, 64, 128...

 -  参考[伙伴分配器的一个极简实现](http://coolshell.cn/articles/10427.html)， 在ucore中实现buddy system分配算法，要求有比较充分的测试用例说明实现的正确性，需要有设计文档。
 
#### 扩展练习Challenge：任意大小的内存单元slub分配算法（需要编程）

slub算法，实现两层架构的高效内存单元分配，第一层是基于页大小的内存分配，第二层是在第一层基础上实现基于任意大小的内存分配。可简化实现，能够体现其主体思想即可。

 - 参考[linux的slub分配算法/](http://www.ibm.com/developerworks/cn/linux/l-cn-slub/)，在ucore中实现slub分配算法。要求有比较充分的测试用例说明实现的正确性，需要有设计文档。

#### 扩展练习Challenge：硬件的可用物理内存范围的获取方法（思考题）
  - 如果 OS 无法提前知道当前硬件的可用物理内存范围，请问你有何办法让 OS 获取可用物理内存范围？

答：OS可以通过下面所述多种方法来动态获取可用的物理内存信息，同时现代OS通常可以综合利用包含下述多种方法，以确保能够正确检测到系统中的所有物理内存。

###### 1. 通过 BIOS 或 UEFI 读取内存信息
现代计算机通常通过BIOS或UEFI提供内存映射表，这些表包含了系统中所有可用和保留的物理内存区域的信息。操作系统在启动过程中可以使用BIOS提供的中断调用（例如，INT 15h的E820 扩展内存检测）或通过UEFI的GetMemoryMap函数来获取这张内存映射表。这样可以准确地识别可用的物理内存范围。

###### 2. 读取 ACPI 表
高级配置和电源接口（ACPI）标准定义了一系列内存映射表，操作系统可以从这些表中获取可用物理内存区域。ACPI 中的内存描述符包含了所有可用内存的起始地址、长度以及类型。OS 可以根据这些信息来初始化其内存管理模块。

###### 3. 硬件探测
操作系统可以通过硬件自检（POST）获取物理内存的信息。这通常在系统的启动过程中进行，硬件会检测到物理内存的数量并将其传递给启动引导程序，最终操作系统可以读取这些信息来确定可用的物理内存范围。

###### 4. 启动引导程序传递信息
操作系统的引导加载程序（如 GRUB）通常在加载内核之前会对系统内存进行探测，并将内存信息以特定的数据结构（如 Multiboot 规范定义的数据结构）传递给内核。内核可以根据这些信息初始化物理内存管理。

###### 5. 直接通过固件或硬件寄存器访问
某些硬件平台可能提供特定的寄存器或专用的系统管理模式（SMM）来提供内存信息。操作系统可以通过读取这些寄存器来了解物理内存的配置。
