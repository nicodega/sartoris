/*  
 *   Sartoris microkernel i386 task-support functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@gmail.com      
 */

#include "sartoris/cpu-arch.h"
#include "kernel-arch.h"
#include "descriptor.h"
#include "lib/containers.h"
#include "lib/indexing.h"
#include "i386.h"

int arch_create_task(int num, struct task *tsk) 
{
	struct i386_task *tinf = (struct i386_task *)CONT_TSK_ARCH_PTR(tsk);
	
	tinf->pdb = NULL;

	build_ldt(tinf, num, tsk->mem_adr, tsk->size, tsk->priv_level);
	
	if(first_task == NULL)
	{
		first_task = tinf;
		tinf->next = tinf->prev = NULL;
	}
	else
	{
		first_task->prev = tinf;
		tinf->next = first_task;
		tinf->prev = NULL;
		first_task = tinf;
	}

	return SUCCESS;
}

int arch_destroy_task(int task_num) 
{
	struct i386_task *tinf = GET_TASK_ARCH(task_num);
    
	if(first_task == tinf)
	{
		first_task = first_task->next;
		first_task->prev = NULL;
	}
	else
	{
		if(tinf->next != NULL) 
			tinf->next->prev = tinf->prev;
		if(tinf->prev != NULL) 
			tinf->prev->next = tinf->next;
	}

	return SUCCESS;
}
