/*  
 *   Sartoris microkernel arch-neutral main file
 *   
 *   Copyright (C) 2002 - 2007, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com
 */


/*
 * The functions in this section are called from stubs in the i386 section,
 * because gcc doesn't understand far calls (as far as we know).
 * They implement all the microkernel functionality, and invoke the
 * architecture-specific section of the kernel through the interface
 * defined in the file include/sartoris/cpu-arch.h
 *
 */

#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/scr-print.h"
#include "lib/indexing.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/salloc.h"
#include <sartoris/critical-section.h>

#ifdef _METRICS_
#include "sartoris/metrics.h"
#endif

#include "sartoris/scr-print.h"

/* important interrupt handling data */

int int_handlers[MAX_IRQ];            /* int number -> thread handler id           */
unsigned char int_nesting[MAX_IRQ];   /* int number -> bool indicates nesting mode */
unsigned char int_active[MAX_THR];    /* thread id -> bool indicating wether this  *
                                       * thread is already handling an interrupt   */

/* the infamous software interrupt stack */
int int_stack[MAX_NESTED_INT];
int int_stack_pointer;
int last_int;

/* page fault info */
#ifdef PAGING
struct page_fault last_page_fault = { -1, -1, 0 };
#endif

/* cached current information */
int  curr_thread;
int  curr_task;
int  curr_priv;
void *curr_base;

/* the following function should be called by the bootstrap program */
int initialize_kernel(void) 
{
	int i, j;

	/* initialize text-mode vga */
	k_scr_init();
	k_scr_clear();

	/* Initialize critical sections */
	mk_cs_init();

	/* Initialize indexes */
	init_indexes();

	/* Initialize dynamic memory subsystem */
	dyn_init();

	/* initialize containers */
	init_containers();
		
	/* initialize salloc */
	init_salloc();

	/* initialize interrupt stack */
	int_stack_pointer = 0;
	for (i = 0; i < MAX_IRQ; i++) 
	{
		int_handlers[i] = -1;
	}

	last_int = -1;

	// initialize metrics if necessary
#ifdef _METRICS_
	initialize_metrics();
#endif

	/* 
		Initialize a pseudo task/thread with id=0,
		necessary for syscalls from arch_init_cpu 
	*/

	curr_base = (void*)KRN_OFFSET;
	curr_task = 0;
	curr_thread = 0;
	curr_priv = 0;
	
	// Allocate a task
	struct task *t = salloc(0, SALLOC_TSK);
	t->state = ALIVE;
	t->size	 = 0x1000000;	/* sixteen megs */
	
	// allocate a thread
	struct thread *th = salloc(0, SALLOC_THR);
	th->invoke_mode = PRIV_LEVEL_ONLY;
	th->invoke_level = 0;

	/* 
		Perform cpu-dependent initialization 
		and create init task/thread according to
		architectural needs.
	*/
	
	arch_init_cpu();
	
	/* Free pseudo task/thread.
	NOTE: this should be safe because no salloc will
	be performed while switching to the new thread. Hence
	information wont be corrupted. (It's dirty I know...)
	*/
	sfree(t, 0, SALLOC_TSK);	
	sfree(th, 0, SALLOC_THR);
		
	/* the kernel now becomes a passive entity... */
	run_thread(INIT_THREAD_NUM);

	while (1);
}
