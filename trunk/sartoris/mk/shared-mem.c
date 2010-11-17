
/*  
 *   Sartoris shared memory object management 
 *   system calls & algorithms implementation
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */

#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/scr-print.h"
#include "sartoris/metrics.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "sartoris/kernel-data.h"
#include "lib/salloc.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>


/* shared memory subsystem implementation */

/* these are the proper system calls: */
int share_mem(int target_task, int addr, int size, int rw) 
{
    int x, result;
    
    result = FAILURE;
    
    if (0 <= target_task && target_task < MAX_TSK) 
	{
		x = mk_enter(); /* enter critical block */
      
		if (TST_PTR(target_task,tsk) && GET_PTR(target_task,tsk)->state == ALIVE && GET_PTR(curr_task,tsk)->smos < MAX_TSK_SMO) 
		{
			if (VALIDATE_PTR(addr) && VALIDATE_PTR(addr + size - 1)) 
			{
				result = get_new_smo(curr_task, target_task, addr, size, rw);

				/* Atomicity might have been broken, test target task again */
				if(result != FAILURE && !(TST_PTR(target_task,tsk) && GET_PTR(target_task,tsk)->state == ALIVE))
				{
					delete_smo(result,curr_task);
					result = FAILURE;
				}
#ifdef _METRICS_
				if(result != FAILURE) metrics.smos++;
#endif
			}
		}

		mk_leave(x); /* exit critical block */
	}
    
    return result;
}

int claim_mem(int smo_id) 
{
    int result;
 
    result = FAILURE;
    
    /* no need to block here, smo_id is in my stack */
    if (0 <= smo_id && smo_id < MAX_SMO && TST_PTR(smo_id,smo)) 
	{
		/* the rest of the validation is in delete_smo */
		result = delete_smo(smo_id,curr_task);

#ifdef _METRICS_
		if(result == SUCCESS) metrics.smos--;
#endif
    }
    
    return result;
}

int read_mem(int smo_id, int off, int size, int *dest) 
{
    struct smo *my_smo;
    char *src;
    int x, result;
    
    result = FAILURE;

    if (0 <= smo_id && smo_id < MAX_SMO && off >= 0 && size >= 0) 
	{
		x = mk_enter(); /* enter critical block */
				
		if (TST_PTR(smo_id,smo) && VALIDATE_PTR(dest)) 
		{			
			my_smo = GET_PTR(smo_id,smo);

			while (my_smo->target == curr_task && off + size <= my_smo->len 
				   && (my_smo->rights & READ_PERM) && result != SUCCESS)
			{                            
                int bytes;
					
				src = (char *) ((unsigned int)my_smo->base + off);
				/*
				If we are using paging and the required page is not present, this function call 
				does not guarantee atomicity. (It might rise a page fault interrupt). That's
				why we must validate all things again.
				*/
				bytes = arch_cpy_from_task(my_smo->owner, (char*)src, (char*)dest, size, x);  
				
				if (!TST_PTR(smo_id,smo))
				{					
					mk_leave(x);
					return FAILURE;
				}
			
				off += bytes;
				size -= bytes;

				if (size == 0) 
				{
					result = SUCCESS; 
				}
			}
		}
      
		mk_leave(x); /* exit critical block */
    }
    
    return result;
}

int write_mem(int smo_id, int off, int size, int *src) 
{
    struct smo *my_smo;
    char *dest;
    int x, result;
    
    result = FAILURE;
    
    if (0 <= smo_id && smo_id < MAX_SMO && off >= 0 && size >= 0) 
	{
		x = mk_enter(); /* enter critical block */

		if (TST_PTR(smo_id,smo) && VALIDATE_PTR(src)) 
		{
			my_smo = GET_PTR(smo_id,smo);

			while (my_smo->target == curr_task && off + size <= my_smo->len 
				   && (my_smo->rights & WRITE_PERM) && result != SUCCESS)                   
			{                      
                int bytes;

				dest = (char *) ((unsigned int)my_smo->base + off);
				bytes = arch_cpy_to_task(my_smo->owner, (char*)src, (char*)dest, size, x); 

				if (!(0 <= smo_id && smo_id < MAX_SMO && off >= 0 && size >= 0 && TST_PTR(smo_id,smo) 
					&& my_smo->target == curr_task && off + size <= my_smo->len 
				    && (my_smo->rights & READ_PERM)))
				{
					mk_leave(x);
					return FAILURE;
				}

				off += bytes;
				size -= bytes;

				if (size == 0) 
				{
					result = SUCCESS;
				}
			}
		}

		mk_leave(x); /* exit critical block */
	}

    return result;
}

int pass_mem(int smo_id, int target_task) 
{
    struct smo *smo;
    int x, result;
    
    result = FAILURE;
    
    if (0 <= smo_id && smo_id < MAX_SMO && TST_PTR(smo_id,smo) && 0 <= target_task && target_task < MAX_TSK) 
	{
		x = mk_enter(); /* enter critical block */
       
		smo = GET_PTR(smo_id,smo);

		if(smo->target == curr_task) 
		{
			smo->target = target_task;
			result = SUCCESS;
		}

		mk_leave(x); /* exit critical block */
    }
    
    return result;
}

int mem_size(int smo_id) 
{
	struct smo *smo;
	int result;

	result = -1;

	if (0 <= smo_id && smo_id < MAX_SMO && TST_PTR(smo_id,smo)) 
	{
		smo = GET_PTR(smo_id,smo);

		if (smo->target == curr_task) 
		{
			result = smo->len;
		}
	}

	return result;
}

/* the following functions implement the data structures used above: */
/* this function is called within a critical block, but might break atomicity! */
int get_new_smo(int task_id, int target_task, int addr, int size, int perms) 
{
	struct smo *smo = NULL;
	struct task *task;
	int id;
   
	/* Get a free smo id */
	id = index_find_free(IDX_SMO);

	if(id == -1)
	{
		/* No free smo id's */
		return FAILURE;
	}

	/* allocate a new SMO. Atomicity CAN be broken HERE. */
	smo = (struct smo*)salloc(id, SALLOC_SMO);

    if(smo == NULL)  /* No free smos */
    {
		return FAILURE;
    }
	    
	task = GET_PTR(task_id,tsk);

	smo->next = task->first_smo;
	smo->prev = NULL;
	task->first_smo = smo;
	task->smos++;
	
	/* copy new SMO parameters */
	smo->id = id;
	smo->owner = task_id;
	smo->target = target_task;
	smo->base = addr;
	smo->len = size;
	smo->rights = perms;
	
    return id;
}

int delete_smo(int id, int task_id) 
{
    int x, result;
    struct smo *prev, *next, *smo;
	struct task *task;
		
    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

	smo = GET_PTR(id,smo);
    
    if (smo->owner == task_id) 
	{	
		result = SUCCESS;

		task = GET_PTR(task_id,tsk);

		prev = smo->prev;
		next = smo->next;

		if(task->first_smo == smo) 
		{
			task->first_smo = smo->next;
		}

		if (prev != NULL) 
			smo->prev->next = next;
		if (next != NULL)
			smo->next->prev = prev;
	
		sfree(smo, id, SALLOC_SMO);

		task->smos--;
    }

    mk_leave(x); /* exit critical block */
    
    return result;
}

void delete_task_smo(int task_id) 
{
	struct smo *smo;
	struct task *task = GET_PTR(task_id,tsk);
	int x;

    x = mk_enter(); /* enter critical block */

	while(task->first_smo != NULL)
	{
		smo = task->first_smo;

		task->first_smo = smo->next;

		sfree(smo, smo->id, SALLOC_SMO);		
	}

	task->smos = 0;
    
    mk_leave(x); /* exit critical block */
}
