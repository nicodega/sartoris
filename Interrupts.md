# What is an Interrupt? #

An interrupt is a piece of code, that gets executed by the processor in an asynchronous fashion. It might be triggered because of the system timer, or by different devices configured to do so.

Each architecture usualy has a way to mask interrupts, so a given piece of code can be executed in an atomic fashion. However, most of the time, there are some interrupts which cannot be masked (like Page Fault interrupts on x86 architecture).

# Sartoris and Interrupt Handling #

Initially Sartoris sets up the system so no interrupts are serviced by the user. A System service runing at privilege 0 can attach a given thread to one or more interrupts, issuing a call to `create_int_handler` System Call. This system call will ensure (if successful) that each time the processor rises (if not masked) the interrupt, the given thread will be runned.

On sartoris there is no way a thread might tell if it was runned by a call to `run_thread` or because of an interrupt.

When invoking `create_int_handler` System Call, a thread `id` must be provided as the handler for the interrupt. If `nesting` parameter is 1, the handler thread **must** retorn from the interrupt using `ret_from_int` system Call. If `nesting` is 0 the only way out of the handler is by issuing a call to `run_thread`, to switch context to the other thread.

# Sartoris Interrupt Stack #

Each time a nesting interrupt is rised by the processor before performing a switch to the handler thread, sartoris will push onto an internal stack the id of the current running thread. When the interrupt handler invokes `ret_from_int` Sartoris will return execution to the first thread on the stack, poping it.

This mechanism ensures that if an interrupt handler is interrupted by a higher privilege interrupt handler, when the later completes, execution will be returned the the other handler. It's important however, to notice, that if a nesting interrupt is running, it unmasks interrupts, and a non-nesting interrupt handler is executed, execution the the former handler wont return, until a new nesting interrupt is generated or a call to `resume_int` is issued.

From version 2 of the microkernel, there's also a way to manually operate with the interrupt stack. This can be used only by privilege 0 services and must be used with caution.