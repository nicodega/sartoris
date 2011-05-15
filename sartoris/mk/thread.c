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
#include "sartoris/metrics.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/salloc.h"
#include "lib/containers.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>
#include "sartoris/kernel-data.h"
#include "sartoris/error.h"
#include "sartoris/permissions.h"

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

		if (0 <= id && id < MAX_THR && !TST_PTR(id,thr)) 
		{
			if (0 <= cached_thr.invoke_mode && cached_thr.invoke_mode <= MAX_USAGE_MODE)
			{
				if(0 <= tsk_id && tsk_id < MAX_TSK && TST_PTR(tsk_id,tsk))
				{
					task = GET_PTR(tsk_id,tsk);
			
					if (task->state == ALIVE) 
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
                            thread->last_error = SERR_OK;
                            task->thread_count++;

                            init_perms(&thread->run_perms);

							if (arch_create_thread(id, task->priv_level, thread) < 0) 
							{
								task->thread_count--;
								/* 
									This could break atomicity if a dynamic memory
									page is returned.
								*/
								sfree(thread, id, SALLOC_THR);
								result = FAILURE;

                                set_error(SERR_ERROR);
							}
                            else
                            {
#ifdef METRICS
                                metrics.threads++;
#endif                    
                                set_error(SERR_OK);
                            }
						}
                        else
                        {
                            set_error(SERR_NO_MEM);
                        }
					}
                    else
                    {
                        set_error(SERR_INVALID_TSK);
                    }
				}
                else
                {
                    set_error(SERR_INVALID_TSK);
                }
			}
            else
            {
                set_error(SERR_INVALID_MODE);
            }
		}
        else
        {
            set_error(SERR_INVALID_ID);
        }
	}
    else
    {
        set_error(SERR_INVALID_PTR);
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
#ifdef METRICS
            metrics.threads--;
#endif
            set_error(SERR_OK);
		}
        else
        {
            set_error(SERR_SAME_THREAD);
        }
	}
    else
    {
        if(0 > id || id >= MAX_THR)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_THR);
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
    	
    if (0 <= id && id < MAX_THR && TST_PTR(id,thr))
	{
        thread = GET_PTR(id,thr);
        
        // NOTE: test_permission could produce a page fault
        if(curr_priv != 0 
            && thread->invoke_mode == PERM_REQ 
            && test_permission(thread->task_num, &thread->run_perms, curr_thread) == FAILURE)
        {
            mk_leave(x);
		    return FAILURE;
        }
        
        if (thread->invoke_mode != DISABLED) 
		{            
            if( curr_priv <= thread->invoke_level 
                || thread->invoke_mode == UNRESTRICTED 
                || thread->invoke_mode == PERM_REQ)
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
                        set_error(SERR_ERROR);
						thread = GET_PTR(prev_curr_thread,thr);
						task = GET_PTR(thread->task_num,tsk);

						/* rollback: thread switching failed! */
						curr_thread = prev_curr_thread;
						curr_task = thread->task_num;
						curr_base = task->mem_adr;
						curr_priv = task->priv_level;
					}
                    else
                    {
                        set_error(SERR_OK);
                    }
				}
                else
                {
                    set_error(SERR_SAME_THREAD);
                }
			}
            else
            {
                set_error(SERR_NO_PERMISSION);
            }
		}
        else
        {
            set_error(SERR_INVALID_THR);
        }
    }
    else
    {
        if(0 > id || id >= MAX_THR)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_THR);
    }

	mk_leave(x); /* exit critical block */

	return result;
}


/*
Software interrupt. This allows runing a thread within a given stack starting at a specific eip.
If stack is null current thread stack will be used.
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
    	
    if (0 <= id && id < MAX_THR && TST_PTR(id,thr))
	{
        thread = GET_PTR(id,thr);
        
        // NOTE: test_permission could produce a page fault
        if(curr_priv != 0 
            && thread->invoke_mode == PERM_REQ 
            && test_permission(thread->task_num, &thread->run_perms, curr_thread) == FAILURE)
        {
            mk_leave(x);
		    return FAILURE;
        }
        
        if (thread->invoke_mode != DISABLED) 
		{            
            if( curr_priv <= thread->invoke_level 
                || thread->invoke_mode == UNRESTRICTED 
                || thread->invoke_mode == PERM_REQ)
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
                        set_error(SERR_ERROR);
						thread = GET_PTR(prev_curr_thread,thr);
						task = GET_PTR(thread->task_num,tsk);

						/* rollback: thread switching failed! */
						curr_thread = prev_curr_thread;
						curr_task = thread->task_num;
						curr_base = task->mem_adr;
						curr_priv = task->priv_level;
					}
                    else
                    {
                        set_error(SERR_OK);
                    }
				}
                else
                {
                    set_error(SERR_SAME_THREAD);
                }
			}
            else
            {
                set_error(SERR_NO_PERMISSION);
            }
		}
        else
        {
            set_error(SERR_INVALID_THR);
        }
    }
    else
    {
        if(0 > id || id >= MAX_THR)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_THR);
    }

	mk_leave(x); /* exit critical block */

	return result;
}

int set_thread_run_perms(int thr_id, struct permissions *perms)
{
    struct thread *thread;
    struct permissions prm;
	int x, result = FAILURE;
    
    x = mk_enter();

    if (0 <= thr_id 
        && thr_id < MAX_THR 
        && TST_PTR(thr_id,thr)) 
    {
        if(0 > thr_id || thr_id >= MAX_THR)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_THR);
        mk_leave(x);
        return FAILURE;
    }

    if (thr_id == curr_thread
        && curr_priv > 0) 
    {
        set_error(SERR_INVALID_THR);
        mk_leave(x);
        return FAILURE;
    }

    thread = GET_PTR(thr_id,thr);
        
    if(validate_perms_ptr(perms, &prm, MAX_THR, thread->task_num))
    {   
        // Validate the thread still exists
        if(TST_PTR(thr_id,thr))
        {
            thread->run_perms = prm;
	        result = SUCCESS;
            set_error(SERR_OK);
        }
        else
        {
            init_perms(&thread->run_perms);
            set_error(SERR_INVALID_THR);
            result = FAILURE;
        }
    }
    else
    {
        set_error(SERR_INVALID_SIZE);
    }
    
	mk_leave(x);
	
	return result;
}

int set_thread_run_mode(int thr_id, int priv, enum usage_mode mode) 
{
	int x, result;
	struct thread *thread;

	x = mk_enter();

    if (0 <= thr_id && thr_id < MAX_THR && TST_PTR(thr_id,thr)) 
    {
        if(0 > thr_id || thr_id >= MAX_THR)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_THR);
        mk_leave(x);
        return FAILURE;
    }

    if (thr_id == curr_thread && curr_priv > 0 && mode == PERM_REQ) 
    {
        set_error(SERR_INVALID_THR);
        mk_leave(x);
        return FAILURE;
    }

	if (0 <= mode && mode <= MAX_USAGE_MODE) 
	{
		thread = GET_PTR(curr_thread,thr);
		thread->invoke_level = priv;
		thread->invoke_mode = mode;
		result = SUCCESS;
        set_error(SERR_OK);
	}
	else 
	{
        set_error(SERR_INVALID_MODE);
		result = FAILURE;
	}

	mk_leave(x);

	return result;
}

int get_current_thread(void) 
{
    set_error(SERR_OK);
    return curr_thread;
}

