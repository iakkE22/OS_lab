### 练习

对实验报告的要求：
 - 基于markdown格式来完成，以文本方式为主
 - 填写各个基本练习中要求完成的报告内容
 - 完成实验后，请分析ucore_lab中提供的参考答案，并请在实验报告中说明你的实现与参考答案的区别
 - 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
 - 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

#### 练习0：填写已有实验
本实验依赖实验1/2。请把你做的实验1/2的代码填入本实验中代码中有“LAB1”,“LAB2”的注释相应部分。

#### 练习1：理解基于FIFO的页面替换算法（思考题）
描述FIFO页面置换算法下，一个页面从被换入到被换出的过程中，会经过代码里哪些函数/宏的处理（或者说，需要调用哪些函数/宏），并用简单的一两句话描述每个函数在过程中做了什么？（为了方便同学们完成练习，所以实际上我们的项目代码和实验指导的还是略有不同，例如我们将FIFO页面置换算法头文件的大部分代码放在了`kern/mm/swap_fifo.c`文件中，这点请同学们注意）
 - 至少正确指出10个不同的函数分别做了什么？如果少于10个将酌情给分。我们认为只要函数原型不同，就算两个不同的函数。要求指出对执行过程有实际影响,删去后会导致输出结果不同的函数（例如assert）而不是cprintf这样的函数。如果你选择的函数不能完整地体现”从换入到换出“的过程，比如10个函数都是页面换入的时候调用的，或者解释功能的时候只解释了这10个函数在页面换入时的功能，那么也会扣除一定的分数

答：

​    在遇到`Page Fault`时，最先是`kern/trap/trap.c`中的`pgfault_handler`函数进行处理，其中的`trapFrame`传递了`badvaddr`给`do_pgfault()`函数:`badvaddr`即`stval`寄存器中数值，存储访问出错的虚拟地址。

```c
static int pgfault_handler(struct trapframe *tf) {
    extern struct mm_struct *check_mm_struct;
    print_pgfault(tf);
    if (check_mm_struct != NULL) {
        return do_pgfault(check_mm_struct, tf->cause, tf->badvaddr);
    }
    panic("unhandled page fault.\n");
}
```
​    然后便是页面转换机制的核心，`kern/mm/vmm.c`中的`do_pgfault`函数，在接受`pgfault_handler`中传入的访问出错的虚拟地址等相关参数后，检查虚拟地址的合法性，并尝试为该地址分配物理页面或从交换区载入页面，确保进程可以继续访问该虚拟地址。
```c
int
do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    struct vma_struct *vma = find_vma(mm, addr);
    pgfault_num++;
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= (PTE_R | PTE_W);
    }
    addr = ROUNDDOWN(addr, PGSIZE);
    ret = -E_NO_MEM;
    pte_t *ptep=NULL;
    ptep = get_pte(mm->pgdir, addr, 1);  
    if (*ptep == 0) {
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    } else {
        /*LAB3 EXERCISE 3: YOUR CODE*/
        if (swap_init_ok) {
            struct Page *page = NULL;
            // 你要编写的内容在这里，请基于上文说明以及下文的英文注释完成代码编写
            page->pra_vaddr = addr;
        } else {
            cprintf("no swap_init_ok but ptep is %x, failed\n", *ptep);
            goto failed;
        }
   }
   ret = 0;
failed:
    return ret;
}
```
​    可以看到`do_pgfault`函数中，`struct vma_struct *vma = find_vma(mm, addr);`语句调用了同样位于`kern/mm/vmm.c`的`find_vma`函数，其接受传入的访问出错的虚拟地址参数，查找包含指定虚拟地址`addr`的虚拟内存区域`VMA`：如果找到合适的`vma_struct`结构体，则返回该区域的指针,否则返回 `NULL`,以确认地址是否合法。
```c
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr) {
    struct vma_struct *vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
                bool found = 0;
                list_entry_t *list = &(mm->mmap_list), *le = list;
                while ((le = list_next(le)) != list) {
                    vma = le2vma(le, list_link);
                    if (vma->vm_start<=addr && addr < vma->vm_end) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    vma = NULL;
                }
        }
        if (vma != NULL) {
            mm->mmap_cache = vma;
        }
    }
    return vma;
}
```
​	   然后在`do_pgfault`函数中，`ptep = get_pte(mm->pgdir, addr, 1); `处调用了位于`kern/mm/pmm.c`的`get_pte`函数，该函数从页目录`pgdir`中获取给定线性地址`addr`的页表项`PTE`：如果该页表项不存在，并且`create`参数为`true`，则会尝试分配新的页表页，并初始化相关的页目录和页表项，从而确保地址`addr`在页表中有对应的有效页表项。
```c
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create) {
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V)) {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL) {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
//    pde_t *pdep0 = &((pde_t *)(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V)) {
    	struct Page *page;
    	if (!create || (page = alloc_page()) == NULL) {
    		return NULL;
    	}
    	set_page_ref(page, 1);
    	uintptr_t pa = page2pa(page);
    	memset(KADDR(pa), 0, PGSIZE);
 //   	memset(pa, 0, PGSIZE);
    	*pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}
```
​	   接下来是`do_pgfault`函数中，`if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) `中对位于`kern/mm/pmm.c`的`pgdir_alloc_page`函数的调用，该函数在页目录`pgdir`中为给定的线性地址`addr`分配并插入一个新的物理页面，并根据参数设置页面的权限`perm`；同时如果系统支持交换机制，即`swap_init_ok`为真，该函数还会将页面标记为可交换，以支持将页面换出内存。
```c
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm) {
    struct Page *page = alloc_page();
    if (page != NULL) {
        if (page_insert(pgdir, page, la, perm) != 0) {
            free_page(page);
            return NULL;
        }
        if (swap_init_ok) {
            swap_map_swappable(check_mm_struct, la, page, 0);
            page->pra_vaddr = la;
            assert(page_ref(page) == 1);
            // cprintf("get No. %d  page: pra_vaddr %x, pra_link.prev %x,
            // pra_link_next %x in pgdir_alloc_page\n", (page-pages),
            // page->pra_vaddr,page->pra_page_link.prev,
            // page->pra_page_link.next);
        }
    }

    return page;
}
```
​	   顺序梳理完`do_pgfault`函数中调用的所有函数后，我们对这些被调用的函数中进行层层梳理，并简要的对以下函数的作用进行阐述。
​	**首先是`find_vma`函数中调用的`list_next`函数与`le2vma`函数**

- `list_next`函数对链表进行操作，用于获取链表中下一个节点的指针;
- `le2vma`函数的作用是将链表节点le转换为包含`list_link`等链接字段的`struct vma_struct`类型的指针，从而方便访问`vma_struct`中的`vm_start`、`vm_end`等字段。

​	**然后是`get_pte`函数中调用的如下函数：`alloc_page`，`set_page_ref`，`page2pa`，`pte_create`。**

- `alloc_page`函数从物理内存的空闲页面池中分配一个页面，并返回指向该页面的`Page`结构体指针；
- `set_page_ref`函数设置页面的引用计数，以跟踪页面被引用的次数，以便在没有进程引用该页面时将其释放；
- `page2pa`函数通过计算将页面的`Page`结构体转换为页面的物理地址pa，以便后续对该物理地址的操作；
- `pte_create`函数可以创建一个用于定义虚拟地址到物理地址的映射关系以及访问权限的页表项`PTE`，其中包含页面的物理页帧号`ppn`和权限标志`perm`。

​	**最后是`pgdir_alloc_page`函数中调用的如下函数：`page_insert`，`free_page`，`swap_map_swappable`，`assert`，`page_ref`。**

- `page_insert`函数能够将物理页面插入到页表，使给定的虚拟地址`la`映射到该页面，并设置页面的权限 `perm`,如果虚拟地址`la`之前已经有映射的页面，`page_insert` 会将旧页面替换为新的页面并更新映射关系，同时维护其引用计数；
- `free_page`函数则可以释放一个不再被引用物理页面，将其归还到空闲页面池中，以供其他进程或任务使用；
- `swap_map_swappable`函数可以将页面标记为可交换，将页面加入到`FIFO`的交换页队列中，以便操作系统可以进行有效管理，同时在内存不足时可以将该页面换出到磁盘或其他二级存储中；
- `assert`函数则是一个断言检查函数，与操作系统页面管理的关联性不大，主要用于检查给定的条件是否为真，在条件不为真触发错误；
- `page_ref`函数可以获取页面的引用计数，与`set_page_ref`函数配合使用，确保页面在引用计数变为0时才被释放。

#### 练习2：深入理解不同分页模式的工作原理（思考题）
get_pte()函数（位于`kern/mm/pmm.c`）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。
 - get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
 - 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

答：
    首先我们要了解`sv32`，`sv39`，`sv48`在不同方面的异同，才能够对`get_pte()`函数中两段代码形式类似的现象做出解释。 
    
    `sv32`、`sv39` 和 `sv48` 是 RISC-V 架构中的三种页表模式，分别适用于不同位宽和地址空间需求。以下从不同方面介绍它们的异同。

1. **地址空间与位宽**
- **sv32**：支持 32 位虚拟地址，适用于 32 位系统，虚拟地址空间为 4 GiB。
- **sv39**：支持 39 位虚拟地址，适用于 64 位系统，虚拟地址空间为 512 GiB。
- **sv48**：支持 48 位虚拟地址，适用于 64 位系统，虚拟地址空间为 256 TiB。

**总结**：32、39、48代表虚拟地址位宽，位宽越大，虚拟地址空间也随之越大。

2. **页表层数**
- **sv32**：使用 2 级页表结构，需要 2 级页表查找。
- **sv39**：使用 3 级页表结构，需要 3 级页表查找。
- **sv48**：使用 4 级页表结构，需要 4 级页表查找。

**总结**：页表层数随着虚拟地址位宽的增加而增加，以支持更大的虚拟地址空间。

3. **页表项的数量与页表项大小**
- **页表项大小**：在所有三种模式中，每个页表项（PTE）的大小都是 **8 字节（64 位）**。
- **每级页表的页表项数量**：
  - **sv32**：每级页表包含 1024 个页表项（10 位用于索引）。
  - **sv39**：每级页表包含 512 个页表项（9 位用于索引）。
  - **sv48**：每级页表包含 512 个页表项（9 位用于索引）。

**总结**：页表项大小一致，但每级页表项数量不同，`sv32` 有更多页表项，而 `sv39` 和 `sv48` 的页表项数量相同。

4. **页大小**
- 三种模式下的默认页面大小均为 **4 KiB**。
- RISC-V 还允许支持更大的页面（如 2 MiB 或 1 GiB 等），通常用于减少内存碎片和页表开销。

**总结**：页大小一致，但在具体实现中可能支持更大页面的映射。

5. **虚拟地址解析过程**
- **sv32**：虚拟地址为 32 位，其中高 20 位用于页表索引（10 位用于每一级页表），低 12 位用于页面偏移。
- **sv39**：虚拟地址为 39 位，其中高 27 位用于页表索引（9 位用于每一级页表），低 12 位用于页面偏移。
- **sv48**：虚拟地址为 48 位，其中高 36 位用于页表索引（9 位用于每一级页表），低 12 位用于页面偏移。

**总结**：解析虚拟地址时，每级页表索引位数不同。`sv32` 每级页表索引 10 位，而 `sv39` 和 `sv48` 每级索引 9 位。

6. **总结表**

| 特性            | sv32             | sv39             | sv48              |
|-----------------|------------------|------------------|-------------------|
| 地址空间        | 4 GiB            | 512 GiB         | 256 TiB          |
| 页表层数        | 2                | 3               | 4                |
| 每级页表项数量   | 1024             | 512             | 512              |
| 页表项大小      | 8 字节           | 8 字节          | 8 字节           |
| 页大小          | 4 KiB            | 4 KiB           | 4 KiB            |
| 虚拟地址位宽    | 32 位            | 39 位           | 48 位            |
| 索引位数（每级）| 10 位            | 9 位            | 9 位             |

    此时便可以对`get_pte()`函数中两段代码形式类似的现象做出解释。

​	   首先会有两段代码，是因为函数需要处理`sv39`的三级页表结构，需要逐级检查和分配页面，确保最终能够定位或创建用于虚拟地址到物理地址映射的页表项，这里的两段代码分别负责处理初始页表级别与第二页表级别。

​	   此时观察下面两段相似的代码，发现其只有开头的赋值语句`pde_t *pdep1 = &pgdir[PDX1(la)]`与`pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)]`不同，对于后续的处理是完全相同的，就是因为`sv39`的前两个页表项结构相同，函数只需要依次按照多级页表的映射关系，找到下一级的页目录或者页表项，或在其他情况下，进行重新分配，所以如此相似。

```c
    pde_t *pdep1 = &pgdir[PDX1(la)];                              //不同
    if (!(*pdep1 & PTE_V)) {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL) {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
```
```c
    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)]; //不同
    if (!(*pdep0 & PTE_V)) {
    	struct Page *page;
    	if (!create || (page = alloc_page()) == NULL) {
    		return NULL;
    	}
    	set_page_ref(page, 1);
    	uintptr_t pa = page2pa(page);
    	memset(KADDR(pa), 0, PGSIZE);
    	*pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];          //处理最后一级页表
```
​	   而`RISCV`同样支持`sv32`与`sv48`的页面框架，由于`sv32`只有两级页表，所以此处只会有一段代码和一个return；而`sv48`有四级页表，此处就会有三段代码和一个return。但是，所有对于单级页表项进行处理的代码，不论是`sv32`，`sv39`还是`sv48`，因为虚拟地址位宽与每级索引位数的不同没有在代码形式上被体现出来，都应该是相似的。

​	   下面我们来分析get_pte()函数将页表项的查找和页表项的分配合并在一个函数里的必要性与否。

​	   我们认为在 `get_pte()` 函数中，将**页表项的查找和分配**逻辑合并在一起，这在 RISC-V 多级页表结构中显得尤为必要。这样的合并既简化了代码逻辑，提高了性能和内存利用率，同时也适应了不同页表模式下的需求。原因如下：

1. **简化调用流程，减少重复代码：**在多级页表结构中，如果将查找和分配逻辑分开，代码会变得非常复杂,但若是`get_pte()` 合并查找与分配后，操作系统只需一次调用就能完成查找和必要的分配，避免编写重复代码。

2. **提高性能，减少函数调用开销：**合并查找和分配使 `get_pte()` 可以在一次函数调用中完成所有操作，避免分开查找和分配带来的函数调用开销，这对具有三级页表层级的`sv39` 非常重要。

3. **支持按需分配内存，提高内存利用率：**在`get_pte()`中，查找与分配的合并允许函数通过`create`参数控制是否需要在缺少页表项时分配新的页面，即允许按需分配页表，避免提前分配所有页表造成的内存浪费。
4. **适配不同的页表模式和多级结构：**若把眼光放的更远，由于 `RISC-V` 支持 `sv32`、`sv39` 和 `sv48` 等不同的页表模式，`get_pte()` 必须适配多级页表结构的查找和分配需求，那么查找与分配的合并可以保证每级的查找和分配逻辑一致，函数则可以通过递归处理实现多级页表的支持。
5. **减少错误，提高可维护性：**合并后的 `get_pte()` 可以将页表项分配的逻辑集中管理，确保每层级的分配和初始化方式一致,减少了分散分配代码带来的维护复杂性，也降低了错误发生的可能性。


#### 练习3：给未被映射的地址映射上物理页（需要编程）
补充完成do_pgfault（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
 - 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。
 - 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？
- 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

#### 练习4：补充完成Clock页替换算法（需要编程）
通过之前的练习，相信大家对FIFO的页面替换算法有了更深入的了解，现在请在我们给出的框架上，填写代码，实现 Clock页替换算法（mm/swap_clock.c）。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
 - 比较Clock页替换算法和FIFO算法的不同。

#### 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）
如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

答：

​	   采用“一页表大页映射”方式（也称为单级页表映射），即将整个虚拟地址空间映射到一个大的页表中，不使用多级页表结构，这种方式在特定情况下确实有一些好处，但也伴随着一定的劣势和风险。

**好处与优势**

1. **简单实现和快速查找**

- 单级页表结构简单，查找过程直接通过虚拟地址的索引定位到页表项，无需逐级查找。
- 查找开销较小，相比多级页表可以减少内存访问次数，提高查找速度。
2. **避免 TLB 错误率高**

- 单级页表可以直接映射所有虚拟地址，较少引入地址分级问题，不会因为逐级解析而导致 TLB 缓存失效，减少频繁页表查找。
- 尤其在小内存应用场景中，页表完全可以加载到 TLB 中，提升访问速度。
3. **适合小型嵌入式系统**

- 在小型设备和嵌入式系统中，虚拟地址空间需求不大，单级页表可以映射较小的物理地址空间且无冗余，因此可以降低实现复杂度和内存消耗。
- 小型系统中，内存有限，单级页表结构更为直接，符合小型系统对性能和资源的需求。
4. **减少内存碎片**

- 通过一个大页表映射，可以映射一个较大块的连续内存空间，减少多级页表中可能出现的碎片问题。
- 特别适用于固定内存布局的应用场景，避免多级结构带来的碎片。

**坏处与风险**
1. **内存占用较大**

- 对于大型系统来说，单级页表需要足够大的空间来覆盖完整的虚拟地址空间，内存开销极大。
- 比如对于 64 位地址空间，如果采用单级页表，内存开销将指数级增大，而大多数物理内存可能是稀疏分布的，导致大量页表空间被浪费。
2. **缺乏内存管理灵活性**

- 单级页表结构不易进行动态调整和分配，不适合复杂系统的需求。
- 多级页表可以按需创建和分配子页表，大页表映射则缺乏这种灵活性，不利于动态扩展和内存保护。
3. **不支持大地址空间应用**

- 单级页表不适合64位大型系统的大地址空间需求，如果直接建立单级页表，页表会占用大量内存。
- 多级页表通过逐层递归的方式映射地址，能够支持更大地址空间；单级页表则会遇到地址空间增长带来的存储开销问题。
4. **高碎片化风险与不适合按需分配**

- 单级页表无法按需分配，只能一次性分配完整的映射表。大页表映射导致内存碎片化的风险增加。
- 当仅一小部分虚拟地址实际被使用时，单级页表会导致大量页表项和内存资源浪费。
5. **安全性和隔离性降低**

- 单级页表结构将整个虚拟地址空间直接映射到物理地址空间，缺乏分层级的内存访问权限管理。
- 多级页表允许对不同层级设置不同的权限和隔离策略，而单级结构则可能导致地址空间布局过于集中，安全性降低。

| 特性               | 单级页表（一个大页映射）               | 多级页表                        |
|--------------------|----------------------------------------|---------------------------------|
| **内存占用**       | 大（对于大地址空间极不经济）           | 按需分配，内存利用率更高        |
| **查找效率**       | 快速，一次性查找                      | 慢，多级查找                    |
| **实现复杂度**     | 简单                                  | 较复杂                          |
| **适合的应用场景** | 小地址空间或嵌入式系统                | 大地址空间或通用计算系统        |
| **灵活性**         | 低，不支持按需分配和动态扩展          | 高，可按需分配和逐级扩展        |
| **内存碎片风险**   | 高                                    | 低                              |
| **安全性和隔离**   | 较差，难以实现多层安全控制            | 高，支持多层安全权限和隔离      |
#### 扩展练习 Challenge：实现不考虑实现开销和效率的LRU页替换算法（需要编程）
challenge部分不是必做部分，不过在正确最后会酌情加分。需写出有详细的设计、分析和测试的实验报告。完成出色的可获得适当加分。

##### 设计

LRU的思想虽然与FIFO不同，但是其实现是类似的：都是通过维护一个链表结构来确定swap_out时选出的页，只是LRU需要不断更新整个链表结构

关于实现有三种方案：

- 借助tick事件，绑定swap_tick函数来定期更新lru链表（在一次tick下，至多只有一次内存写入），每次都检查是否被写入过，如果被写入，则将写入标记设为0，并将其放到队列尾部（从队列头部取出最久未使用页面）
- 通过设定页面权限，每次调用的map_swap函数时（任意的内存错误），将除了所需页面对应addr的页面全部设置为只读，所需页面设置为读写，以在之后的写入中触发同样的函数，这样就能够实现反复的更新；对于pgfault_num增大的问题，添加判断函数，如果是由于权限导致（通过页面是否存在即可），则pgfault_num--，认为不是错误
- 修改写入功能，写入必须调用access_memory，该函数在完成写入后自动调用`_lru_map_swappable`，实现自动更新状态

最终采用的实现为第三种

##### 实现的问题

- 方案1：在check_swap的过程中，整个系统是被阻塞的，不会自动计数，导致不可能绑定tick事件
- 方案2：通过权限异常触发的错误，会引发一次alloc_pages，而该函数会自动调用swap_out，造成链表头被弹出，可能弹出本来有用的页
- 方案3：这种方案最简单，但是开销也是巨大的，在每次写入时都要强制更新页面，更新页面的开销大于实际写入的开销

##### 测试

**测试代码**：

```c
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
```

结果推导：略过初始化步骤，链表中的顺序为1234（1是最久未被使用，使用y代替0xy000这样的地址），+代表出现一次pgfault，此前pgfault_num==4

| 写入 | 目前状态 |
| ---- | -------- |
| 3    | 1243     |
| 1    | 2431     |
| 4    | 2314     |
| 2    | 3142     |
| 5    | 1425+    |
| 2    | 1452     |
| 1    | 4521     |
| 2    | 4512     |
| 3    | 5123+    |
| 4    | 1234+    |
| 5    | 2345+    |
| 1    | 3451+    |

共计9次pg_fault，这与推导是相符的

**测试结果**：

```
...略
page fault at 0x00001000: K/R
swap_out: i 0, store page in vaddr 0x2000 to disk swap entry 3
swap_in: load disk swap entry 2 with swap_page in vadr 0x1000
count is 1, total is 8
check_swap() succeeded!
++ setup timer interrupts
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
```

