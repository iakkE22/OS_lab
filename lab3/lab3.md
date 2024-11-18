### 练习

对实验报告的要求：
 - 基于markdown格式来完成，以文本方式为主
 - 填写各个基本练习中要求完成的报告内容
 - 完成实验后，请分析ucore_lab中提供的参考答案，并请在实验报告中说明你的实现与参考答案的区别
 - 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
 - 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

#### 练习0：填写已有实验
>本实验依赖实验1/2。请把你做的实验1/2的代码填入本实验中代码中有“LAB1”,“LAB2”的注释相应部分。

#### 练习1：理解基于FIFO的页面替换算法（思考题）
>描述FIFO页面置换算法下，一个页面从被换入到被换出的过程中，会经过代码里哪些函数/宏的处理（或者说，需要调用哪些函数/宏），并用简单的一两句话描述每个函数在过程中做了什么？（为了方便同学们完成练习，所以实际上我们的项目代码和实验指导的还是略有不同，例如我们将FIFO页面置换算法头文件的大部分代码放在了`kern/mm/swap_fifo.c`文件中，这点请同学们注意）
 >- 至少正确指出10个不同的函数分别做了什么？如果少于10个将酌情给分。我们认为只要函数原型不同，就算两个不同的函数。要求指出对执行过程有实际影响,删去后会导致输出结果不同的函数（例如assert）而不是cprintf这样的函数。如果你选择的函数不能完整地体现”从换入到换出“的过程，比如10个函数都是页面换入的时候调用的，或者解释功能的时候只解释了这10个函数在页面换入时的功能，那么也会扣除一定的分数

答：

​	在遇到`Page Fault`时，最先是`kern/trap/trap.c`中的`pgfault_handler`函数进行处理，其中的`trapFrame`传递了`badvaddr`给`do_pgfault()`函数:`badvaddr`即`stval`寄存器中数值，存储访问出错的虚拟地址。

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
​	然后便是页面转换机制的核心，`kern/mm/vmm.c`中的`do_pgfault`函数，在接受`pgfault_handler`中传入的访问出错的虚拟地址等相关参数后，检查虚拟地址的合法性，并尝试为该地址分配物理页面或从交换区载入页面，确保进程可以继续访问该虚拟地址。
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
​	可以看到`do_pgfault`函数中，`struct vma_struct *vma = find_vma(mm, addr);`语句调用了同样位于`kern/mm/vmm.c`的`find_vma`函数，其接受传入的访问出错的虚拟地址参数，查找包含指定虚拟地址`addr`的虚拟内存区域`VMA`：如果找到合适的`vma_struct`结构体，则返回该区域的指针,否则返回 `NULL`,以确认地址是否合法。
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
​	然后在`do_pgfault`函数中，`ptep = get_pte(mm->pgdir, addr, 1); `处调用了位于`kern/mm/pmm.c`的`get_pte`函数，该函数从页目录`pgdir`中获取给定线性地址`addr`的页表项`PTE`：如果该页表项不存在，并且`create`参数为`true`，则会尝试分配新的页表页，并初始化相关的页目录和页表项，从而确保地址`addr`在页表中有对应的有效页表项。
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
​	接下来是`do_pgfault`函数中，`if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) `中对位于`kern/mm/pmm.c`的`pgdir_alloc_page`函数的调用，该函数在页目录`pgdir`中为给定的线性地址`addr`分配并插入一个新的物理页面，并根据参数设置页面的权限`perm`；同时如果系统支持交换机制，即`swap_init_ok`为真，该函数还会将页面标记为可交换，以支持将页面换出内存。
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
​	顺序梳理完`do_pgfault`函数中调用的所有函数后，我们对这些被调用的函数中进行层层梳理，并简要的对以下函数的作用进行阐述。
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
> get_pte()函数（位于`kern/mm/pmm.c`）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。
 - get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
 - 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

答：
之所以会有两段代码，是因为函数需要处理`sv39`的三级页表结构，需要逐级检查和分配页面，确保最终能够定位或创建用于虚拟地址到物理地址映射的页表项，这里的两段代码分别负责处理初始页表级别与第二页表级别。

让我们看看这两段相似的代码：    
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
这段代码是查找 `pde_t *pdep1`，也就是页目录项，并根据` PTE_V`判断该目录项是否有效。如果无效且 `create `为真，就分配一个新的物理页面并将其初始化为页表。
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
这段代码是用来查找页表项 页表项，并根据` PTE_V` 判断该页表项是否有效。如果无效且 create 为真，分配一个新的物理页面并将其初始化为页面条目。

此时观察这两段相似的代码，发现其只有开头的赋值语句`pde_t *pdep1 = &pgdir[PDX1(la)]`与`pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)]`不同，对于后续的处理是完全相同的，就是因为`sv39`的前两个页表项结构相同，函数只需要依次按照多级页表的映射关系，找到下一级的页目录或者页表项，或在其他情况下，进行重新分配，所以如此相似。

它们执行的是**查找并分配页表项**这一相同操作，只是它们操作的是不同层次的页表，一个是页目录一个是页表。究其根本，就是虚拟内存结构在不同级别页表的一致性，尤其是分配新页表项时，虽然细节上略有不同，但总体的结构、操作都是类似的。

`sv32`、`sv39` 和 `sv48`分别适用于不同位宽和地址空间需求，如下表所示：

| 特性         | SV32                   | SV39                     | SV48                     |
|--------------|-------------------------|---------------------------|---------------------------|
| **地址空间** | 4 GiB                  | 512 GiB                  | 256 TiB                  |
| **虚拟地址** | 32 位                  | 39 位                    | 48 位                    |
| **物理地址** | 支持最多 34 位         | 支持最多 56 位           | 支持最多 56 位           |
| **页表级别** | 2 级页表               | 3 级页表                 | 4 级页表                 |
| **页大小**   | 4 KiB, 2 MiB           | 4 KiB, 2 MiB, 1 GiB      | 4 KiB, 2 MiB, 1 GiB      |
| **页表入口** | 每级 1024 项（10 位索引） | 每级 512 项（9 位索引）  | 每级 512 项（9 位索引）  |
| **页表基址** | 20 位物理地址          | 27 位物理地址            | 36 位物理地址            |

由上表可知，sv32、sv39、sv48的共同点是所有这些地址映射结构都使用了多级页表，它们将虚拟地址分为多个段。无论是 sv32、sv39 还是 sv48，都会使用类似的方式来查找和分配每一层页表的条目。
​	
而它们的不同点就在于使用不同的页表级别。由于`sv32`只有两级页表，所以只会有一段代码和一个return；`sv39`有三级页表，就会有两段代码和一个return；而`sv48`有四级页表，此处就会有三段代码和一个return。但是，所有对于单级页表项进行处理的代码，不论是`sv32`，`sv39`还是`sv48`，因为虚拟地址位宽与每级索引位数的不同没有在代码形式上被体现出来，都应该是相似的。

​	下面我们来分析get_pte()函数将页表项的查找和页表项的分配合并在一个函数里的必要性与否。

​	在 `get_pte()` 函数中，将**页表项的查找和分配**逻辑合并与否各有优点，但我们认为将逻辑合并是非常合理的，这样的合并既简化了代码逻辑，提高了性能和内存利用率，同时也适应了不同页表模式下的需求。原因如下：

合并的优点：
1. **简化调用**：在多级页表结构中，如果将查找和分配逻辑分开，代码会变得非常复杂,但若是`get_pte()` 合并查找与分配后，操作系统只需一次调用就能完成查找和必要的分配，避免编写重复代码。

2. **减少开销**:合并查找和分配使 `get_pte()` 可以在一次函数调用中完成所有操作，避免分开查找和分配带来的函数调用开销，这对具有三级页表层级的`sv39` 非常重要。

3. **充分利用内存**:在`get_pte()`中，查找与分配的合并允许函数通过`create`参数控制是否需要在缺少页表项时分配新的页面，即允许按需分配页表，避免提前分配所有页表造成的内存浪费。
4. **支持不同页表模式**:若把眼光放的更远，由于 `RISC-V` 支持 `sv32`、`sv39` 和 `sv48` 等不同的页表模式，`get_pte()` 必须适配多级页表结构的查找和分配需求，那么查找与分配的合并可以保证每级的查找和分配逻辑一致，函数则可以通过递归处理实现多级页表的支持。
5. **更加可靠**:合并后的 `get_pte()` 可以将页表项分配的逻辑集中管理，确保每层级的分配和初始化方式一致,减少了分散分配代码带来的维护复杂性，也降低了错误发生的可能性。

而分开的优点如下：
1. **增强可读性与可维护性**：将“查找页表项”和“分配页表项”拆开，使得每个函数的职责更加明确。
2. **提高重用性**：在某些时候可能只需要用到查找与分配二者之一，分离这两个功能可以提高它们的重用性。

合并与分离都有合理的原因，但是在当前实现中将查找和分配操作合并在同一个函数中是更合理的，因为当前系统的页面管理策略并不特别复杂，合并后的提升更高。

#### 练习3：给未被映射的地址映射上物理页（需要编程）
> 补充完成do_pgfault（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
 >- 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。
 >- 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？
>- 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

`do_pgfault`页访问异常处理函数用于在发生页面缺失时，加载对应的页面到物理内存并修复页表映射。当某个虚拟地址访问失败时，`do_pgfault`首先判断是否存在包含该地址的虚拟内存区域。如果地址合法，根据需求分配物理页面并建立虚拟地址与物理地址的映射。如果页面被换出磁盘，则将其加载回内存，修复页表并确保程序正常运行。我们需要补充的代码主要涉及到`交换页面的加载`和`页面映射`，具体代码如下所示：



```c
int
do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    //try to find a vma which include addr
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++;
    //If the addr is in the range of a mm's vma?
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }

    /* IF (write an existed addr ) OR
     *    (write an non_existed addr && addr is writable) OR
     *    (read  an non_existed addr && addr is readable)
     * THEN
     *    continue process
     */
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= (PTE_R | PTE_W);
    }
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    pte_t *ptep=NULL;
    /*
    * Maybe you want help comment, BELOW comments can help you finish the code
    *
    * Some Useful MACROs and DEFINEs, you can use them in below implementation.
    * MACROs or Functions:
    *   get_pte : get an pte and return the kernel virtual address of this pte for la
    *             if the PT contians this pte didn't exist, alloc a page for PT (notice the 3th parameter '1')
    *   pgdir_alloc_page : call alloc_page & page_insert functions to allocate a page size memory & setup
    *             an addr map pa<--->la with linear address la and the PDT pgdir
    * DEFINES:
    *   VM_WRITE  : If vma->vm_flags & VM_WRITE == 1/0, then the vma is writable/non writable
    *   PTE_W           0x002                   // page table/directory entry flags bit : Writeable
    *   PTE_U           0x004                   // page table/directory entry flags bit : User can access
    * VARIABLES:
    *   mm->pgdir : the PDT of these vma
    *
    */


    ptep = get_pte(mm->pgdir, addr, 1);  //(1) try to find a pte, if pte's
                                         //PT(Page Table) isn't existed, then
                                         //create a PT.
    if (*ptep == 0) {
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    } else {
        /*LAB3 EXERCISE 3: 2210636
        * 请你根据以下信息提示，补充函数
        * 现在我们认为pte是一个交换条目，那我们应该从磁盘加载数据并放到带有phy addr的页面，
        * 并将phy addr与逻辑addr映射，触发交换管理器记录该页面的访问情况
        *
        *  一些有用的宏和定义，可能会对你接下来代码的编写产生帮助(显然是有帮助的)
        *  宏或函数:
        *    swap_in(mm, addr, &page) : 分配一个内存页，然后根据
        *    PTE中的swap条目的addr，找到磁盘页的地址，将磁盘页的内容读入这个内存页
        *    page_insert ： 建立一个Page的phy addr与线性addr la的映射
        *    swap_map_swappable ： 设置页面可交换
        */
        if (swap_init_ok) {
            struct Page *page = NULL;
            // 你要编写的内容在这里，请基于上文说明以及下文的英文注释完成代码编写
            //(1）According to the mm AND addr, try
            //to load the content of right disk page
            //into the memory which page managed.
            //(2) According to the mm,
            //addr AND page, setup the
            //map of phy addr <--->
            //logical addr
            //(3) make the page swappable.
            swap_in(mm, addr, &page);
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);
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
1. **查找或分配PTE**：使用`get_pte` 获取页表项,如果对应的页表不存在，则分配一个页表。如果PTE未映射到物理页面`（*ptep == 0）`，调用 `pgdir_alloc_page`分配一个物理页面并更新页表，建立地址映射;如果 PTE 存在，但页面在磁盘中，需要从磁盘加载页面。

2. **从磁盘加载页面**:当`swap_init_ok`为真时，调用`swap_in`将磁盘中的页面加载到内存，随后调用`page_insert`更新页表，建立虚拟地址和物理地址的映射，再调用 `swap_map_swappable`标记页面为可交换，最后更新页面的虚拟地址信息。

3. **错误处理**:如果任何步骤失败，返回对应的错误码
   
>描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。

在ucore的页替换算法中，PDE 和 PTE 的标志位和地址字段是关键工具，用于判断页面状态、触发页面加载或替换操作，并管理页面的访问和写回操作。

1. **有效性检查**
   `PDE`与`PTE`的有效位`PTE_V`决定了一个虚拟地址是否有效。如果有效位为 0，则表明该地址未映射到物理内存。在页替换算法中，操作系统可以通过检查有效位快速判断页面是否已经换出。如果有效位为 0，表示页面不在内存中，系统可能需要从磁盘或交换空间加载该页面。
   替换时首先扫描页面表，寻找有效位为 0 的页面。有效位为 0 的页面通常无需再被换出，可以直接分配给新页面使用；而如果一个有效位为 0 的页面需要被访问，则触发页面换入；而有效位为 1 的页面可能成为换出候选。

2. **访问权限**
    - PTE_W（写权限）：指示页面是否可写。
    - PTE_U（用户权限）：指示页面是否允许用户级进程访问。
  
    可写页面（PTE_W=1）可能更频繁地被修改，因此在压力较大时，操作系统可能优先选择这些页面进行置换。只读页面（PTE_W=0）通常对内存保护至关重要，替换优先级较低。而用户页面（PTE_U=1）可能需要更高的优先级进行替换，尤其在内存压力较大的用户空间应用中。
    另外，脏页标志指示页面是否在内存中被修改过。在页面换出时，系统需要检查脏页标志。如果页面被修改，则需要将其写回磁盘；如果页面未被修改，可以直接释放。通过脏页标志，操作系统能够减少不必要的写操作，从而提高性能。
    通过 PTE_W 和 PTE_U 确定页面的访问权限，辅助替换算法决定哪些页面可以优先被替换。

3. **访问历史**
   当页面被访问时，页表项中的引用位会被硬件设置为 1，以此实现页面访问历史的记录，支持 LRU 或 CLOCK 等替换算法。
   - LRU 算法可以根据引用位判断页面最近是否被访问过，未被访问的页面优先替换。
   - CLOCK 算法可以周期性地清除引用位，通过模拟时钟指针判断页面的替换顺序。

    如果页面既未被访问（引用位为 0）又未被修改（脏页标志为 0），则该页面是优先替换的理想候选。
   
4. **辅助决策**
   通过页目录项和页表项提供的信息，操作系统能够更有效地管理内存。
   - VM_WRITE 标志判断虚拟内存区域是否允许写入，决定页面的可换出性。写入频繁的页面可能优先被换出以减轻内存压力。
   - VM_EXEC 标志判断页面是否包含可执行代码。操作系统通常避免替换包含代码的页面，以保证执行效率。
    
    页置换算法结合虚拟内存区域的标志和页表权限，操作系统能够精确地选择适合的页面进行置换，从而提升系统的稳定性与效率。

>如果 ucore 的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？
1. **触发页访问异常**
当 ucore 的缺页服务例程尝试访问无效的内存地址或受权限限制的内存时，硬件会识别这种情况并触发一个页访问异常。

2. **保存异常上下文**
硬件会自动保存异常发生时的必要上下文，包括以下内容：

- 异常地址：
  - 保存导致异常的虚拟地址，该地址存储在 `stval` 或 `mtval` 寄存器中。

- 错误码：
  - 硬件会将页访问异常的原因编码为错误码，错误码内容包括：
    - P 标志：页面不存在（P=0）或访问权限错误（P=1）。
    - W/R 标志：访问是写操作（W=1）还是读操作（R=0）。
    - U/S 标志：异常发生时的特权级别，用户态（U=1）或内核态（S=0）。
    - 保留位错误：访问非法保留位时产生的异常。

- 当前指令地址：
  - 保存触发异常的指令地址，用于恢复程序执行。

3. **转移控制到异常处理程序**
- 硬件根据异常向量表将控制权转移到内核定义的页访问异常处理程序 `do_pgfault`。
- 在转移控制权时，硬件会：
  - 切换到内核态（特权级别 Ring 0）。
  - 保存当前的程序状态，以便在异常处理后恢复执行。

4. **更新特权级别与堆栈指针**
  - 如果异常发生时处于用户态，硬件会将特权级别从用户态（Ring 3）切换到内核态（Ring 0）。
- 加载内核栈指针
  - 硬件从任务状态段或特定寄存器中加载内核栈指针，确保异常处理在受控环境中执行。


5. **锁定和中断处理**
- 屏蔽中断
  - 防止其他中断或异常干扰当前的页访问异常处理。
- 内存锁定
  - 确保异常处理期间的内存访问是一致的，避免状态污染。

6. **提供诊断信息**
硬件提供的信息用于协助操作系统处理异常:
- 虚拟地址
   - 导致异常的地址（ `stval`）。
- 错误码
   - 描述异常的原因（页面不存在、权限错误等）。
- 触发异常的指令地址
   - 保存当前指令地址，便于异常处理结束后恢复程序执行。

7. **硬件与操作系统的协同**
在硬件完成上述操作后，操作系统的缺页服务例程 `do_pgfault`会接管控制权,首先检查异常原因，然后修复页面映射，最后恢复页面状态并重新执行触发异常的指令。

>数据结构 Page 的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

Page 是用于管理物理内存页面的数据结构。每个 Page 对象代表物理内存中的一页，Page 数组则是一个全局变量，表示整个物理内存中的所有页面。而页目录项PDE和页表项PTE则是虚拟内存到物理内存的映射的关键结构，它们用于管理虚拟地址与物理内存之间的映射关系。

- PTE 和 Page 数组是直接对应关系：每个有效的 PTE 存储物理页框号PFN，PFN 对应 Page 数组中的一个条目，表示一个物理页面。
- PDE 与 Page 数组是间接关系：PDE 指向页表，而页表项PTE则指向具体的物理页面。PDE 通过页表间接指向物理页面，而 PTE 通过 PFN 直接指向 Page 数组中的物理页面。
   

#### 练习4：补充完成Clock页替换算法（需要编程）
通过之前的练习，相信大家对FIFO的页面替换算法有了更深入的了解，现在请在我们给出的框架上，填写代码，实现 Clock页替换算法（mm/swap_clock.c）。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
 - 比较Clock页替换算法和FIFO算法的不同。
  
我们逐步完成以下几个函数：
1. **_clock_init_mm**
   ```c
   static int
    _clock_init_mm(struct mm_struct *mm)
    {
        /*LAB3 EXERCISE 4: 2210636*/
        // 初始化pra_list_head为空链表
        // 初始化当前指针curr_ptr指向pra_list_head，表示当前页面替换位置为链表头
        // 将mm的私有成员指针指向pra_list_head，用于后续的页面替换算法操作
        // cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
        list_init(&pra_list_head);
        curr_ptr = &pra_list_head;
        mm->sm_priv = &pra_list_head;

        return 0;
    }
   ```

`_clock_init_mm` 是 CLOCK 页面置换算法的初始化函数，主要用于初始化页面链表和相关指针，确保` mm_struct `的私有数据与页面替换链表关联起来。

   首先，调用 `list_init(&pra_list_head) `初始化页面链表头部 `pra_list_head`。该链表将用于存储所有可替换的页面，按照 CLOCK 算法的访问顺序进行管理。

   然后将当前指针 `curr_ptr` 设置为 `pra_list_head`，初始位置指向链表头部。`curr_ptr` 的作用是记录当前需要检查的页面，用于后续页面替换操作。

   最后将 `mm_struct `结构体的私有成员` sm_priv `设置为` pra_list_head `的地址，将`mm->sm_priv`与页面链表关联。通过这种关联关系，后续页面替换操作可以通过 `mm->sm_priv `访问页面链表，统一管理页面数据。

2. **_clock_map_swappable**
```c
    static int
    _clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
    {
        list_entry_t *head=(list_entry_t*) mm->sm_priv;
        list_entry_t *entry=&(page->pra_page_link);

        assert(entry != NULL && curr_ptr != NULL);
        // record the page access situlation
        /*LAB3 EXERCISE 4: 2213788*/
        // link the most recent arrival page at the back of the pra_list_head qeueue.
        // 将页面page插入到页面链表pra_list_head的末尾
        // 将页面的visited标志置为1，表示该页面已被访问
        list_add(head, entry);
        page->visited = 1;

        return 0;
    }
```
`_clock_map_swappable` 函数的作用是将一个页面标记为可替换，并将其加入页面链表`pra_list_head`的末尾，同时设置页面的访问标记 `visited`。

首先通过` mm->sm_priv `获取页面链表的头部指针 `head`。
通过` page->pra_page_link `获取页面对应的链表节点 `entry`，用于插入操作。

然后确保页面链表节点` entry `和当前指针` curr_ptr `都已正确初始化。如果指针为 NULL，说明链表结构未初始化，程序会停止运行。

随后使用` list_add `函数，将页面对应的链表节点插入到` pra_list_head `的末尾。
页面加入链表后，表示其已经进入可替换的页面队列。

最后设置页面的访问标记` visited `为 1。该标记用于记录页面是否在最近被访问过，为 CLOCK 算法的换出逻辑提供依据。

3. **_clock_swap_out_victim**

```c
static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
     assert(in_tick==0);
     /* Select the victim */
     //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
     //(2)  set the addr of addr of this page to ptr_page
    while (1) {
        /*LAB3 EXERCISE 4: 2213788*/ 
        // 编写代码
        // 遍历页面链表pra_list_head，查找最早未被访问的页面
        // 获取当前页面对应的Page结构指针
        // 如果当前页面未被访问，则将该页面从页面链表中删除，并将该页面指针赋值给ptr_page作为换出页面
        // 如果当前页面已被访问，则将visited标志置为0，表示该页面已被重新访问
        if (curr_ptr == head) curr_ptr = head->prev;
        list_entry_t *prev = curr_ptr->prev;
        struct Page *page = le2page(curr_ptr, pra_page_link);
        if (page->visited == 0) {
            list_del(curr_ptr);
            *ptr_page = page;
            break;
        }
        page->visited = 0;
        curr_ptr = prev;
    }
    return 0;
}
```

`_clock_swap_out_victim` 函数是 CLOCK 页面置换算法 的核心部分，用于选择需要换出的页面（victim page）。它通过遍历页面链表，优先选择最近未被访问的页面进行换出，并更新链表结构和页面状态。

在进行实验要求的断言后，先从当前指针 `curr_ptr `开始遍历链表，如果当前指针指向链表头部，循环回到链表尾部`（head->prev）`，再使用 `le2page` 宏将链表节点 `curr_ptr `转换为对应的` Page `结构。

然后检查页面的 `visited` 标记
- 如果 `visited == 0`，表示页面未被访问，是需要换出的页面。将该页面从链表中删除，并将其指针赋值给 `*ptr_page`，作为换出的页面,随后退出循环，完成页面选择。


- 如果页面的 `visited == 1`，表示页面最近被访问过，将 `visited` 标记重置为 0，表示页面已被重新检查，随后将当前指针 `curr_ptr` 移动到前一个页面，继续检查链表中的下一个页面。

>比较Clock页替换算法和FIFO算法的不同

- FIFO算法是最简单的页面置换算法，按照页面进入内存的时间顺序替换最早进入的页面。我们使用一个队列，队列头存储最早进入的页面，当需要替换时直接移除队列头部的页面。
  - **优点**：
    - 实现简单，时间复杂度低（O(1)）。
    - 不需要额外的标记或判断，直接替换队列头部页面。
  - **缺点**：
    - 不考虑页面访问历史，容易导致频繁访问的页面被替换。
    - 可能发生 Belady 异常，增加页面帧数反而降低性能。


- Clock页替换算法是对 FIFO 的改进，结合了页面访问历史信息。使用一个循环队列和一个访问标记`visited` ，通过判断页面是否最近被访问，优先替换未被访问的页面。
  - **优点**：
    - 使用访问标记（`visited` 位）记录页面的使用情况，更智能地选择替换页面。
    - 避免频繁访问的页面被替换，减少不必要的页面换入和换出操作。
    - 减少 Belady 异常的可能性，提高系统性能。
  - **缺点**：
    - 实现复杂，需要维护循环链表和 `visited` 标记。
    - 替换时需要遍历链表检查 `visited` 位，时间复杂度较高（O(n)）。


二者核心差异如下表所示：
| 特性                 | FIFO 算法                                      | CLOCK 算法                                    |
|----------------------|-----------------------------------------------|---------------------------------------------|
| **页面选择策略**     | 总是替换队列中最早进入的页面。                 | 根据 `visited` 位选择最近未被访问的页面。    |
| **页面历史的利用**   | 不考虑页面是否被访问过。                       | 使用 `visited` 位记录页面是否被访问。        |
| **数据结构**         | 普通队列（先进先出）。                         | 循环链表（结合访问标记位）。                 |
| **性能改进**         | 无法避免不必要的换出，例如频繁访问的页面可能被替换。| 减少频繁访问页面被替换的可能，提高性能。     |
| **复杂度**           | 时间复杂度为 O(1)，操作简单，直接替换队列头部。                  | 时间复杂度为 O(n)，需遍历链表判断 `visited` 位，需要维护访问标记和指针。 |
| **Belady 异常**      | 可能发生 Belady 异常，即增加页面帧反而降低性能。 | 避免 Belady 异常，通过访问历史优化替换决策。|


---

#### 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）
如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

答：

​	采用“一页表大页映射”方式（也称为单级页表映射），即将整个虚拟地址空间映射到一个大的页表中，不使用多级页表结构，这种映射方式有利有弊，需要辩证看待，我们分析了其优缺点，展示如下。

**好处与优势**

1. **简单快速**

单级页表结构简单，查找过程直接通过虚拟地址的索引定位到页表项，无需逐级查找；同时查找开销较小，相比多级页表可以减少内存访问次数，提高查找速度。

2. **更适配TLB**

单级页表可以直接映射所有虚拟地址，较少引入地址分级问题，不会因为逐级解析而导致 TLB 缓存失效，减少频繁页表查找，尤其在小内存应用场景中，页表完全可以加载到 TLB 中，提升访问速度。

3. **减少内存碎片**

通过一个大页表映射，可以映射一个较大块的连续内存空间，减少多级页表中可能出现的碎片问题，所以“一页表”特别适用于固定内存布局的应用场景，避免多级结构带来的碎片。

**坏处与风险**
1. **内存开销大**

对于大型系统来说，单级页表需要足够大的空间来覆盖完整的虚拟地址空间，内存开销极大：比如对于 64 位地址空间，如果采用单级页表，内存开销将指数级增大，而大多数物理内存可能是稀疏分布的，导致大量页表空间被浪费。

2. **缺乏灵活性**

单级页表结构不易进行动态调整和分配，不适合复杂系统的需求，而相比于多级页表的按需创建和分配子页表，大页表映射则显得缺乏这种灵活性，不利于动态扩展和内存保护。

3. **内存资源浪费**

单级页表无法按需分配，只能一次性分配完整的映射表，会导致内存碎片化的风险增加，尤其是仅一小部分虚拟地址需要被使用时，单级页表会导致大量页表项和内存资源浪费。

4. **安全性问题**

单级页表结构将整个虚拟地址空间直接映射到物理地址空间，缺乏分层级的内存访问权限管理，那么相比于多级页表允许对不同层级设置不同的权限和隔离策略，单级结构则会导致地址空间布局过于集中，安全性降低。

为了更好的展示“一页表大页映射”方式与多级页表分配方式的对比，我们做出表格如下：
| 特性               | 单级页表（一个大页映射）               | 多级页表                        |
|--------------------|----------------------------------------|---------------------------------|
| **内存开销**       | 大（对于大地址空间极不经济）           | 按需分配，内存利用率更高        |
| **查找效率**       | 快速，一次性查找                      | 慢，多级查找                    |
| **实现复杂度**     | 简单                                  | 较复杂                          |
| **灵活性**         | 低，不支持按需分配和动态扩展          | 高，可按需分配和逐级扩展        |
| **内存资源浪费**   | 高                                    | 低                              |
| **安全性**   | 较差，难以实现多层安全控制            | 高，支持多层安全权限和隔离      |

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

