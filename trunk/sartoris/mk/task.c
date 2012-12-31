/*  
 *   Sartoris microkernel arch-neutral task subsystem
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */


#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/metrics.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/indexing.h"
#include "lib/salloc.h"
#include "sartoris/error.h"
#include "sartoris/kernel-data.h"
#include <sartoris/critical-section.h>
#include <sartoris/syscall.h>

#ifndef ARCH_FUNC_ATT2
asd
#endif 

int counter = 0;
/* multi-tasking implementation */
int ARCH_FUNC_ATT2 create_task(int id, struct task *tsk) 
{
	void *cached_mem_adr;
	unsigned int cached_size, i;
	int cached_priv_level;
	int x;             /* mutex variable */
	int result;
	struct task *task;
    
    counter++;
    	
	result = FAILURE;

	x = mk_enter();  /* enter critical block */

	if (VALIDATE_PTR(tsk)) 
	{        
        tsk = (struct task *)MAKE_KRN_PTR(tsk);
		cached_mem_adr = tsk->mem_adr;        /* if we will pagefault, we must do it now */
		cached_size = tsk->size;              /* (before the sanity checks) */
		cached_priv_level = tsk->priv_level;

		/* no page fault may occur after this point,
		the rest is truly atomic */
		if (0 <= id && id < MAX_TSK && !TST_PTR(id,tsk) && (unsigned int)cached_mem_adr >= USER_OFFSET) 
		{
			/* check if the received task struct is ok */
			if (cached_priv_level >= 0) 
			{
				/* allocate a task (might break atomicity) */
				task = (struct task *)salloc(id, SALLOC_TSK);

				if(task != NULL)
				{
					task->state = LOADING;
					result = SUCCESS;
				}
                else
                {
                    set_error(SERR_NO_MEM);
                }
			}
            else
            {
                set_error(SERR_INVALID_PRIV);
            }
		}
        else
        {
            if(0 > id || id >= MAX_TSK)
                set_error(SERR_INVALID_ID);
            else if(TST_PTR(id,tsk))
                set_error(SERR_INVALID_TSK);
            else
                set_error(SERR_INVALID_BASE);
        }
	}
    else
    {
        set_error(SERR_INVALID_PTR);
    }

	mk_leave(x); /* exit critical block */

	if (result == SUCCESS) 
	{
		/* 
			Initialize everything else outisde critical block:
			create_task() has not returned yet so it's ok if we
			are interrupted, and state==LOADING, hence nobody will
			touch anything. 
		*/

		task->mem_adr = cached_mem_adr;
		task->size = cached_size;
		task->priv_level = cached_priv_level;
		task->thread_count = 0;
		task->first_smo = NULL;
		task->smos = 0;
        task->evts = 0;
		
		/* set every port as closed/non-existant */
		for (i = 0; i < MAX_TSK_OPEN_PORTS; i++) 
		{
			task->open_ports[i] = NULL;
		}
					
		if (arch_create_task(id, task) == 0) 
		{
			/* the task is officially alive, other syscalls should
			operate on it, therefore: */
			task->state = ALIVE;

#ifdef METRICS
            metrics.tasks++;
#endif
            set_error(SERR_OK);
		}
		else 
		{	
			/* if arch_create fails, rollback everything */
			/* 
				No need to delete_task_smos/delete_task_ports, 
			    there cannot be any 
			*/
            set_error(SERR_ERROR);

			sfree(task, id, SALLOC_TSK);
		
			result = FAILURE;
		}
	}
	
	return result;
}

int ARCH_FUNC_ATT3 init_task(int task, int *start, unsigned int size)
{
	struct task *stask = NULL;
	int x = mk_enter(), result = FAILURE;
	
	/*
	NEW: We MUST check task is created and alive.
	*/
	if((0 <= task) && (task < MAX_TSK) && TST_PTR(task,tsk))	
	{
		stask = GET_PTR(task,tsk);
		
		if(stask->state == ALIVE)
		{
			/* check if memory initializing is allright */
			if (VALIDATE_PTR(start)) 
			{
				if (size > 0 && size <= stask->size) 
				{
					if (VALIDATE_PTR(((unsigned int)start) + size - 1) && 
						!SUMOVERFLOW(((unsigned int)start), size - 1)) 
					{   
						result = SUCCESS;
						stask->state = LOADING;
					}
                    else
                    {
                        set_error(SERR_INVALID_SIZE);
                    }
				}
                else
                {
                    set_error(SERR_INVALID_SIZE);
                }
			}
            else
            {
                set_error(SERR_INVALID_PTR);
            }
		}
        else
        {
            set_error(SERR_INVALID_TSK);
        }
	}
    else
    {
        if(0 > task || task >= MAX_TSK)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_TSK);
    }

	mk_leave(x);

	if (result == SUCCESS) 
	{
        unsigned int dest = 0;
        while(size > 0)
        {
            int bytes;
		    bytes = arch_cpy_to_task(task, (char*)start, (char*)dest, size, x, 0);

            if(bytes < 0)
            {
                result == FAILURE;
                set_error(SERR_NO_PERMISSION);
                break;
            }

            start += bytes;
			size -= bytes;
            dest += bytes;
        }

        if (result == SUCCESS) 
        {
            stask->state = ALIVE;
            set_error(SERR_OK);
        }
	}
	
	return result;
}

int ARCH_FUNC_ATT1 destroy_task(int id) 
{ 
	struct task *tsk;
	int x;
	int result;
  
	result = FAILURE;

	x = mk_enter(); /* enter critical block */
    
    /* check everything */
	if (0 <= id && id < MAX_TSK && TST_PTR(id,tsk)) 
	{        
		tsk = GET_PTR(id,tsk);
        
		if (tsk->thread_count == 0 && curr_task != id && tsk->state == ALIVE) 
		{
			result = SUCCESS;
			tsk->state = UNLOADING;
		}
        else
        {
            if(curr_task == id)
                set_error(SERR_SAME_TASK);
            else
                set_error(SERR_INVALID_TSK);
        }
	}
    else
    {
        if(0 > id || id >= MAX_TSK)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_TSK);
    }
    
	mk_leave(x); /* exit critical block */

	if (result == SUCCESS) 
	{
		result = arch_destroy_task(id); /* if cannot destroy, set result accordingly */
        
		if (result == SUCCESS) 
		{
			delete_task_smo(id);
			delete_task_ports(tsk);
#ifdef METRICS
            metrics.tasks--;
#endif			
			sfree(tsk, id, SALLOC_TSK);
            set_error(SERR_OK);
		}		
	}

	return result;
}

int ARCH_FUNC_ATT0 get_current_task() 
{
    set_error(SERR_OK);
	return curr_task;
}
