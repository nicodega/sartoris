#ifndef __TSS_H
#define __TSS_H

#define SWITCH_FLUSH_TLB

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

#define SFLAG_MMXFPU_STORED     0x1     // thread has MMX/FPU/SSE state stored
#define SFLAG_SSE               0x2     // SSE, SSE2 and SSE3
#define SFLAG_RUN_INT           0x4     // thread is runing a software interrupt

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
#ifdef FPU_MMX
	unsigned int mmx[128]; // for preserving FPU/MMX registers
#endif
#ifdef SSE
	
#endif
};

void build_tss(struct thr_state *thr_state, int id, int task_num, int priv, void *ep, void *stack);

/* This will be the only TSS we will use ever. */
extern struct tss global_tss;


#endif

