# lab5实验报告

## 练习0：填写已有实验
由于本次实验的需要，需要在lab4所填写函数`alloc_proc(void)`中，增添更多的变量初始化。
```c
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        //lab4新增
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

        //lab5新增
        proc->wait_state = 0;
        proc->cptr = NULL;
        proc->optr = NULL;
        proc->yptr = NULL;
    }
    return proc;
}
```

## 练习1：加载应用程序并执行（需要编码）

### 1.1 `load_icode`代码补充

对`load_icode`的第6步补充如下：
```c
tf->gpr.sp = USTACKTOP;
tf->epc = elf->e_entry;
tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);
```

大体来说，这三句代码对用户进程的trapframe进行设置，以确保进程从内核返回到用户模式时，可以**正确地恢复其执行状态**。

下面对这三条语句的作用进行说明

### 1.2 tf->gpr.sp = USTACKTOP;

- 该句用来**设置栈指针**

`trapframe` 中用`gpr.sp`保存用户栈指针，指向当前栈的顶端位置。

`USTACKTOP` 则是用户栈的顶部地址。

所以此处我们编写`tf->gpr.sp = USTACKTOP` ，将用户进程的栈指针初始化为用户栈的顶部位置，使栈在进程开始时为空，这样进程就可以通过栈空间，对函数调用和局部变量等进行正确可靠的存储。

### 1.3 tf->epc = elf->e_entry;

- 该句用来**设置程序入口点**

`tf->epc` 是 `trapframe` 中保存用户程序的入口点地址（exception program counter），指定了进程在用户态下开始执行的位置。

`elf->e_entry` 是 `ELF` 文件头中的一个字段，表示该程序的入口地址，即程序开始执行的地方。

所以此处我们编写 `tf->epc = elf->e_entry` ，就是为了确保进程在进入用户态后，从`elf->e_entry`开始执行。

### 1.4 tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);

- 该句用来**设置状态寄存器**

`tf->status` 是`trapframe`中状态寄存器，保存进程的状态。

`sstatus` 是当前内核态下的状态寄存器的值。

`SSTATUS_SPP` 是 `sstatus` 的标志位之一，指示当前进程是否处于内核模式：若处在内核态，`SPP`标志位则被设置。

`SSTATUS_SPIE` 也是 `sstatus` 的标志位之一，指示当前进程是否启用了中断：若处在内核态，`SPIE`标志位也被设置。

所以此处我们编写 `tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);` ，就是将 `sstatus` 中的 `SSTATUS_SPP` 和 `SSTATUS_SPIE` 两标志位清零，然后进行赋值，以确保进程返回到时不会保留内核模式的特权标志，被正确设置为用户模式。

### 1.5 执行过程描述

- 在此对用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过进行描述

在init_main中调用kernel_thread
```c
static int
init_main(void *arg) {
    ...
    int pid = kernel_thread(user_main, NULL, 0);
    ...
}
```
kernel_thread中调用do_fork创建并唤醒进程
```c
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    ...
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}
```
随之执行参数中函数user_main，进程开始执行
```c
static int
user_main(void *arg) {
...
#else
    KERNEL_EXECVE(exit);
...
}
```
user_main函数调用宏KERNEL_EXECVE
```c
#define KERNEL_EXECVE(x) ({                                             
            ...
            __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,     \
                            _binary_obj___user_##x##_out_size);         \
        })
```
宏KERNEL_EXECVE调用kernel_execve,执行ebreak，发生断点异常
```c
static int
kernel_execve(const char *name, unsigned char *binary, size_t size) {
    ...
    asm volatile(
        ...
        "ebreak\n"
        ...)
    ...
}
```
断点异常产生，执行trap函数,调用trap_dispatch函数
```c
void trap(struct trapframe *tf) { trap_dispatch(tf); }
```
trap_dispatch函数调用exception_handler函数
```c
static inline void trap_dispatch(struct trapframe *tf) {
    ...
    else {
        exception_handler(tf);
    }
}
```
trap_dispatch函数执行到CAUSE_BREAKPOINT处,调用syscall
```c
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        ...
         case CAUSE_BREAKPOINT:
            if(tf->gpr.a7 == 10){
                tf->epc += 4;
                syscall();
                kernel_execve_ret(tf,current->kstack+KSTACKSIZE);
            }
            break;
        ...
    }
}
```
syscall根据参数，执行sys_exec，调用do_execve
```c
static int
sys_exec(uint64_t arg[]) {
    ...
    return do_execve(name, len, binary, size);
}
```
do_execve调用load_icode，加载文件
```c
int
do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    ...
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;
    }
    ...
}
```
加载完毕，返回至__alltraps的末尾，执行__trapret后的内容，直到sret，退出内核态，回到用户态，执行加载的文件。
```c
.globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
```

## 练习2：父进程复制自己的内存空间给子进程（需要编码）

### 2.1 代码实现
```c
uintptr_t *src_kvaddr = page2kva(page);
uintptr_t *dst_kvaddr = page2kva(npage);
memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
ret = page_insert(to, npage, start, perm);
```
- 首先通过`page2kva`获取源页面`page`和目标页面`npage`在内核虚拟地址空间中的地址,将这两个地址分别赋值给`src_kvaddr`与`dst_kvaddr`。

- 通过`memcpy`将 `src_kvaddr` 中的数据复制到 `dst_kvaddr`，即将源页的内容完整地复制到目标页，其中字节数为`PGSIZE`。

- 在目标进程的页表中创建一个新的映射，指向新分配的目标物理页面，从而使目标进程能够访问复制后的数据。

- 最后调用`page_insert`， 将目标页`npage` 插入到页表中。

### 2.2 COW 概要设计

#### 2.2.1 初始化（fork）时的处理
当 fork() 被调用时，操作系统为子进程复制父进程的页表，并设置页表项的标志为只读，使父子进程将共享相同的物理内存页，并且页表项都设置为 COW 标志。

#### 2.2.2 页面错误处理（COW触发）
当父进程或子进程尝试修改共享的内存时，硬件会产生页面错误，此时页面错误处理程序便会捕获这一异常。

#### 2.2.3 COW处理
如果是父进程或子进程修改共享页面，就分配一个新的物理页面，将旧共享页面中内容复制到新页面中，同时更新进程的页表，指向新分配的页面，将新页面设置为可写。新页面配置完成后，便一直保留父子进程之间的共享关系，直到某一方写入该页。

#### 2.2.4 值得注意的边界条件
如果多个进程同时修改一个共享页面，则每个进程需要独立复制该页面，各自分配一个新的物理页面，操作自己的独立副本。

## 练习3： 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）
请在实验报告中简要说明你对 fork/exec/wait/exit 函数的分析。并回答如下问题：

请分析fork/exec/wait/exit的执行流程。重点关注哪些操作是在用户态完成，哪些是在内核态完成？内核态与用户态程序是如何交错执行的？内核态执行结果是如何返回给用户程序的？
请给出ucore中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）
### 3.1 fork/exec/wait/exit 
#### 3.1.1 fork：
可以通过`sys_fork`或`kernel_thread`等调用`do_fork`函数，以创建并唤醒进程。

```c
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;

    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }
    proc->parent = current;
    assert(current->wait_state == 0); // 确保当前进程在等待

    if (setup_kstack(proc) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    if (copy_mm(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc); // 设置进程链接
    }
    local_intr_restore(intr_flag);
    wakeup_proc(proc);
    ret = proc->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```
- 检查当前进程数是否超过最大进程数 `MAX_PROCESS`，如果是则返回错误;尝试分配一个新的进程结构 `proc`，如果分配失败则退出。
- 设置新进程的父进程为当前进程，并确保当前进程不在等待状态。
- 分配并初始化新进程的内核栈，复制当前进程的内存空间，然后复制当前进程的线程信息。
- 函数保存中断状态并设置新进程的进程ID，加入进程哈希表并设置进程链接。
- 最后，恢复中断并唤醒新进程，使其准备好运行，并返回新进程的进程ID。

#### 3.1.2 exec
可以通过`sys_exec`调用`do_execve`函数，创建用户空间，以加载用户程序。

```c
int
do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    struct mm_struct *mm = current->mm;
    if (!user_mem_check(mm, (uintptr_t)name, len, 0)) {
        return -E_INVAL;
    }
    if (len > PROC_NAME_LEN) {
        len = PROC_NAME_LEN;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);

    if (mm != NULL) {
        cputs("mm != NULL");
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    int ret;
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;
    }
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}
```
- 检查传入的程序名称是否有效，并确保程序名称长度不超过 `PROC_NAME_LEN`，然后复制名称到本地缓冲区。
- 如果当前进程已有内存映射，则释放当前进程的内存空间:调用 `lcr3` 切换页目录，减少 `mm` 引用计数并清理内存。
- 调用 `load_icode` 加载新程序的二进制内容，设置进程名称并返回成功。

#### 3.1.3 wait
可以通过`sys_wait`或`init_main`调用`do_wait`函数，以等待进程完成。

```c
int
do_wait(int pid, int *code_store) {
    struct mm_struct *mm = current->mm;
    if (code_store != NULL) {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1)) {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;
    if (pid != 0) {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    }
    else {
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    }
    if (haskid) {
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();
        if (current->flags & PF_EXITING) {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == idleproc || proc == initproc) {
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL) {
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}
```

- 如果传入的 code_store 非空，检查其是否可访问。
- 检查指定的子进程 pid 是否存在，若 pid != 0，则查找该子进程；若 pid == 0，则检查当前进程的所有子进程。
- 若找到子进程且子进程处于 PROC_ZOMBIE 状态，即已经结束，便进入处理流程；若没有找到子进程，则进入休眠状态，等待子进程的退出，并再次调度。
- 若当前进程被标记为退出状态，则调用 do_exit 退出。
- 若找到了结束的子进程，则将其退出状态码存储到 code_store，并清理该进程的栈和内存。
- 在过程中，若等待 idleproc 或 initproc，则触发错误。

#### 3.1.4 exit

可以通过`sys_exit`、`trap`、`do_execve`或`do_wait`调用`do_exit`函数，以退出进程。

```c
int
do_exit(int error_code) {
    if (current == idleproc) {
        panic("idleproc exit.\n");
    }
    if (current == initproc) {
        panic("initproc exit.\n");
    }
    struct mm_struct *mm = current->mm;
    if (mm != NULL) {
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;
    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        proc = current->parent;
        if (proc->wait_state == WT_CHILD) {
            wakeup_proc(proc);
        }
        while (current->cptr != NULL) {
            proc = current->cptr;
            current->cptr = proc->optr;
    
            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL) {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE) {
                if (initproc->wait_state == WT_CHILD) {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);
    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}
```
- 检查当前进程是否为 `idleproc` 或 `initproc`，若是，则触发内核错误，因为这两个进程不应退出。
- 若当前进程有内存空间，则释放当前进程的内存映射：调用 `lcr3` 切换到 `boot_cr3`，并减少 `mm` 引用计数，最终销毁内存。
- 将当前进程的状态设置为 `PROC_ZOMBIE`，并保存退出码。
- 保存中断状态并进入临界区，处理当前进程的父进程和子进程。
- 如果父进程处于等待状态 ，则唤醒父进程。
- 遍历并处理当前进程的所有子进程，将它们的父进程改为 `initproc`，并调整子进程链表。
- 若某个子进程已经处于 `PROC_ZOMBIE` 状态，并且 `initproc` 处于等待状态，则唤醒 `initproc`。
- 调用`schedule`切换到其他进程。

### 3.2 用户态与内核态
#### 3.2.1 do_fork
内核态操作：

- alloc_proc()：为新进程分配进程结构。
- setup_kstack()：为新进程分配并初始化内核栈。
- copy_mm()：复制当前进程的内存空间。
- copy_thread()：复制线程状态。
- 分配进程ID、设置进程哈希表、链接新进程等。
- 调用 schedule() 进行进程调度。
- wakeup_proc()：唤醒新进程。
- 进程的状态更新和资源清理。

用户态操作：

- 用户程序在 fork 调用后会返回一个新进程的 PID，进入用户态执行。
#### 3.2.2 do_execve
内核态操作：

- user_mem_check()：检查用户传入的参数（程序名称）是否有效。
- 内存清理操作：如果进程已拥有内存空间，释放并清理当前进程的内存映射。
- load_icode()：加载指定的程序二进制内容。
- 设置当前进程的名称和新的内存映射。

用户态操作：

- 用户程序调用 execve 后，进程的地址空间被新的程序加载覆盖，返回后开始执行新的程序。
#### 3.2.3 do_wait
内核态操作：

- user_mem_check()：检查传入的 code_store 是否可访问。
- find_proc()：查找并管理子进程。
- 如果当前进程的子进程未退出，则进入睡眠状态并调用 schedule() 进行调度。
- 对已结束的子进程进行资源清理，回收其内存和进程结构。
- 处理与进程状态相关的更新，如更新进程的父子关系，唤醒父进程等。

用户态操作：

- 用户程序调用 wait()，当子进程退出时，它会从 do_wait 返回并获取子进程的退出码。
#### 3.2.4 do_exit
内核态操作：

- 释放当前进程的内存映射，清理进程资源。
- 设置进程状态为 PROC_ZOMBIE，并记录退出码。
- 处理父进程和子进程的状态，通知父进程或 initproc 唤醒，清理当前进程的子进程。
- 调用 schedule() 进行进程调度。

用户态操作：

- 用户程序通过系统调用触发进程退出，之后程序会返回并终止。
#### 3.2.5 互相转换
- 内核态通过系统调用结束后的sret指令切换到用户态。
- 用户态通过发起系统调用产生ebreak异常切换到内核态
#### 3.2.6内核态返回
- 内核态执行的结果通过kernel_execve_ret将中断帧添加到进程的内核栈中，从而将结果返回给用户

### 3.3 生命周期图
```plaintext
+------------------+              +-----------------+
|    PROC_UNINIT   | ------------>|  PROC_RUNNABLE  |
|  (未初始化)      |   alloc_proc |  (就绪)         |
+------------------+              +-----------------+
        |                                | do_fork
        |                                V
        |                         +-----------------+
        |                         |  PROC_RUNNING   |
        |                         |  (运行中)       |
        |                         +-----------------+
        |                                |
        |                            do_exit
        |                                V
        |                         +-----------------+
        |                         |  PROC_ZOMBIE    |
        |                         |  (僵尸)         |
        |                         +-----------------+
        |                                |
        |                            do_wait
        |                                V
        |                         +-----------------+
        |                         |  PROC_SLEEPING  |
        |                         |  (睡眠中)       |
        |                         +-----------------+
        |                                |
        +--------------------------------+
        | wakeup_proc                   |
        V                               |
+-------------------+                 |
|      none         |<----------------+
+-------------------+  

```