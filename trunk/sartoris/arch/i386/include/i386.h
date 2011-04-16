/*  
 *   Sartoris i386 dependent header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */

#ifndef I386
#define I386

#include "sartoris/kernel.h"
#include "task.h"
#include "descriptor.h"
#include "paging.h"
#include "kernel-arch.h"

#define STACK0_SIZE 0x590     /* 1.3 kb. Remember to change it on state_switch if changed here. */

#define __align(a) __attribute__ ((aligned (a)))

extern int curr_thread;
extern int curr_task;
extern void *curr_base;
extern struct page_fault last_page_fault;

unsigned int get_flags(void);

struct i386_task *first_task;

/* Information we will need for each task */
struct i386_task
{
#ifdef PAGING
	/* An awful tasks linked list */
	struct i386_task *next;
	struct i386_task *prev;
	/* Page directory base */
	pd_entry *pdb;
#endif
	/* Task LDT */
	struct seg_desc ldt[LDT_ENT];
};

#define GET_TASK_ARCH(id) ((struct i386_task *)CONT_TSK_ARCH_PTR(GET_PTR(id,tsk)))
#define GET_THRSTATE_ARCH(id) ((struct thr_state*)CONT_THR_STATE_PTR(GET_PTR(id,thr)))

#endif

