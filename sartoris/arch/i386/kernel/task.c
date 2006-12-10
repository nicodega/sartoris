/*  
 *   Sartoris microkernel i386 task-support functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar      
 */

#include "sartoris/cpu-arch.h"
#include "kernel-arch.h"
#include "descriptor.h"
#include "lib/containers.h"
#include "i386.h"

int arch_create_task(int num, struct task *tsk) 
{
	struct i386_task *tinf = (struct i386_task *)CONT_TSK_ARCH_PTR(tsk);
	
	tinf->pdb = 0;

	build_ldt(tinf, num, tsk->mem_adr, tsk->size, tsk->priv_level);
	
	return 0;
}

int arch_destroy_task(int task_num) 
{
	inv_ldt_desc(task_num);
	return 0;
}
