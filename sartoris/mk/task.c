/*  
 *   Sartoris microkernel arch-neutral task subsystem
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@cvtci.com.ar      
 */


#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/scr-print.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/indexing.h"
#include "lib/salloc.h"

#include "sartoris/kernel-data.h"

/* multi-tasking implementation */
int create_task(int id, struct task *tsk, int *initm, unsigned int init_size) 
{
	char *dst2, *src2;
	int *dst;
	void *cached_mem_adr;
	unsigned int cached_size, i;
	int cached_priv_level, x;             /* mutex variable */
	int result, initm_result;
	struct task *task;

#ifdef COMPAT_REL53
	int j;	           /* temporary! */
#endif

	result = FAILURE;
	initm_result = FAILURE;  /* memory copy: failure */

	x = arch_cli();  /* enter critical block */

	if (VALIDATE_PTR(tsk)) 
	{
		tsk = (struct task *) MAKE_KRN_PTR(tsk);
		cached_mem_adr = tsk->mem_adr;        /* if we will pagefault, we must do it now */
		cached_size = tsk->size;              /* (before the sanity checks) */
		cached_priv_level = tsk->priv_level;

		/* no page fault may occur after this point,
		the rest is truly atomic */

		if (0 <= id && id < MAX_TSK && !TST_PTR(id,tsk)) 
		{
			/* check if the memory initializing is allright */
			if (initm != NO_TASK_INIT) 
			{    						// June 30 2005: I've changed this check to NO_TASK_INIT (defined as 0xFFFFFFFF)
										// because NULL is 0. And the task might be cloning itself.
				if (VALIDATE_PTR(initm)) 
				{
					if (init_size >= 0 && init_size <= cached_size) 
					{
						if (init_size != 0 && VALIDATE_PTR(((unsigned int)initm) + init_size - 1) && 
							!SUMOVERFLOW(((unsigned int)initm), init_size - 1)) 
						{
							initm_result = SUCCESS;
						}
					}
				}
			}

			/* check if the received task struct is ok */
			if (cached_priv_level >= 0) 
			{
				/* allocate a task (might break atomicity when dynamic mem is implemente) */
				task = (struct task *)salloc(id, SALLOC_TSK);

				if(task != NULL)
				{
					task->state = LOADING;
					result = SUCCESS;
				}
			}
		}
	}    

	arch_sti(x); /* exit critical block */

	if (result == SUCCESS) 
	{
		/* initialize everything else outisde critical block:
		create_task() has not returned yet so it's ok if we
		are interrupted, and state==LOADING, hence nobody will
		touch anything. */

		task->mem_adr = cached_mem_adr;
		task->size = cached_size;
		task->priv_level = cached_priv_level;
		task->thread_count = 0;
		task->first_smo = NULL;
		task->smos = 0;

		/* set every port as closed */
		for (i = 0; i < MAX_TSK_OPEN_PORTS; i++) 
		{
			task->open_ports[i] = NULL;
		}

		/* initialize memory if requested */

		if (initm != NO_TASK_INIT && initm_result == SUCCESS) 
		{
			arch_cpy_to_task(id, (char*)initm, 0, init_size);
		}

#ifdef COMPAT_REL53
		/* magic: for sartoris 0.5.3 compatibility, open 10 ports :( */
		for (j = 0; j < 32; j++) 
		{
			task->open_ports[j] = create_port(task);

			if (task->open_ports[j] != NULL) 
			{
				task->open_ports[j]->mode = PRIV_LEVEL_ONLY;
				task->open_ports[j]->priv = 3;				
			}
		}
#endif

		if (arch_create_task(id, CONT_TSK_ARCH_PTR(task)) == 0) 
		{
			/* the task is officially alive, other syscalls should
			operate on it, therefore: */
			task->state = ALIVE;
		}
		else 
		{
			/* if arch_create fails, rollback everything */
			/* no need to delete_task_smos, there cannot be any */

			delete_task_ports(task); /* <-- I think that only makes sense if    */
									/*      they were opened automatically for */
									/*      compatibility reasons              */

			sfree(task, id, SALLOC_TSK);
		
			result = FAILURE;                        
		}

	}

	return result;
}





int destroy_task(int id) 
{ 
	struct task *tsk;
	int x;
	int result;
  
	result = FAILURE;

	x = arch_cli(); /* enter critical block */

	/* check everything */
	if (0 <= id && id < MAX_TSK && TST_PTR(id,tsk)) 
	{
		tsk = (struct task *)GET_PTR(id,tsk);

		if (tsk->thread_count == 0 && curr_task != id && tsk->state == ALIVE) 
		{
			result = SUCCESS;
			tsk->state = UNLOADING;
		}
	}

	arch_sti(x); /* exit critical block */

	if (result == SUCCESS) 
	{
		result = arch_destroy_task(id); /* if cannot destroy, set result accordingly */

		if (result == SUCCESS) 
		{
			delete_task_smo(id);
			delete_task_ports(tsk);
			
			sfree(tsk, id, SALLOC_TSK);
		}		
	}

	return result;
}

int get_current_task(void) 
{
	return curr_task;
}
