/*  
 *   Sartoris microkernel i386 thread support functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar      
 */

#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "kernel-arch.h"
#include "tss.h"
#include "descriptor.h"
#include "paging.h"

#ifdef PAGING
extern pd_entry *tsk_pdb[MAX_TSK];   /* needed to quickly */
#endif

extern struct tss thr_tss[MAX_THR];  
extern void arch_switch_thread(int thr_id, unsigned int cr3);

int arch_create_thread(int id, int priv, struct thread* thr) 
{
	build_tss(id, thr->task_num, priv, thr->ep, thr->stack);
	
	return 0;
}

int arch_destroy_thread(int id, struct thread* thr) 
{
	return 0;
}

int arch_run_thread(int id) 
{
	unsigned int tsk_sel[2];

#ifdef PAGING
	int tsk_num;
	pd_entry *page_dir_base;

	tsk_num = threads[id].task_num;
	page_dir_base = tsk_pdb[tsk_num];

	if (page_dir_base == NULL) 
		return FAILURE;

	arch_switch_thread(id, (unsigned int)page_dir_base);
#else
	arch_switch_thread(id, 0);
#endif
	return SUCCESS;
}


#ifdef _SOFTINT_
int arch_run_thread_int(int id, void *eip, void *stack)
{
	unsigned int tsk_sel[2];

#ifdef PAGING
	int tsk_num;
	pd_entry *page_dir_base;

	tsk_num = threads[id].task_num;
	page_dir_base = tsk_pdb[tsk_num];

	if (page_dir_base == NULL) 
		return FAILURE;

	arch_switch_thread_int(id, (unsigned int)page_dir_base, eip, stack);
#else
	arch_switch_thread_int(id, 0, eip, stack);
#endif
	return SUCCESS;
}
#endif





