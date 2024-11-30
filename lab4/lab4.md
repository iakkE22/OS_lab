# 练习

对实验报告的要求：

- 基于markdown格式来完成，以文本方式为主
- 填写各个基本练习中要求完成的报告内容
- 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
- 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

# 练习0：填写已有实验

本实验依赖实验2/3。请把你做的实验2/3的代码填入本实验中代码中有“LAB2”,“LAB3”的注释相应部分。

# 练习1：分配并初始化一个进程控制块（需要编码）

*alloc\_proc 函数（位于 kern/process/proc.c 中）负责分配并返回一个新的 struct proc\_struct 结构，用于存储新建立的内核线程的管理信息。ucore 需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。 *

*【提示】在 alloc\_proc 函数的实现中，需要初始化的 proc\_struct 结构中的成员变量至少包括：state/pid/runs/kstack/need\_resched/parent/mm/context/tf/cr3/ffags/name。*

- 请在实验报告中简要说明你的设计实现过程。请回答如下问题：**
- *请说明 proc\_struct 中 struct context context 和 struct trapframe tf 成员变量含义和 在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）*

答：

`kern/process/proc.c`中,`alloc_proc`函数通过`f(proc!=NULL)`,对新建立的内核线程分配结构体`struct proc\_struct`，以表示表示这个新进程进程的状态和信息，结构体中需要被初始化的变量列举如下：

```c
enum proc_state state;                      // Process state
int pid;                                    // Process ID
int runs;                                   // the running times of Proces
uintptr_t kstack;                           // Process kernel stack
volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
struct proc_struct *parent;                 // the parent process
struct mm_struct *mm;                       // Process's memory management field
struct context context;                     // Switch here to run process
struct trapframe *tf;                       // Trap frame for current interrupt
uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
uint32_t flags;                             // Process flag
char name[PROC_NAME_LEN + 1];               // Process name
```

对这些变量的解释如下：

- `enum proc_state state`：用于表示进程当前的状态，而一般情况下进程有各种不同的状态，比如待运行、就绪）、阻塞、终止 等等，故需要一个枚举类型 `proc_state `，对这些状态值进行定义。

- `int pid`：是每个进程的唯一标识符 `PID `，内核便用此来管理进程。

- `int runs`：用于存放进程的运行次数，当进程被调度运行时，这个计数会增加。

- `uintptr_t kstack`：是进程的内核栈地址，其中内核栈用于存储内核模式下的函数调用和局部变量，在进程上下文切换时会需要用到该地址。

- `volatile bool need_resched`：一个用于表示进程是否需要重新调度的标志位，如果该值为真，则意味着当前进程需要被重新调度以释放 `CPU` 给其他进程。

- `struct proc_struct *parent`：指向当前进程的父进程，由于每个进程在创建时，都会有一个父进程，便需要指针 `parent` 指向父进程所在结构体，若当前进程是初始化进程（`PID`为`1`），则指针指向 `NULL`。

- `struct mm_struct *mm`：指向每个进程单独的内存管理结构，`mm_struct `在之前实验已经了解过，此处用于管理进程的虚拟内存空间，包括地址空间、页表、内存映射等信息。

- `struct context context`：进程的上下文切换信息，由于进程需要被调度，便使用 `context` 保存该进程的寄存器状态、程序计数器等信息，便于在进程被重新调度时快速恢复执行。

- `struct trapframe *tf`：进程的陷阱帧，在进行系统调用、异常处理或中断时，`trapframe`存储当前进程的状态，以便在进程返回用户空间或从中断处理程序恢复时快速恢复执行状态。

- `uintptr_t cr3`：进程的CR3寄存器值，指向进程的 `PDT`，辅助映射虚拟内存到物理内存。

- `uint32_t flags`：进程的标志位，用于存储进程的特定标记或状态信息，比如是否处于调度中、是否允许某些操作等。

- `char name[PROC_NAME_LEN + 1]`：进程的名称，用于标识进程的名字，`PROC_NAME_LEN` 是一个常量，表示名称的最大长度，`+1 `是为了包含字符串结束符` \0`。

现在我们对这些变量进行初始化。

```c
proc->state = PROC_UNINIT;
proc->pid = -1;
proc->runs = 0;
proc->kstack = 0;
proc->need_resched = 0;
proc->parent = NULL;
proc->mm = NULL;
memset(&(proc->context), 0, sizeof(struct context));
proc->tf = NULL;
proc->cr3 = boot_cr3;
proc->flags = 0;
memset(proc->name, 0, PROC_NAME_LEN + 1);
```

对初始化的解释如下：

- `proc->state = PROC_UNINIT;`：将 `state` 设置为 `PROC_UNINIT`（一个默认状态），表示进程处于未初始化状态。

- `proc->pid = -1;`：将进程的 `PID` 设置为 `-1`，这是一个默认的无效 `PID`，表示此进程等待分配有效的 `PID`。

- `proc->runs = 0;`：将 `runs` 设置为` 0`，表示进程运行次数为`0`，即还没有被运行过。

- `proc->kstack = 0;`：将进程的内核栈地址 kstack 设置为 `0`，等待内核栈地址的分配或初始化。

- `proc->need_resched = 0;`：将` need_resched `设置为` 0`，表示当前进程还不需要被重新调度。

- `proc->parent = NULL;`：将 `parent` 设置为` NULL`，还未设置进程的父进程。

- `proc->mm = NULL;`：将 `mm` 设置为` NULL`，表示进程尚未分配或初始化内存管理结构 `mm_struct`，即没有内存空间的映射。

- `memset(&(proc->context), 0, sizeof(struct context));`：使用 `memset` 将 `context` 结构体的所有字节清零，即进程还没有设置任何寄存器值或状态。

- `proc->tf = NULL;`：将` tf` 设置为 `NULL`，表示当前进程没有有效的陷阱帧（trapframe）。

- `proc->cr3 = boot_cr3;`：将` cr3` 设置为` boot_cr3`，即内核启动时的 `CR3` 寄存器值。

- `proc->flags = 0;`：将 `flags` 设置为` 0`，表示进程没有任何特殊的标志位。

- `memset(proc->name, 0, PROC_NAME_LEN + 1);`
使用 `memset` 将进程名称` name` 的所有字节清零，确保字符串为空，并准备名称长度`PROC_NAME_LEN + 1` 。

# 练习2

创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用do_fork函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要"fork"的东西就是stack和trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。

你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

答：

首先实现`kern/process/proc.c`中的`do_fork`函数，我们依据指导书所给出的步骤进行实现，在此依序展示：

- 调用`alloc_proc`，首先获得一块用户信息块。

```c
proc = alloc_proc();
if (proc == NULL)
{
    goto bad_fork_cleanup_kstack;
}
```

此处调用了`alloc_proc()`分配新内存，若分配内存失败，则进入`bad_fork_cleanup_kstack`

- 为进程分配一个内核栈。

```c
proc->pid = get_pid();
setup_kstack(proc);
```

调用`etup_kstack`为`proc`分配内存，其中调用`alloc_pages`函数分配页，并调用`page2kva(page)`分配内核栈。

```c
static int
setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}
```

- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）

```c
if (copy_mm(clone_flags, proc) != 0)
{
    goto bad_fork_cleanup_proc;
}
```

调用`copy_mm`函数，通过`clone_flags`进行判断，此处并没有做什么实质性的事情。

```c
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    assert(current->mm == NULL);
    /* do nothing in this project */
    return 0;
}
```

- 复制原进程上下文到新进程

```c
copy_thread(proc, stack, tf);
```

调用`copy_thread`函数，复制当前进程的 `trapframe`（即进程的状态信息）到新的进程中，并为新进程设置必要的上下文信息，同时使得新进程在调度时能够从正确的状态恢复执行。

```c
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;

    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
```

- 将新进程添加到进程列表

```c
hash_proc(proc);
list_add(&proc_list, &(proc->list_link));
```

调用了`hash_proc`函数和`list_add`函数，先将进程` proc `添加到一个哈希链表中，以便基于` PID `哈希值快速查找该进程，然后将进程 `proc` 添加到一个存储所有进程的链表 `proc_lis`t 中，`list_link `是一个链表节点指针，表示该进程在进程链表中的位置。

```c
static void
hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}
```

```c
static inline void
list_add(list_entry_t *listelm, list_entry_t *elm) {
    list_add_after(listelm, elm);
}
```

- 唤醒新进程

```c
wakeup_proc(proc);
```

调用`wakeup_proc`函数进行唤醒，将 `proc->state` 设为 `PROC_RUNNABLE`

```c
void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}
```

- 返回新进程号

```c
ret = proc->pid;
```

其中，`ucore`确实做到了给每个新`fork`的线程一个唯一的`id`。

上述过程中，有语句`proc->pid = get_pid();`，可以看出分配`id`要依赖`get_pid()`函数，那么我们进入该函数并查看。

```c
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

简要分析该函数的关键部分：

1. 静态变量 `last_pid` 和 `next_safe`：

- `last_pid`：用于追踪上次分配的 `PID`，函数中，每次为新进程分配 `PID` 时，`last_pid` 会增加，直到达到最大 `PID` 值`MAX_PID`，它会回绕到 `1`，避免 `PID `为零（`PID `为零通常保留给初始化进程）。

- `next_safe`：用来追踪当前系统中尚未使用的最小 `PID`，函数中，`next_safe`记录当前进程链表中最大 `PID` 值的下一个安全 `PID`。

2. 分配 `PID` 的流程：

- 首先检查` last_pid` 是否已经达到了 `MAX_PID`，如果是，则回绕到` 1`。

- 当每次生成一个新的 `ast_pid` 时，函数会遍历` proc_list`，检查当前的 `last_pid `是否已经被其他进程使用：如果发现冲突，则递增` last_pid`，直到找到一个没有被使用的 `PID`。`（*）`

- 然后更新` next_safe`，帮助优化 `PID `分配过程，使得每次寻找下一个可用 `PID` 时，能够从最后分配的 `PID` 开始，避免从头遍历所有进程。

3. 遍历进程链表：

- 进程链表 `proc_list `保存着所有已创建的进程，通过遍历链表，`get_pid` 会检查每个进程的 `PID`，确保新的 `PID `不与已存在的 `PID `冲突。`（*）`

4. 回绕与重试：

- 如果 `last_pid `超过了 `next_safe`，则会重新扫描链表并更新 `next_safe`；如果 `PID `冲突，`last_pid `会增加并继续检查直到找到一个唯一的 `PID`。

那么从上述`（*）`机制中，便可以看出`get_pid()`函数中的防止重复赋值的机制。

# 练习3

proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：

- 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
- 禁用中断。你可以使用/kern/sync/sync.h中定义好的宏local_intr_save(x)和local_intr_restore(x)来实现关、开中断。
- 切换当前进程为要运行的进程。
- 切换页表，以便使用新进程的地址空间。/libs/riscv.h中提供了lcr3(unsigned int cr3)函数，可实现修改CR3寄存器值的功能。
- 实现上下文切换。/kern/process中已经预先编写好了switch.S，其中定义了switch_to()函数。可实现两个进程的context切换。
- 允许中断。

请回答如下问题：

- 在本实验的执行过程中，创建且运行了几个内核线程？
- 完成代码编写后，编译并运行代码：make qemu

如果可以得到如 附录A所示的显示内容（仅供参考，不是标准答案输出），则基本正确。

答：
对`proc_run`的编写如下

```c
void
proc_run(struct proc_struct *proc) 
{
    if (proc != current) 
    {
        local_intr_save(current->need_resched);
        lcr3(proc->cr3);
        struct proc_struct *prev = current;
        current = proc;
        switch_to(&(prev->context), &(proc->context));
        local_intr_restore(proc->need_resched);
    }
}
```

此处仍然按照指导书给出的思路，对我们编写的代码进行解释：

- 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。

```c
if (proc != current) 
```

- 禁用中断。你可以使用`/kern/sync/sync.h`中定义好的宏`local_intr_save(x)`和`local_intr_restore(x)`来实现关、开中断。

```c
local_intr_save(current->need_resched);
```

```c
local_intr_restore(proc->need_resched);
```

- 切换当前进程为要运行的进程。

```c
struct proc_struct *prev = current;
current = proc;
```

- 切换页表，以便使用新进程的地址空间。`/libs/riscv.h`中提供了`lcr3(unsigned int cr3)`函数，可实现修改`CR3`寄存器值的功能。

```c
lcr3(proc->cr3);
```

- 实现上下文切换。`/kern/process`中已经预先编写好了`switch.S`，其中定义了`switch_to()`函数。可实现两个进程的`context`切换。

```c
switch_to(&(prev->context), &(proc->context));
```

此时可以分析出，在本实验的执行过程中，创建且运行了两个内核线程：

- `0`号线程`idlepro`c：第一个内核进程，完成内核中各个子系统的初始化

- `1`号线程`initproc`：辅助完成实验的内核进程

# 扩展练习

说明语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);是如何实现开关中断的？

首先找出语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`

```c
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);
```

可以看到宏定义函数 `#define local_intr_save(x)` 和` #define local_intr_restore(x) ` 分别对内联函数 `static inline bool __intr_save(void)` 和` static inline void __intr_restore(bool flag)`进行了调用。

首先是`local_intr_save(x)`通过`do...while(0)`循环对` __intr_save();`进行调用，在确定` sstatus `的中断使能位`SIE`为`1`时，便会调用`intr_disable()`函数进行中断并返回`1`，此时`x`被赋值为`1`：即遇到中断的情况，会进行中断并将`x`赋值为`1`。

然后` local_intr_restore(x)` 函数会传入`x`作为`flag`标志位 ，若`x`为`1` ，即已中断，那么 `__intr_restore`函数便会进入if分支，调用 `intr_enable();` 恢复进程：即若进程中断，`x==1`会被检测到，进程会被恢复。

如此两个函数配合，便可以实现开关的中断与恢复。s

