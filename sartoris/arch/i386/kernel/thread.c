/*  
 *   Sartoris microkernel i386 thread support functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar      
 */

#include "lib/indexing.h"
#include "lib/containers.h"
#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "kernel-arch.h"
#include "tss.h"
#include "descriptor.h"
#include "paging.h"


extern struct tss thr_tss[MAX_THR];  
extern void arch_switch_thread(struct tss *thread_state, int thr_id, unsigned int cr3);
extern int arch_switch_thread_int(struct tss *thread_state, int id, unsigned int cr3, void *eip, void *stack);

int arch_create_thread(int id, int priv, struct thread* thr) 
{
	build_tss((struct thr_state *)CONT_THR_STATE_PTR(thr), id, thr->task_num, priv, thr->ep, thr->stack);
	
	return 0;
}

int arch_destroy_thread(int id, struct thread* thr) 
{
	return 0;
}

int arch_run_thread(int id)
{
	int tsk_num;
	unsigned int tsk_sel[2];

	tsk_num = ((struct thread*)GET_PTR(id,thr))->task_num;

	/* 
	We will now modify LDT descriptor on GDT. This won't affect us
	because our segments are pointing directly to the GDT.
	*/
	switch_ldt_desc((struct i386_task *)CONT_TSK_ARCH_PTR(GET_PTR(tsk_num,tsk)), ((struct task*)GET_PTR(tsk_num,tsk))->priv_level);

#ifdef PAGING
	pd_entry *page_dir_base;

	page_dir_base = ((struct i386_task *)CONT_TSK_ARCH_PTR(GET_PTR(tsk_num,tsk)))->pdb;

	if (page_dir_base == NULL) 
		return FAILURE;

	arch_switch_thread((struct tss*)CONT_THR_STATE_PTR(GET_PTR(id,thr)), id, (unsigned int)page_dir_base);
#else
	arch_switch_thread((struct tss*)CONT_THR_STATE_PTR(GET_PTR(id,thr)), id, 0);
#endif
	return SUCCESS;
}


#ifdef _SOFTINT_
int arch_run_thread_int(int id, void *eip, void *stack)
{
	int tsk_num;
	unsigned int tsk_sel[2];

	tsk_num = ((struct thread*)GET_PTR(id,thr))->task_num;

	/* 
	We will now modify LDT descriptor on GDT. This won't affect us
	because our segments are pointing directly to the GDT.
	*/
	switch_ldt_desc((struct i386_task *)CONT_TSK_ARCH_PTR(GET_PTR(tsk_num,tsk)), ((struct task*)GET_PTR(tsk_num,tsk))->priv_level);

#ifdef PAGING
	pd_entry *page_dir_base;

	page_dir_base = ((struct i386_task *)CONT_TSK_ARCH_PTR(GET_PTR(tsk_num,tsk)))->pdb;

	if (page_dir_base == NULL) 
		return FAILURE;

	return arch_switch_thread_int((struct tss*)CONT_THR_STATE_PTR(GET_PTR(id,thr)), id, (unsigned int)page_dir_base, eip, stack);
#else
	return arch_switch_thread_int((struct tss*)CONT_THR_STATE_PTR(GET_PTR(id,thr)), id, 0, eip, stack);
#endif
}
#endif





