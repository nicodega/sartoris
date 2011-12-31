#ifndef __TSS_H
#define __TSS_H

#include "ttrace.h"

/* the back_link and all the segment selectors 
 * (ss0, ss1, ss2, es, cs, ss, ds, fs, gs, ldt_sel)
 * must have their higher 16 bits set to zero
 */

struct tss 
{
	unsigned int back_link;
	unsigned int esp0;
	unsigned int ss0;
	unsigned int esp1;
	unsigned int ss1;
	unsigned int esp2;
	unsigned int ss2;
	unsigned int cr3;
	unsigned int eip;
	unsigned int eflags;
	unsigned int eax;
	unsigned int ecx;
	unsigned int edx;
	unsigned int ebx;
	unsigned int esp;
	unsigned int ebp;
	unsigned int esi;
	unsigned int edi;
	unsigned int es;
	unsigned int cs;
	unsigned int ss;
	unsigned int ds;
	unsigned int fs;
	unsigned int gs;
	unsigned int ldt_sel;
	unsigned int io_map;
};

#define SFLAG_MMXFPU_STORED     0x1     // thread has MMX/FPU/SSE state stored (remember to change the value if changed on state_switch.s)
#define SFLAG_RUN_INT           0x2     // thread is runing a software interrupt (remember to change the value if changed on state_switch.s)
#define SFLAG_RUN_INT_START     0x4     // thread is starting to run as a soft int
#define SFLAG_RUN_INT_STATE     0x8     // thread was running on a soft int and it was interrupted
#define SFLAG_TRACEABLE         0x10    // thread is "traceable", we must wind/unwind the stack (remember to change the value if changed on stack_windings.s)
#define SFLAG_DBG               0x20    // if this flag is set, winding must save/restore debug information
#define SFLAG_TRACE_REQ         0x40    // if this flag is set, a task requested tracing on the thread.
#define SFLAG_TRACE_END         0x80    // if this flag is set, tracing stopped for the thread and it's stack winding must be removed for the last time.

/* Now we use a custom state management structure for threads  */
struct thr_state
{
	unsigned int sflags;	// a bunch of sartoris flags
	unsigned int eip;
	unsigned int eflags; 
	unsigned int ebx;
	unsigned int ebp;
	unsigned int esp;
	unsigned int esi;
	unsigned int edi;
	unsigned int ss;
	unsigned int fs;
	unsigned int gs;
	unsigned int cs;
	unsigned int es;
	unsigned int ds;
	unsigned int ldt_sel;
	struct regs *stack_winding;    // this will contain common and segment registers when an interrupt or run_thread is invoked
                                   // it will be filled only when ttrace is active on the thread and the interrupt is not generated
                                   // on sartoris code.
    unsigned int sints;            // ammount of interrupts triggered while on sartoris code when a trace is being performed
#ifdef FPU_MMX
	unsigned int mmx[128];         // for preserving FPU/MMX registers (given the size of thread and all the c_thread_unit, this will be 16 byte aligned)
#endif
};

void build_tss(struct thr_state *thr_state, int id, int task_num, int priv, void *ep, void *stack);

/* This will be the only TSS we will use ever. */
extern struct tss global_tss;


#endif


