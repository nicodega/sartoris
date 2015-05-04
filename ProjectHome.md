**The Sartoris Project aim is to develop a portable microkernel and a set of operating system services that support:**

  * The efficient implementation of local system calls.

  * Concurrent execution of several OS 'personalities', ie a UNIX environment and a native microkernel-based interface.

  * Simple and elegant integration of distributed operating system components.

**The microkernel implements a minimal system call set, that presents the following abstractions to the operating system:**

  * **Tasks.** A task is composed by a virtual address space, a set of inter task communication objects, and a set of permissions for accessing the input/output address space.

  * **Threads.** A thread is a path of execution within a task. The interrupt handling in a Sartoris-based system is performed using interrupt-driven threads.

  * **Messaging.** The microkernel implements an asynchronous fixed-length inter-task messaging system.

  * **Shared memory.** The microkernel memory-sharing mechanism allows a task to share portions of its address space with other tasks in a secure way. In systems that support paging, this will be implemented using memory aliasing.

Currently, we have a working implementation of the microkernel for the x86 family of processors.

The minimalization of the kernel moves most of the operating system design issues to the operating system servers framework. Currently, we have implemented a toy unix-like operating system to stress the microkernel, named Oblivion, and a more feature complete multi-server system, Asgard.