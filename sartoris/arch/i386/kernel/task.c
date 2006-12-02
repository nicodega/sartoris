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

int arch_create_task(int num, struct task* tsk) 
{
	build_ldt(num, tsk->mem_adr, tsk->size, tsk->priv_level);
	init_ldt_desc(num, tsk->priv_level);
	
	return 0;
}

int arch_destroy_task(int task_num) 
{
	inv_ldt_desc(task_num);
	return 0;
}
