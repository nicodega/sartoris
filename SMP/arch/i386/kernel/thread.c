/*  
 *   Sartoris microkernel i386 thread support functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@gmail.com      
 */

#include "lib/indexing.h"
#include "lib/containers.h"
#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "kernel-arch.h"
#include "tss.h"
#include "descriptor.h"
#include "paging.h"
#include "reg-calls.h"


extern struct tss thr_tss[MAX_THR];  
extern void arch_switch_thread(struct thr_state *thr, int thr_id, unsigned int cr3);
extern int arch_switch_thread_int(struct thr_state *thr, int id, unsigned int cr3, void *eip, void *stack);
extern struct thr_state *mmx_state_owner;

int ARCH_FUNC_ATT3 arch_create_thread(int id, int priv, struct thread* thr) 
{		
	build_tss((struct thr_state *)CONT_THR_STATE_PTR(thr), id, thr->task_num, priv, thr->ep, thr->stack);
		
	return 0;
}

int ARCH_FUNC_ATT2 arch_destroy_thread(int id, struct thread* thr) 
{
#ifdef FPU_MMX	
	struct thr_state *thrs = (struct thr_state*)CONT_THR_STATE_PTR(thr);

	// if thread is state owner of MMX
	// set mmx_state_owner to 0xFFFFFFFF
	if(mmx_state_owner == thrs)
	{
		mmx_state_owner = (struct thr_state *)0xFFFFFFFF;
	}
#endif

	return 0;
}

int ARCH_FUNC_ATT1 arch_run_thread(int id)
{
	int tsk_num;
	unsigned int tsk_sel[2];

	tsk_num = GET_PTR(id,thr)->task_num;

#ifdef PAGING
	pd_entry *page_dir_base;

	page_dir_base = GET_TASK_ARCH(tsk_num)->pdb;

	if (page_dir_base == NULL) 
		return FAILURE;
#endif

	/* 
	We will now modify LDT descriptor on GDT. This won't affect us
	because our segments are pointing directly to the GDT.
	*/
	switch_ldt_desc(GET_TASK_ARCH(tsk_num), GET_PTR(tsk_num,tsk)->priv_level);
	
#ifdef PAGING
	arch_switch_thread(GET_THRSTATE_ARCH(id), id, (unsigned int)page_dir_base);
#else
	arch_switch_thread(GET_THRSTATE_ARCH(id), id, 0);
#endif
	return SUCCESS;
}

int ARCH_FUNC_ATT3 arch_run_thread_int(int id, void *eip, void *stack)
{
	int tsk_num;
	unsigned int tsk_sel[2];

	tsk_num = GET_PTR(id,thr)->task_num;

#ifdef PAGING
	pd_entry *page_dir_base;

	page_dir_base = GET_TASK_ARCH(tsk_num)->pdb;

	if (page_dir_base == NULL) 
		return FAILURE;
#endif

	/* 
	We will now modify LDT descriptor on GDT. This won't affect us
	because our segments are pointing directly to the GDT.
	*/
	switch_ldt_desc(GET_TASK_ARCH(tsk_num), GET_PTR(tsk_num,tsk)->priv_level);
	
#ifdef PAGING
	arch_switch_thread_int(GET_THRSTATE_ARCH(id), id, (unsigned int)page_dir_base, eip, stack);
#else
	arch_switch_thread_int(GET_THRSTATE_ARCH(id), id, 0, eip, stack);
#endif
	return SUCCESS;
}
