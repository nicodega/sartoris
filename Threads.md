# What is a Thread? #

In order to define a Process, you need not only to define it's Virtual Address, code, data etc, but also an execution state. A thread is the processor state associated with a given [Task](Tasks.md). The maximum amount of concurrent threads is determined by the constant `MAX_THR`.

The execution state of a processor includes the following:

  * Execution Pointer
> > This value holds the number assigned to the current instruction.

  * Registers
> > Value of current processor registers required to define processor state on the C
> > convention.

# Thread Creation #

A task might have zero, one, or more threads. Threads are created using the system call `create_thread(int id, struct thread *thr)`. The parameter `id` is an integer that uniquely identifies each thread, and must not exceed `MAX_THR-1`.

The structure `thread` is defined as:

```
struct thread{
  int task_num;
  int invoke_mode;
  int invoke_level;
  int ep;
  int stack;
}; 
```

`task_num` is the task that defines the context in which this thread is to be created (which must have been already created), `ep` is this thread's entry point, `stack` is the initial stack value, and invoke mode is one of the following:

  * `PERM_REQ`: In this mode, it is necessary (additionally to the privilege level constraints) to have specific authorization (obtained through the function `set_thread_run_perm`) to run this thread.

  * `PRIV_LEVEL_ONLY`: In this mode only the usual privilege level constraints are applied to the function `run_thread` applied to this thread.

  * `DISABLED`: The thread is disabled, and it can't be invoked by anyone.

The value of the field `invoke_level` indicates the numerically higher privilege level that can invoke this thread.

# Thread Removal #

Threads are destroyed using the `destroy_thread` system call (It may be worth noting that the currently running thread can't destroy itself), and may be started (or resumed) by interrupt requests signaled to the processor, software generated interrupts, exceptions, and `run_thread` system call.

# Running a Thread #

Unlike [Tasks](Tasks.md), Threads define an execution context, and once they are created, the can be runned with `run_thread(int id)` system call. This system call will perform a state switch between the current executing thread and the thread with the specified `id`. State for the current thread will be preserved and state of the new thread will be loaded in a transparent way.

When current thread is invoked again, it will be impossible to tell that a different thread was runned.


---


# x86 Implementation #

Even though Intel provides us with TSS segments, on the latest version of Sartoris, state preservation is performed by software. we decided to do it software based because it's less expensive, and also uses a unique TSS, and hence a unique TSS descriptor on de GDT, avoiding GDT size limitations to system threads.

Since state switch is performed on sartoris code, we can safely preserve CDECL registers only. The first time software thread switching is executed, we will simulate a FAR call, in order to load the new CS:EIP by performing a RETF instruction. when called on future times, however, we will only use RET. That's because the only way out of a thread on sartoris is because of one of this causes:

  * Current thread invoked run\_thread

  * An interrupt happened.

On the first case, its easy to see that upon state switching we will always return to the `arch_thread_switch` function, which was also the function invoked before the last switch. The second case (when invoked by an interrupt) might not appear so easy, but as sartoris interrupts are invoked by `run_thread` internally, we are on the same case as before.

# MMX/FPU/SSEn State #

Current version of sartoris, will also keep MMX/FPU/SSEn state on demmand.

When a thread uses an MMX instruction (it's the same for FPU/SSEn instructions) sartoris will check if another thread has already used it, and if so, it will preserve MMX state for that thread and load the new thread MMX state.

This actions are performed by setting the TS flag on CR0 of the processor, each time we perform a thread switch only if current thread is not the _owner_ of MMX state and setting EM to 0. When a thread uses MMX, if TS = 1 and EM = 0, an interrupt will be issued and sartoris will perform the MMX state switch, and acknowledge the new thread as the MMX state owner.