/*  
 *   Sartoris microkernel arch-neutral main file
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@cvtci.com.ar      
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
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"

/* main kernel data structures:
   tasks, threads, shared memory objects and message queues */

struct task tasks[MAX_TSK];
struct thread threads[MAX_THR];
struct smo_container smoc;
struct msg_container msgc;

/* (task x port) -> queue  mapping, it binds an open port 
                           to a message queue from the 
                           message queue pool */

int open_ports[MAX_TSK][MAX_TSK_OPEN_PORTS];

/* important interrupt handling data */

int int_handlers[MAX_IRQ];            /* int number -> thread handler id           */
unsigned char int_nesting[MAX_IRQ];   /* int number -> bool indicates nesting mode */
unsigned char int_active[MAX_THR];    /* thread id -> bool indicating wether this  *
                                       * thread is already handling an interrupt   */

/* run_thread permissions matrix
   (or who might run whom) */

unsigned int run_perms[MAX_THR][BITMAP_SIZE(MAX_THR)];

/* the infamous software interrupt stack */

int int_stack[MAX_NESTED_INT];
int int_stack_pointer;

int last_int;

/* page fault info */

#ifdef PAGING

struct page_fault last_page_fault = { -1, -1, 0 };

#endif

/* cached current information */

int curr_thread;
int curr_task;
void *curr_base;
int curr_priv;

/* the following function should be called by the bootstrap program */

int initialize_kernel(void) 
{
	int i, j;

	/* initialize text-mode vga */
	k_scr_init();
	k_scr_clear();

	/* initialize tasks */
	for (i = 0; i < MAX_TSK; i++) 
	{
		tasks[i].state = DEFUNCT;
	}

	/* initialize threads */
	for (i = 0; i < MAX_THR; i++) 
	{
		threads[i].task_num = -1;
		for (j = 0; j < (BITMAP_SIZE(MAX_THR)); j++) 
		{
			run_perms[i][j] = 0;
		}
	}

	/* initialize interrupts */
	int_stack_pointer = 0;
	for (i = 0; i < MAX_IRQ; i++) 
	{
		int_handlers[i] = -1;
	}

	last_int = -1;

	/* initialize shared memory objects */
	init_smos(&smoc);

	/* initialize message queues */
	init_msg(&msgc);

	/* initialize a pseudo task/thread with id=0,
		necessary for syscalls from arch_init_cpu */

	curr_base = (void*)KRN_OFFSET;
	curr_task = 0;
	curr_thread = 0;
	curr_priv = 0;

	tasks[0].state = ALIVE;
	tasks[0].size = 0x1000000;	/* sixteen megs */

	/* perform cpu-dependent initialization 
		and create init task/thread according to
		architectural needs */

	arch_init_cpu();

	/* destroy pseudo task/thread */

	tasks[0].state = DEFUNCT;
	tasks[0].size = 0;

	/* the kernel now becomes a passive entity... */
	run_thread(INIT_THREAD_NUM);

	while (1);
}
