/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/


#ifndef TASKTHREADH
#define TASKTHREADH

#include "types.h"
#include "command.h"
#include "loader.h"
#include "vm.h"
#include "io.h"
#include "signals.h"
#include "scheduler.h"
#include "mem_layout.h"
#include "exception.h"

/* Task status */
#define TSK_NOTHING    0        /* Task is not being used         */
#define TSK_LOADING    1        /* Task is being loaded           */
#define TSK_NORMAL     2        /* Task is properly created	      */
#define TSK_KILLING    3        /* Task is on death row           */
#define TSK_MMAPPING   4        /* Task is properly created	      */
#define TSK_KILLED     5        /* Task has been killed           */

/* Task flags */
#define TSK_FLAG_NONE			0   /* No flags present                                    */
#define TSK_FLAG_FILEOPEN		1	/* task file is open on the filesystem		           */
#define TSK_FLAG_LOADING		2   /* Task is being loaded                                */
#define TSK_FLAG_SERVICE		8	/* this task is a service					           */
#define TSK_FLAG_SYS_SERVICE	(TSK_FLAG_SERVICE | 16)  /* this task is a system service  */

struct pm_task 
{
	UINT16 id;				   /* PMAN task ID												 */
	UINT16 state;              /* Defines task current state							     */
	UINT16 flags;			   /* TSK_FLAG_XXX ORed together								 */

	struct pm_thread *first_thread;    /* First thread for this task								 */
	unsigned short num_threads;        /* How many threads where crated for the task so far          */
	
	/* Loader Module */
	struct loader_info loader_inf;

	/* Command Manager Module */
	struct command_info command_inf;

	/* VM Module */
	struct task_vmm_info vmm_inf;  

	/* IO Module */
	struct fsio_event_source io_event_src;	/* IO Event source Descriptor                        */
	struct io_event io_finished;			/* IO Finished event                                 */
	struct tsk_swp_io_evt swp_io_finished;	/* Swap IO finished event                            */
	struct exception_management exeptions;

	UINT32 tsk_bss_end;
	UINT16 killed_threads;
} PACKED_ATT; 


/* Thread status */
#define THR_NOTHING  0    /* Thread is not being used					*/
#define THR_WAITING  1    /* Thread actually is waiting for scheduling	*/
#define THR_RUNNING  2    /* Thread is running							*/
#define THR_BLOCKED  3    /* Thread is blocked							*/
#define THR_INTHNDL  4    /* Thread is an interrupt handler				*/
#define THR_KILLED   5    /* Thread is on death row						*/
#define THR_EXEPTION 6	  /* Thread was stopped because of an exception */
#define THR_SBLOCKED 7    /* Thread is blocked (by a signal)			*/
#define THR_INTERNAL 8	  /* This is an internal thread                 */

/* Thread flags */
#define THR_FLAG_NONE			0
#define THR_FLAG_PAGEFAULT		1	/* page fault is being processed							*/
#define THR_FLAG_PAGEFAULT_TBL  2	/* Page fault being attended also raised a page table fault */		

struct pm_thread 
{
	UINT16 id;			                    /* PMAN tread id											  */
	UINT16 task_id;		                    /* PMAN task id												  */
	UINT16 state;	                        /* one of THR_NOTHING, ... THD_KILLED                         */
	UINT16 flags;

	/* Scheduler Node */
	struct sch_node sch;

	/* Paging */
	struct thread_vmm_info vmm_info;		/* VMM info */

	/* Thread List */
	struct pm_thread *next_thread;		    /* next thread in task's thread linked list                   */

	/* Interrupt */
	UINT32 interrupt;						/* Interrupt number asociated with this thread                */

	/* Stack Address */
	ADDR stack_addr;						/* Thread stack address.                                      */

	/* IO Module */
	struct fsio_event_source io_event_src;	/* IO Event source Descriptor                                 */
	struct io_event io_finished;			/* IO Finished event                                          */
	struct swp_io_evt swp_io_finished;		/* Swap IO finished event                                     */

	/* Signals */
	struct thr_signals signals;			    /* This is the signals container                              */
} PACKED_ATT;


/* Thread and task id assignation boundaries */
#define MAX_TSK 64

#define STACK_SLOT_ADDRESS(slot)	((ADDR)STACK_ADDR(PMAN_THREAD_STACK_BASE - slot * 0x20000))
#define STACK_ADDR(a)				((UINT32)a - 4)

void thr_init();
void tsk_init();

struct pm_thread *thr_get(UINT16 id);
struct pm_task *tsk_get(UINT16 id);

UINT16 tsk_get_id(UINT32 lower_bound, UINT32 upper_bound); 
UINT16 thr_get_id(UINT32 lower_bound, UINT32 upper_bound);

int thr_destroy_thread(UINT16 thread_id);
struct pm_thread *thr_create(short task_id, short flags, short interrupt, void *entry_point, int *ret);

BOOL tsk_destroy(struct pm_task *task);

#endif

