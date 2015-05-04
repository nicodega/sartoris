# What is Sartoris? #

Any operating system designed to run modern hardware must overcome the (rather complicated) details of the management of the central processors and their memory management units. In most processor architectures, this implies using specific machine instructions and maintaining a set of data structures in memory from which the hardware can obtain the current state of the system. Furthermore, the handling of interrupt requests and the construction and maintenance of the page tables are themselves nontrivial tasks, increasing the overall complexity of the system.

Therefore, it seems reasonable to encapsulate the routines that interact directly with the processor, and that is what the Sartoris microkernel intends to do. Moreover, in order to provide a suitable abstraction to the operating system, the microkernel does not need to provide many other functions.

## What Can I do With it? ##

Sartoris is ment as the base for creating an Operating System, abstracting itself from CPU control o even used to create a single _real-time_ aplication running alone on top of a processor, with full control of it.

Functionality provided by Sartoris can be grouped as follows:

  * **Task and thread management.** sartoris provides a set of system calls, which cover the creation of processor tasks and threads. Sartoris does not, however, load processes from disk or perform any scheduling of the CPU. These chores are to be performed by services running on top of the microkernel (usually, a process-manager task). Under Sartoris, an interrupt handler is just a special thread that is invoked by the microkernel every time the corresponding interrupt request line is raised. Hence, the scheduling thread may be created by binding an ordinary thread to the timer interrupt.

  * **Memory management.** This subsystem presents an abstracted view of the paging mechanism of the underlying processor, as well as an implementation of inter-task memory sharing through Sartoris SMOs (shared memory objects), which are chunks of binary data that can be easily accessed from multiple tasks. Again, the microkernel does not perform any paging directly (except in exceptional cases like interprocess communication, where some temporary mappings may be created and later destroyed during the execution of a system call). It just provides support for the easy creation and modification of page tables, (mostly) independently of the processor page table format and peculiarities.

  * **Message passing.** The kernel provides an asynchronous messaging system, in the form of a set of ports assigned to each task. Each port functions as a mailbox where fixed-sized messages from other tasks are received. These system calls cover the creation, deletion and management of ports.

Next step on this tutorial is [Sartoris boot process explained.](OSChapter2.md)

## See also ##

These are the wiki pages for each of the Basic Concepts of Sartoris:

  * [Tasks](Tasks.md)
  * [Threads](Threads.md)
  * [Interrupts](Interrupts.md)
  * [Messaging](Messaging.md)
  * [Shared Memory Objects](SMO.md)