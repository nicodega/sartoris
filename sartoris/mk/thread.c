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
#include "sartoris/scr-print.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/salloc.h"
#include "lib/containers.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>
#include "sartoris/kernel-data.h"

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
			
			// NOTE: Thread run permissions have been disabled 
			// until I figure a way of creating them only when used
			/*
			for (i = 0; i < (BITMAP_SIZE(MAX_THR)); i++) 
			{
				run_perms[id][i] = 0;
			}
			*/

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

	result = FAILURE;

	x = mk_enter(); /* enter critical block */

	if( dyn_pg_lvl != DYN_PGLVL_NONE && dyn_pg_nest == DYN_NEST_ALLOCATING && dyn_pg_thread == id )
	{
		mk_leave(x);
		/* we cannot return to this thread... it's waiting for a page. */
		return FAILURE;
	}

	if (0 <= id && id < MAX_THR && TST_PTR(id,thr))
	{
		thread = GET_PTR(id,thr);

		if (thread->invoke_mode != DISABLED) 
		{
			if(curr_priv <= thread->invoke_level) 
			{
				//if (!(thread->invoke_mode == PERM_REQ) && !getbit(run_perms[id], curr_thread))) 
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

	result = FAILURE;

	x = mk_enter(); /* enter critical block */

	if( dyn_pg_lvl != DYN_PGLVL_NONE && dyn_pg_nest == DYN_NEST_ALLOCATING && dyn_pg_thread == id )
	{
		mk_leave(x);
		/* we cannot return to this thread... it's waiting for a page. */
		return FAILURE;
	}

	if ((0 <= id) && (id < MAX_THR) && TST_PTR(id,thr)) 
	{
		thread = GET_PTR(id,thr);

		if (thread->invoke_mode != DISABLED) 
		{
			if(curr_priv <= thread->invoke_level) 
			{
				//if (!(thread->invoke_mode == PERM_REQ) && !getbit(run_perms[id], curr_thread))) 
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
	}   

	mk_leave(x); /* exit critical block */

	return result;
}

int set_thread_run_perm(int thread, enum bool perms) 
{
	int x, result = FAILURE;

	/*
	x = mk_enter();
  
	if (0 <= perms && perms <= 1) 
	{
		setbit(run_perms[curr_thread], thread, perms);
		result = SUCCESS;
	}
	
	mk_leave(x);
	*/

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

