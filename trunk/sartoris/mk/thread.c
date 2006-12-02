/*  
 *   Sartoris microkernel arch-neutral thread subsystem
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

#include "sartoris/kernel-data.h"

/* multi-threding implementation */

int create_thread(int id, struct thread *thr) 
{
	struct thread cached_thr;
	int tsk_id, x;
	int result;  

	result = FAILURE;

	x = arch_cli(); /* enter critical block */ 

	if (VALIDATE_PTR(thr)) 
	{
		thr = (struct thread *) MAKE_KRN_PTR(thr);
		cached_thr = *thr;
		tsk_id = cached_thr.task_num;

		if ((0 <= id) && (id < MAX_THR) && (threads[id].task_num < 0)) 
		{
			if ((0 <= cached_thr.invoke_mode) && (cached_thr.invoke_mode <= MAX_USAGE_MODE)) 
			{
				if (0 <= tsk_id && tsk_id < MAX_TSK && tasks[tsk_id].state == ALIVE) 
				{
					result = SUCCESS;

					threads[id].ep = cached_thr.ep;
					threads[id].stack = cached_thr.stack;
					threads[id].invoke_mode = cached_thr.invoke_mode;
					threads[id].invoke_level = cached_thr.invoke_level;
					threads[id].task_num = tsk_id;
					tasks[tsk_id].thread_count++;

					if (arch_create_thread(id, tasks[tsk_id].priv_level, &threads[id]) < 0) 
					{
						threads[id].task_num = -1; /* marks the thread as non-present */
						tasks[tsk_id].thread_count--;
						result = FAILURE;
					}
				}
			}
		}
	}

	arch_sti(x); /* exit critical block */

	return result;
}

int destroy_thread(int id) 
{
	int i, x;
	int result;

	result = FAILURE;

	x = arch_cli(); /* enter critical block */

	if ((0 <= id) && (id < MAX_THR) & (threads[id].task_num >= 0)) 
	{
		if (curr_thread != id) 
		{
			result = SUCCESS;

			tasks[threads[id].task_num].thread_count--;
			threads[id].task_num = -1;

			for (i = 0; i < (BITMAP_SIZE(MAX_THR)); i++) 
			{
				run_perms[id][i] = 0;
			}

			arch_destroy_thread(id, &threads[id]);
		}
	}

	arch_sti(x); /* exit critical block */

	return result;
}

int run_thread(int id) 
{
	int x, res;
	int prev_curr_thread;
	int result;

	result = FAILURE;

	x = arch_cli(); /* enter critical block */

	if ((0 <= id) && (id < MAX_THR) && (threads[id].task_num >= 0)) 
	{
		if (threads[id].invoke_mode != DISABLED) 
		{
			if (curr_priv <= threads[id].invoke_level) 
			{
				if (!(threads[id].invoke_mode == PERM_REQ && !getbit(run_perms[id], curr_thread))) 
				{
					if (id != curr_thread) 
					{
						prev_curr_thread = curr_thread;

						curr_thread = id;
						curr_task = threads[id].task_num;
						curr_base = tasks[curr_task].mem_adr;
						curr_priv = tasks[curr_task].priv_level;

						result = arch_run_thread(id);

						if ( result !=  SUCCESS ) 
						{
							/* rollback: thread creation failed! */
							curr_thread = prev_curr_thread;
							curr_task = threads[curr_thread].task_num;
							curr_base = tasks[curr_task].mem_adr;
							curr_priv = tasks[curr_task].priv_level;
						}
					}
				}
			}
		}
	}   

	arch_sti(x); /* exit critical block */

	return result;
}

int set_thread_run_perm(int thread, enum bool perms) 
{
	int x, result;

	x = arch_cli();
  
	if (0 <= perms && perms <= 1) 
	{
		setbit(run_perms[curr_thread], thread, perms);
		result = SUCCESS;
	}
	else 
	{
		result = FAILURE;
	}
	
	arch_sti(x);

	return result;
}

int set_thread_run_mode(int priv, enum usage_mode mode) 
{
	int x, result;

	x = arch_cli();

	if (0 <= mode && mode <= MAX_USAGE_MODE) 
	{
		threads[curr_thread].invoke_level = priv;
		threads[curr_thread].invoke_mode = mode;
		result = SUCCESS;
	}
	else 
	{
		result = FAILURE;
	}

	arch_sti(x);

	return result;
}

int get_current_thread(void) 
{
    return curr_thread;
}

