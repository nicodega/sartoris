/*  
 *   Sartoris microkernel interrupt handling arch-neutral functions
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
#include "lib/indexing.h"
#include <sartoris/critical-section.h>
#include "sartoris/kernel-data.h"
#include "sartoris/error.h"

/* interrupt management implementation */

int create_int_handler(int number, int thread_id, int nesting, int priority) 
{
    int x;
    int result;	
    
    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */
        
    if (0 <= number && number < MAX_IRQ && 
        0 <= thread_id && thread_id < MAX_THR &&
        TST_PTR(thread_id,thr) && int_handlers[number] < 0) 
	{        
		if (arch_create_int_handler(number) == 0) 
		{
            set_error(SERR_OK);
            result = SUCCESS;
	
			if (nesting) 
				int_nesting[number] = 1;
			else
				int_nesting[number] = 0;
		
			int_handlers[number] = thread_id;
			int_active[thread_id] = 0;
		}
        else
        {
            set_error(SERR_ERROR);
        }
    }
    else
    {
        if(0 > number || number >= MAX_IRQ)
            set_error(SERR_INVALID_INTERRUPT);
        else if(int_handlers[number] >= 0)
            set_error(SERR_INTERRUPT_HANDLED);
        else
            set_error(SERR_INVALID_THR);
    }
        
    mk_leave(x); /* exit critical block */
        
    return result;
}

int destroy_int_handler(int number, int thread) 
{
    int x;
    int result;
    
    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

    if (number >= 0 && number < MAX_IRQ && int_handlers[number] == thread) 
	{
		if (arch_destroy_int_handler(number) == 0) 
		{
            set_error(SERR_OK);
			result = SUCCESS;
			int_handlers[number] = -1;
		}
        else
        {
            set_error(SERR_ERROR);
        }
    }
    else
    {
        if(0 > number || number >= MAX_IRQ)
            set_error(SERR_INVALID_INTERRUPT);
        else if(int_handlers[number] >= 0)
            set_error(SERR_INVALID_THR);
    }

    mk_leave(x); /* exit critical block */
    
    return result;
}

/* note: the following function is not invoked from userland, 
   only from the arch-kernel section */

/* atomicity is assumed */
void handle_int(int number) 
{
    int h;
	struct thread *thread;
	struct task *task;
    
#ifdef PAGING
    if (IS_PAGE_FAULT(number)) 
	{		
        kprintf(12, "\nmk/INTERRUPT.C: PF");
        thread = GET_PTR(curr_thread, thr);
        task = GET_PTR(thread->task_num,tsk);

		/* IS_PAGE_FAULT comes from kernel-arch.h */
		thread->page_faulted = 1;

		/*
		If dynamic memory request level is not NONE and it's not nested, or 
		we are in the middle of freeing a page.
		*/
		if(dyn_pg_lvl != DYN_PGLVL_NONE && dyn_pg_nest == DYN_NEST_NONE)
		{			
            kprintf(12, "\nmk/INTERRUPT.C: ALLOCATING");for(;;);
			dyn_pg_nest = DYN_NEST_ALLOCATING;

			last_page_fault.task_id = -1;
			last_page_fault.thread_id = curr_thread;
			last_page_fault.linear = NULL; 
			last_page_fault.pg_size = PG_SIZE;
			dyn_remaining = arch_req_pages();
			last_page_fault.flags = PF_FLAG_PGS(arch_req_pages(dyn_remaining));
			dyn_pg_thread = curr_thread;	// we will use this to return here 
											// when grant_page_mk(..) is issued
		}
		else if(dyn_pg_ret != 0) // dynamic memory page is being freed?
		{
            kprintf(12, "\nmk/INTERRUPT.C: dyn_pg_ret NOT NULL");for(;;);
			last_page_fault.task_id = -1;
			last_page_fault.thread_id = -1;
			last_page_fault.linear = arch_get_freed_physical();
			last_page_fault.pg_size = PG_SIZE;
			last_page_fault.flags = PF_FLAG_FREE;
		}	
		else
		{
			// a common page fault
			last_page_fault.task_id = curr_task;
			last_page_fault.thread_id = curr_thread;
			last_page_fault.linear = arch_get_page_fault(); 
			last_page_fault.pg_size = PG_SIZE;
			last_page_fault.flags = PF_FLAG_NONE;
			
			// did we pagefault on a kernel dynamic memory page?
			if(last_page_fault.linear < (void*)MAX_ALLOC_LINEAR)
			{
                kprintf(12, "\nmk/INTERRUPT.C: PF ON KERNEL DYNAMIC");for(;;);
				if(last_page_fault.linear < (void*)KERN_LMEM_SIZE)
					k_scr_print("\nmk/INTERRUPT.C: KERNEL SPACE PAGE FAULT!",12);for(;;);

				// try to map an existing kernel table onto the task
				if(last_page_fault.linear > (void*)KERN_LMEM_SIZE && arch_kernel_pf(last_page_fault.linear) != FAILURE)
				{
					last_page_fault.linear = (void*)0xFF; 
					last_page_fault.pg_size = 0;
					return;
				}
			}			
		}
    }
#endif

    if ((h = int_handlers[number]) >= 0) 
	{
		if (h != curr_thread && !int_active[h]) 
		{
			if (int_nesting[number]) 
			{
				if (int_stack_pointer == MAX_NESTED_INT) 
				{
					return;
				}
				int_stack[int_stack_pointer++] = curr_thread;
				int_active[h] = 1;
			}
            thread = GET_PTR(h, thr);
            task = GET_PTR(thread->task_num,tsk);
			curr_thread = h;
			curr_task = thread->task_num;
			curr_base = task->mem_adr;
			curr_priv = task->priv_level;
			last_int = number;
			arch_run_thread(h);

#ifdef PAGING
			if (IS_PAGE_FAULT(number)) 
			{
				if(dyn_pg_lvl != DYN_PGLVL_NONE && dyn_pg_nest == DYN_NEST_ALLOCATED)
				{
					/* 
					We returned to the thread which generated sartoris 
					dynamic mem fault on the first place. Decrement nesting
					so we can produce another sartoris fault.
					*/
					dyn_pg_nest = DYN_NEST_NONE;
				}
			}
#endif
		}
    }
}

int ret_from_int(void) 
{
    int x;
    int result;
	struct thread *thread = GET_PTR(curr_thread, thr);
	struct task *task = GET_PTR(thread->task_num,tsk);

    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */
    
    if (int_stack_pointer > 0) 
	{
		result = SUCCESS;

		int_active[curr_thread] = false;
		curr_thread = int_stack[--int_stack_pointer];
		curr_task = thread->task_num;
		curr_base = task->mem_adr;
		curr_priv = task->priv_level;
		arch_run_thread(curr_thread);
    }
    
    mk_leave(x); /* exit critical block */
    
    return result;
}

int get_last_int(void) 
{
    set_error(SERR_OK);
    return last_int;
}

/* remove a nested int from the stack, but keep it active.
NOTE: This function should be called only from a non nesting interrupt. */
int pop_int()
{
	int x;
	int result;

    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

	if (int_stack_pointer > 0) 
    {
        set_error(SERR_OK);
        result = SUCCESS;

        // we will leave the interrupt active
        int_stack_pointer--;
    }
    else
    {
        set_error(SERR_NO_INTERRUPT);
    }

	mk_leave(x); /* exit critical block */
    
    return result;
}

/* Insert a nested int on the stack.
NOTE: This function should be called only from a non nesting interrupt.*/
int push_int(int number)
{
	int x, h;
	int result;

    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

	if ((h = int_handlers[number]) >= 0) 
	{
		if (h != curr_thread && int_active[h]) 
		{
			if (int_nesting[number]) 
			{
				if (int_stack_pointer == MAX_NESTED_INT) 
				{
                    set_error(SERR_INTERRUPTS_MAXED);
					mk_leave(x); /* exit critical block */
					return result;
				}
                 set_error(SERR_OK);
				result = SUCCESS;
				int_stack[int_stack_pointer++] = h;
				last_int = number;
			}
            else
            {
                set_error(SERR_INTERRUPT_NOT_NESTING);
            }
		}
        else
        {
            if(h == curr_thread)
                set_error(SERR_INVALID_THR);
            else
                set_error(SERR_INTERRUPT_NOT_ACTIVE);
        }
    }
    else
    {
        set_error(SERR_NO_INTERRUPT);
    }

	mk_leave(x); /* exit critical block */
    
    return result;
}

/* This will continue execution for the first interrupt on the int stack.
NOTE: This function should be called only from a non nesting interrupt. */
int resume_int()
{
	int x;
	int result;
	struct thread *thread;
	struct task *task;

    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

	if(int_stack_pointer > 0)
	{
		curr_thread = int_stack[int_stack_pointer-1];
		
		thread = GET_PTR(curr_thread, thr);

		curr_task = thread->task_num;

		task = GET_PTR(curr_task,tsk);

		curr_base = task->mem_adr;
		curr_priv = task->priv_level;
		arch_run_thread(curr_thread);

        set_error(SERR_OK);
		result = SUCCESS;
	}
    else
    {
        set_error(SERR_NO_INTERRUPT);
    }

	mk_leave(x); /* exit critical block */
    
    return result;
}





