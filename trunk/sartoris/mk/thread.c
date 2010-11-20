/*  
 *   Sartoris microkernel arch-neutral thread subsystem
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */

#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/salloc.h"
#include "lib/containers.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>
#include "sartoris/kernel-data.h"

/* We need need this for gcc to compile */
void *memcpy( void *to, const void *from, int count )
{
	int i = 0;
	while(i < count){
		((unsigned char *)to)[i] = ((unsigned char *)from)[i];
		i++;
	}

	return to;
}

/* multi-threding implementation */
int create_thread(int id, struct thread *thr) 
{
	struct thread cached_thr, *thread;
	struct task *task;
	int tsk_id, x;
	int result;  

	result = FAILURE;

	x = mk_enter(); /* enter critical block */ 

	if (VALIDATE_PTR(thr)) 
	{
		thr = (struct thread *)MAKE_KRN_PTR(thr);
		cached_thr = *thr;
		tsk_id = cached_thr.task_num;

		if ((0 <= id) && (id < MAX_THR) && !TST_PTR(id,thr)) 
		{
			if ((0 <= cached_thr.invoke_mode) && (cached_thr.invoke_mode <= MAX_USAGE_MODE)) 
			{
				if(TST_PTR(tsk_id,tsk))
				{
					task = GET_PTR(tsk_id,tsk);
										
					if (0 <= tsk_id && tsk_id < MAX_TSK && task->state == ALIVE) 
					{
						thread = (struct thread*)salloc(id, SALLOC_THR);

						if(thread != NULL)
						{
							result = SUCCESS;

							thread->ep = cached_thr.ep;
							thread->stack = cached_thr.stack;
							thread->invoke_mode = cached_thr.invoke_mode;
							thread->invoke_level = cached_thr.invoke_level;
							thread->task_num = tsk_id;
                            thread->run_perms = NULL;
							task->thread_count++;

							if (arch_create_thread(id, task->priv_level, thread) < 0) 
							{
								task->thread_count--;
								/* 
									This could break atomicity if a dynamic memory
									page is returned.
								*/
								sfree(thread, id, SALLOC_THR);
								result = FAILURE;
							}
						}
					}
				}
			}
		}
	}

	mk_leave(x); /* exit critical block */

	return result;
}

int destroy_thread(int id) 
{
	int i, x;
	int result;
	struct task *task;
	struct thread *thread;

	result = FAILURE;

	x = mk_enter(); /* enter critical block */

	if (0 <= id && id < MAX_THR && TST_PTR(id,thr)) 
	{
		if (curr_thread != id) 
		{
			result = SUCCESS;

			thread = GET_PTR(id,thr);
			task = GET_PTR(thread->task_num,tsk);

			task->thread_count--;
			
			arch_destroy_thread(id, thread);
			
			sfree(thread, id, SALLOC_THR);
		}
	}

	mk_leave(x); /* exit critical block */

	return result;
}

int run_thread(int id) 
{
	int x, res;
	int prev_curr_thread;
	int result;
	struct thread *thread;
	struct task *task;
    unsigned int *perms;

	result = FAILURE;

	x = mk_enter(); /* enter critical block */

	if( dyn_pg_lvl != DYN_PGLVL_NONE && dyn_pg_nest == DYN_NEST_ALLOCATING && dyn_pg_thread == id )
	{
        kprintf(0x7, "run_thread: Cannot run thread while allocation is taking place.");
		mk_leave(x);
		/* we cannot return to this thread... it's waiting for a page. */
		return FAILURE;
	}

	if (0 <= id && id < MAX_THR && TST_PTR(id,thr))
	{
		thread = GET_PTR(id,thr);
        
        // if invoke move is PERM_REQ, and the user loaded a bitmap for permissions
        // check for permissions.
        // NOTE: this could produce a page fault because it's a user space pointer
        if(thread->invoke_mode == PERM_REQ && thread->run_perms != NULL)
        {   
            /*
                thread->run_perms is on the desination thread address space. We must ask
                the arch dependant part of the kernel to map it to our thread mapping zone!!!
            */
#ifdef PAGING
            perms = (unsigned int*)map_address(thread->task_num, (unsigned int*)thread->run_perms + 1);
#else
            perms = ((unsigned int*)thread->run_perms + 1);
#endif
            // we could get a page fault on getbit
            if(thread->run_perms->length >= BITMAP_SIZE(curr_thread) && getbit(perms, curr_thread) )
            {
                mk_leave(x);
		        return FAILURE;
            }
        }
        
        if (thread->invoke_mode != DISABLED) 
		{
			if( curr_priv <= thread->invoke_level || thread->invoke_mode == UNRESTRICTED )
			{
				if (id != curr_thread) 
				{
					prev_curr_thread = curr_thread;
					task = GET_PTR(thread->task_num,tsk);

					curr_thread = id;
					curr_task = thread->task_num;
					curr_base = task->mem_adr;
					curr_priv = task->priv_level;
				
					result = arch_run_thread(id);

					if ( result !=  SUCCESS ) 
					{
						thread = GET_PTR(prev_curr_thread,thr);
						task = GET_PTR(thread->task_num,tsk);

						/* rollback: thread switching failed! */
						curr_thread = prev_curr_thread;
						curr_task = thread->task_num;
						curr_base = task->mem_adr;
						curr_priv = task->priv_level;
					}
				}
			}
		}
	}   

	mk_leave(x); /* exit critical block */

	return result;
}

/*
Software interrupt. This allows runing a thread within a given stack starting at a specific eip.
If stack is null current thread stack will be used.
Called thread must issue ret_from_int to return.
*/
int run_thread_int(int id, void *eip, void *stack)
{
	int x, res;
	int prev_curr_thread;
	int result;
	struct thread *thread;
	struct task *task;
    unsigned int *perms;

	result = FAILURE;

	x = mk_enter(); /* enter critical block */

	if ((0 <= id) && (id < MAX_THR) && TST_PTR(id,thr)) 
	{
		thread = GET_PTR(id,thr);

        // if invoke move is PERM_REQ, and the user loaded a bitmap for permissions
        // check for permissions.
        // NOTE: this could produce a page fault because it's a user space pointer
        if(thread->invoke_mode == PERM_REQ && thread->run_perms != NULL)
        {
            /*
                thread->run_perms is on the desination thread address space. We must ask
                the arch dependant part of the kernel to map it to our thread mapping zone!!!
            */
#ifdef PAGING
            perms = (unsigned int*)map_address(thread->task_num, (unsigned int*)thread->run_perms + 1);
#else
            perms = ((unsigned int*)thread->run_perms + 1);
#endif
            // we could get a page fault on getbit
            if(thread->run_perms->length >= BITMAP_SIZE(curr_thread) && getbit(((unsigned int*)thread->run_perms + 1), curr_thread) )
            {
                mk_leave(x);
		        return FAILURE;
            }
        }
        
	    if( dyn_pg_lvl != DYN_PGLVL_NONE && dyn_pg_nest == DYN_NEST_ALLOCATING && dyn_pg_thread == id )
	    {
		    mk_leave(x);
		    /* we cannot return to this thread... it's waiting for a page. */
		    return FAILURE;
	    }

        // Test the thread pointer is valid again (because of the possible PG Fault.
		if (TST_PTR(id,thr) && thread->invoke_mode != DISABLED) 
		{
			if(curr_priv <= thread->invoke_level || thread->invoke_mode == UNRESTRICTED) 
			{
				if (id != curr_thread) 
				{
					prev_curr_thread = curr_thread;
					task = GET_PTR(thread->task_num,tsk);

					curr_thread = id;
					curr_task = thread->task_num;
					curr_base = task->mem_adr;
					curr_priv = task->priv_level;

					result = arch_run_thread_int(id, eip, stack);

					if ( result !=  SUCCESS ) 
					{
						thread = GET_PTR(prev_curr_thread,thr);
						task = GET_PTR(thread->task_num,tsk);

						/* rollback: thread switching failed! */
						curr_thread = prev_curr_thread;
						curr_task = thread->task_num;
						curr_base = task->mem_adr;
						curr_priv = task->priv_level;
					}
				}
			}
		}
	}   

	mk_leave(x); /* exit critical block */

	return result;
}

int set_thread_run_perms(struct thread_perms *perms)
{
    struct thread *thread;
	int x, result = FAILURE;
    unsigned int len;
	
    if(VALIDATE_PTR(perms) && VALIDATE_PTR((unsigned int)perms + 4))
    {
        // this could produce a page fault..
        len = perms->length;
    }
    else
    {
        return FAILURE;
    }

    x = mk_enter();

    if(len <= BITMAP_SIZE(MAX_THR) && VALIDATE_PTR((unsigned int)perms + sizeof(unsigned int) + len))
    {
        thread = GET_PTR(curr_thread,thr);
        
        thread->run_perms = perms;

	    result = SUCCESS;
    }
    
	mk_leave(x);
	
	return result;
}

int set_thread_run_mode(int priv, enum usage_mode mode) 
{
	int x, result;
	struct thread *thread;

	x = mk_enter();

	if (0 <= mode && mode <= MAX_USAGE_MODE) 
	{
		thread = GET_PTR(curr_thread,thr);
		thread->invoke_level = priv;
		thread->invoke_mode = mode;
		result = SUCCESS;
	}
	else 
	{
		result = FAILURE;
	}

	mk_leave(x);

	return result;
}

int get_current_thread(void) 
{
    return curr_thread;
}

