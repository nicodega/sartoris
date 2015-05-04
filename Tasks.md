# What is a Task? #

A Task on Sartoris, represents a process. Not the running state of a process but it's code, data, and all static concepts of it.

A task is composed of a [virtual address space](Paging.md), with its physical mappings defined by a set of pages, and also contains a set of [Ports](Messaging.md) and [Shared Memory Objects](SMO.md) assigned.

# Task Creation #

In order to create a Task on the microkernel, a System Service must invoke the `create_task` system call, passing an id and a creation structure to it, defined as follows:

```
struct task{
   int mem_adr;
   int size; 
   int priv_level;
};
```

When Paging is disabled, `mem_adr` indicates the physical address where this task should be placed in main memory. If Paging is enabled, this field will be the desired task Base Address, and it must be greater than `MIN_TASK_OFFSET` defined on `kernel.h`.

`Size` indicates the size in bytes of the task being created, and `priv_level` indicates its privilege level. This number might be used by the operating system to restrict the ability to send messages to ports and to run specific threads using privilege levels. Zero is the most privileged level, and levels zero and one have a special meaning to the microkernel, because they are the only levels that can access the input/output space.

Tasks are destroyed using the similar function `destroy_task`, which receives the **id** of the task being destroyed.

It should be noted that System Calls to create and destroy tasks are restricted to tasks running in privilege level zero.


---


# x86 Implementation #

Each task on the i386 architecture, is composed of an LDT segment, a page directory and a set of pages assigned to the task address space.

Sartoris defines only 4 LDT segment descriptors on the GDT, each one with a different privilege level set from 0 to 3. When switching to a given Task Thread, sartoris will modify the corresponding LDT segment descriptor so it points to the new Threads Task LDT. This turns to be useful for [dynamic memory](DynamicMemory.md).