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
#include "lib/indexing.h"
#include "sartoris/error.h"
#include <sartoris/critical-section.h>

/*
Thread tracing system calls.
Since many things depends on the architecture, register values will
be passed as a void pointer.
*/

// Begin tracing the given thread, from the specified task.
int ttrace_begin(int thr_id, int task_id)
{
    struct thread *thread;
    struct task *task;
    int ret = FAILURE;
    int x;

    mk_enter(x);
    // thread exists
    if(TST_PTR(thr_id,thr) && TST_PTR(task_id,tsk))
    {
        thread = GET_PTR(thr_id, thr);
        task = GET_PTR(task_id, tsk);

        // it's not being traced and it's not from the same task
        if(thread->trace_task == -1 && task_id != thread->task_num)
        {
            ret = arch_ttrace_begin(thr_id);
            if(ret == SUCCESS)
                thread->trace_task = task_id;
            else
                set_error(SERR_INVALID_THR);
        }
        else
        {
            if(thread->trace_task != -1)
                set_error(SERR_ALREADY_TRACING);
            else
                set_error(SERR_INVALID_TSK);
        }
    }
    else
    {
        if(!TST_PTR(thr_id,thr))
            set_error(SERR_INVALID_THR);
        else
            set_error(SERR_INVALID_TSK);
    }
    mk_leave(x);

    return ret;
}

/*
End thread tracing from the given task. If the task_id is 
not the one performing the trace and it's not -1, this function will fail.
*/
int ttrace_end(int thr_id, int task_id)
{
    struct thread *thread;
    int ret = FAILURE;
    int x;

    mk_enter(x);
    // thread exists
    if(TST_PTR(thr_id,thr))
    {
        thread = GET_PTR(thr_id, thr);

        // it's being traced by the task
        if(thread->trace_task == task_id || (task_id == -1 && thread->trace_task != -1))
        {
            arch_ttrace_end(thr_id);
            thread->trace_task = -1;
            ret = SUCCESS;
        }
        else
        {
            set_error(SERR_NOT_TRACING);
        }
    }
    else
    {
        set_error(SERR_INVALID_THR);
    }
    mk_leave(x);

    return ret;
}

/*
Read/Write a register from the thread state, or all general registers.
*/
int ttrace_reg(int thr_id, int reg, void *value, int set)
{
    struct thread *thread;
    int ret = FAILURE;
    int x, s = arch_ttrace_reg_size(reg);
    unsigned char tst;

    if(s > 0 && VALIDATE_PTR(value) && VALIDATE_PTR((unsigned int)value + s))
    {
        // force a page fault if pages are not allocated
        tst = *((unsigned char*)value);
        tst = *((unsigned char*)((unsigned int)value + (s-1)));

        mk_enter(x);
        // thread exists
        if(TST_PTR(thr_id,thr))
        {
            thread = GET_PTR(thr_id, thr);

            // it's being traced by the task
            if(thread->trace_task == curr_task)
            {
                value = MAKE_KRN_PTR(value);
                if(reg == -1)
                {
                    if(set)
                        ret = arch_ttrace_set_regs(thr_id, value);
                    else
                        ret = arch_ttrace_get_regs(thr_id, value);
                }
                else
                {
                    if(set)
                        ret = arch_ttrace_set_reg(thr_id, reg, value);
                    else
                        ret = arch_ttrace_get_reg(thr_id, reg, value);
                }
            }
            else
            {
                set_error(SERR_NOT_TRACING);
            }
        }
        mk_leave(x);
    }
    else
    {
        if(s == 0)
            set_error(SERR_INVALID_REG);
        else if(!VALIDATE_PTR(value))
            set_error(SERR_INVALID_PTR);
        else
            set_error(SERR_INVALID_SIZE);
    }

    return ret;
}

/*
Perform a memory read on the specified thread task.
*/
int ttrace_mem_read(int thr_id, void *src, void *dst, int size)
{
    struct thread *thread;
    int ret = FAILURE;
    int x, bytes;
    unsigned int off = 0;
    
    if (VALIDATE_PTR(dst) && VALIDATE_PTR((unsigned int)dst + size)) 
	{
		x = mk_enter(); /* enter critical block */

        if(TST_PTR(thr_id,thr))
        {
            thread = GET_PTR(thr_id, thr);

            if(thread->trace_task == curr_task 
                && VALIDATE_PTR_TSK(thread->task_num,src) && VALIDATE_PTR_TSK(thread->task_num,(unsigned int)src + size))
            {
                dst = MAKE_KRN_PTR(dst);
                src = MAKE_KRN_PTR(src);

			    while (ret != SUCCESS)
			    {                            
                    int bytes;
					
				    /*
				    If we are using paging and the required page is not present, this function call 
				    does not guarantee atomicity. (It might rise a page fault interrupt). That's
				    why we must validate all things again.
				    */
				    bytes = arch_cpy_from_task(thread->task_num, (char*)((unsigned int)src+off), (char*)((unsigned int)dst+off), size, x);  
				
				    if (!TST_PTR(thr_id,thr) || thread->trace_task != curr_task )
				    {
                        if(!TST_PTR(thr_id,thr))
                            set_error(SERR_INVALID_THR);
                        else
                            set_error(SERR_NOT_TRACING);
					    mk_leave(x);
					    return FAILURE;
				    }
			
				    off += bytes;
				    size -= bytes;

				    if (size == 0) 
				    {
                        set_error(SERR_OK);
					    ret = SUCCESS; 
				    }
			    }
            }
            else
            {
                if(thread->trace_task != curr_task)
                    set_error(SERR_NOT_TRACING);
                else if(!VALIDATE_PTR_TSK(thread->task_num,src))
                    set_error(SERR_INVALID_PTR);
                else
                    set_error(SERR_INVALID_SIZE);
            }
		}
        else
        {
            set_error(SERR_INVALID_THR);
        }
      
		mk_leave(x); /* exit critical block */
    }
    else
    {
        if(!VALIDATE_PTR(dst))
            set_error(SERR_INVALID_PTR);
        else
            set_error(SERR_INVALID_SIZE);
    }

    return ret;
}


/*
Perform a memory read on the specified thread task.
*/
int ttrace_mem_write(int thr_id, void *src, void *dst, int size)
{
    struct thread *thread;
    int ret = FAILURE;
    int x, bytes;
    unsigned int off = 0;
    
    if (VALIDATE_PTR(src) && VALIDATE_PTR((unsigned int)src + size)) 
	{
		x = mk_enter(); /* enter critical block */

        if(TST_PTR(thr_id,thr))
        {
            thread = GET_PTR(thr_id, thr);

            if(thread->trace_task == curr_task 
                && VALIDATE_PTR_TSK(thread->task_num,dst) && VALIDATE_PTR_TSK(thread->task_num,(unsigned int)dst + size))
            {
                src = MAKE_KRN_PTR(src);
                dst = MAKE_KRN_PTR(dst);

			    while (ret != SUCCESS)
			    {                            
                    int bytes;
					
				    /*
				    If we are using paging and the required page is not present, this function call 
				    does not guarantee atomicity. (It might rise a page fault interrupt). That's
				    why we must validate all things again.
				    */
				    bytes = arch_cpy_to_task(thread->task_num, (char*)((unsigned int)src+off), (char*)((unsigned int)dst+off), size, x);  
				
				    if (!TST_PTR(thr_id,thr) || thread->trace_task != curr_task )
				    {
                        if(!TST_PTR(thr_id,thr))
                            set_error(SERR_INVALID_THR);
                        else
                            set_error(SERR_NOT_TRACING);
					    mk_leave(x);
					    return FAILURE;
				    }
			
				    off += bytes;
				    size -= bytes;

				    if (size == 0) 
				    {
                        set_error(SERR_OK);
					    ret = SUCCESS; 
				    }
			    }
            }
            else
            {
                if(thread->trace_task != curr_task)
                    set_error(SERR_NOT_TRACING);
                else if(!VALIDATE_PTR_TSK(thread->task_num,dst))
                    set_error(SERR_INVALID_PTR);
                else
                    set_error(SERR_INVALID_SIZE);
            }
		}
        else
        {
            set_error(SERR_INVALID_THR);
        }
      
		mk_leave(x); /* exit critical block */
    }
    else
    {
        if(!VALIDATE_PTR(src))
            set_error(SERR_INVALID_PTR);
        else
            set_error(SERR_INVALID_SIZE);
    }

    return ret;
}
