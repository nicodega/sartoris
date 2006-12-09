#ifndef _KERNEL_EMU_H_
#define _KERNEL_EMU_H_

/* access to the microkernel threads and tasks data structures */
extern struct task tasks[MAX_TSK];
extern struct thread threads[MAX_THR];

void build_emu_thr(struct emu_thread *emu_thr, struct thread *thr);

#endif
