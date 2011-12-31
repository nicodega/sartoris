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
#include "sartoris/syscall.h"

/* interrupt management implementation */
extern unsigned int exc_error_code; // it's defined on arch interrupt.c
extern void *exc_int_addr;   // it's defined on arch interrupt.c

int create_int_handler(int number, int thread_id, int nesting, int priority) 
{
    int x;
    int result;	
    
    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

    if (0 <= number && number < MAX_IRQ && 
        0 <= thread_id && thread_id < MAX_THR &&
        TST_PTR(thread_id,thr) && int_handlers[number].thr_id < 0) 
	{
		if (arch_create_int_handler(number) == 0) 
		{
            set_error(SERR_OK);
            result = SUCCESS;
	
			if (nesting) 
				int_handlers[number].int_flags = INT_FLAG_NESTING;
			else
				int_handlers[number].int_flags = INT_FLAG_NONE;
		
			int_handlers[number].thr_id = thread_id;

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
        else if(int_handlers[number].thr_id >= 0)
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

    if (number >= 0 && number < MAX_IRQ && int_handlers[number].thr_id == thread 
        && (int_handlers[number].int_flags & INT_FLAG_ACTIVE) == 0) 
	{
		if (arch_destroy_int_handler(number) == 0) 
		{
            set_error(SERR_OK);
			result = SUCCESS;
			int_handlers[number].thr_id = -1;
            int_handlers[number].int_flags = INT_FLAG_NONE;
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
        else if(int_handlers[number].thr_id != thread)
            set_error(SERR_INVALID_THR); 
        else
            set_error(SERR_INTERRUPT_ACTIVE);
    }

    mk_leave(x); /* exit critical block */
    
    return result;
}

/* note: the following function is not invoked from userland, 
   only from the arch-kernel section */

/* atomicity is assumed */
void handle_int(int number) 
{
    int h, hint;
	struct thread *thread;
	struct task *task;
    
#ifdef PAGING
    /* IS_PAGE_FAULT comes from kernel-arch.h */
	if (IS_PAGE_FAULT(number)) 
	{		
        thread = GET_PTR(curr_thread, thr);
        task = GET_PTR(thread->task_num,tsk);

		thread->page_faulted = 1;

		/*
		If dynamic memory request level is not NONE and it's not nested, or 
		we are in the middle of freeing a page.
		*/
        if(dyn_pg_thread == curr_thread)
        {
			dyn_pg_nest = DYN_NEST_ALLOCATING;

			last_page_fault.task_id = -1;
			last_page_fault.thread_id = curr_thread;
			last_page_fault.linear = NULL; 
			last_page_fault.pg_size = PG_SIZE;
            dyn_remaining = arch_req_pages();
			last_page_fault.flags = PF_FLAG_PGS(dyn_remaining);
        }
        else if(dyn_pg_ret_thread == curr_thread) // dynamic memory page is being freed?
		{
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
                bprintf("mk/INTERRUPT.C: PF ON KERNEL DYNAMIC\n");

				// try to map an existing kernel table onto the task
				if(last_page_fault.linear > (void*)KERN_LMEM_SIZE 
                    && arch_kernel_pf(last_page_fault.linear) != FAILURE)
				{
					return;
				}
			}			
		}
    }
#endif

    if ((h = int_handlers[number].thr_id) >= 0) 
	{
		if (h != curr_thread && handling_int[h] == MAX_IRQ && (int_handlers[number].int_flags & INT_FLAG_ACTIVE) == 0) 
		{
            thread = GET_PTR(h, thr);

			if (int_handlers[number].int_flags & INT_FLAG_NESTING) 
			{
				if (int_stack_count == MAX_NESTED_INT) 
					return;
                if(int_stack_count == 0)
                    stack_first_thread = curr_thread;
                // add int to the stack
                int_stack_count++;
				int_handlers[number].prev = int_stack_first;
                int_stack_first = number;
                
                thread->last_runned_int = number;
			}
			
            handling_int[h] = number;

            task = GET_PTR(thread->task_num,tsk);

            // if the current thread is handling a non nesting interrupt
            // we should set handling_int[curr_trhead] to MAX_IRQ
            hint = handling_int[curr_thread];
            if(hint != MAX_IRQ && (int_handlers[hint].int_flags & INT_FLAG_NESTING) == 0)
            {
                handling_int[curr_thread] = MAX_IRQ;
                int_handlers[hint].int_flags &= ~INT_FLAG_ACTIVE;
            }

			curr_thread = h;
			curr_task = thread->task_num;
			curr_base = task->mem_adr;
			curr_priv = task->priv_level;
			last_int = number;
            int_handlers[number].int_flags |= (INT_FLAG_ACTIVE | INT_FLAG_ACTIVATED);
			arch_run_thread(h);

            if(thread->evts)
            {
                evt_raise(h, SARTORIS_EVT_INT, number);
            }
        }
        else if(h != curr_thread && (int_handlers[number].int_flags & INT_FLAG_ACTIVE) == 0 && (int_handlers[number].int_flags & INT_FLAG_NESTING))
        {
            if (int_stack_count == MAX_NESTED_INT) 
					return;
			/* 
            the thread is already handling an int, but this int is not active.
            Push the incoming int on the stack but before the thread las raised int. 
            */
            int_stack_count++;
            
            int_handlers[number].prev = int_handlers[thread->last_runned_int].prev;
			int_handlers[thread->last_runned_int].prev =  number;
            thread->last_runned_int = number;

            if(thread->last_poped_int != -1)
            {
                // add the int to the front of this list
                thread->last_poped_int = number;
            }
        }
    }
}

int ret_from_int(void) 
{
    int x;
    int result;
	struct thread *thread = NULL;
	struct task *task = NULL;

    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

    if(arch_is_soft_int() || handling_int[curr_thread] == MAX_IRQ)
    {
        mk_leave(x);
        return;
    }

    if (int_stack_count > 0 && handling_int[curr_thread] == int_stack_first) 
	{
		result = SUCCESS;
        
        /* Remove the int from the stack and set the current 
           thread to the next one on the stack. */

        int_stack_count--;
        int_handlers[int_stack_first].int_flags &= ~INT_FLAG_ACTIVE;
        int_stack_first = int_handlers[int_stack_first].prev;
		if(int_stack_first == -1)
        {
            handling_int[curr_thread] = MAX_IRQ;
            curr_thread = stack_first_thread;
            stack_first_thread = -1;
        }
        else
        {
            if(curr_thread != int_handlers[int_stack_first].thr_id)
                handling_int[curr_thread] = MAX_IRQ;
            else
                handling_int[curr_thread] = int_handlers[int_stack_first].int_num;
            curr_thread = int_handlers[int_stack_first].thr_id;
        }
        
        thread = GET_PTR(curr_thread, thr);
        task = GET_PTR(thread->task_num,tsk);
		
        curr_task = thread->task_num;
		curr_base = task->mem_adr;
		curr_priv = task->priv_level;
		
        result = arch_run_thread(curr_thread);

        // this should never happen.. if it does, die.
        if(result == FAILURE)
        {
            kprintf(12, "An interrupt tried to go back to a dead thread!!");
            for(;;);
        }
    }
    
    mk_leave(x); /* exit critical block */
    
    return result;
}

int get_last_int(unsigned int *error_code) 
{
    if(error_code != NULL && VALIDATE_PTR(error_code))
         *((unsigned int*)MAKE_KRN_PTR(error_code)) = exc_error_code;
    set_error(SERR_OK);
    return last_int;
}

void *get_last_int_addr() 
{
    set_error(SERR_OK);
    return exc_int_addr;
}

/* remove a nested int from the stack, but keep it active.
NOTE: This function should be called only from a non nesting interrupt. */
int pop_int()
{
	int x;
	int result;
    int prev, id;
    struct thread *thread;

    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

	if (int_stack_count > 0) 
    {
        set_error(SERR_OK);
        result = SUCCESS;

        // we will leave the interrupt active
        // so it won't trigger again
        // NOTE: We will remove all ints for the thread on the stack
        // and push them in the thread->last_poped_int list.
        id = int_handlers[int_stack_first].thr_id;
        thread = GET_PTR(id, thr);
        
        do
        {
            prev = int_handlers[int_stack_first].prev;
            int_handlers[int_stack_first].prev = thread->last_poped_int;
            thread->last_poped_int = int_stack_first;
            int_stack_first = prev;
        }while(int_stack_first != -1 && int_handlers[int_stack_first].thr_id == id);
    }
    else
    {
        set_error(SERR_NO_INTERRUPT);
    }

	mk_leave(x); /* exit critical block */
    
    return result;
}

/* Insert a nested int on the stack.
NOTE: This function should be called only from a non nesting interrupt for an already
poped interrupt. */
int push_int(int number)
{
	int x, h, hint;
	int result;
    int i, prev;
    struct thread *thread;
    struct task *task;

    result = FAILURE;
    
    x = mk_enter(); /* enter critical block */

	if ((h = int_handlers[number].thr_id) >= 0) 
	{
		if (h != curr_thread && handling_int[h] == MAX_IRQ && (int_handlers[number].int_flags & INT_FLAG_ACTIVE) != 0) 
		{
			if (int_handlers[number].int_flags & INT_FLAG_NESTING) 
			{
                // Put all interrupts for the thread on the stack again
                thread = GET_PTR(int_handlers[number].thr_id, thr);

                i = thread->last_poped_int;

                if(i != -1)
                {
                    set_error(SERR_OK);
                    result = SUCCESS;

                    do
                    {
                        prev = int_handlers[i].prev;
                        int_handlers[i].prev = int_stack_first;
                        int_stack_first = i;
                        i = prev;
                    }while(prev != -1);

				    last_int = number;
                    thread->last_poped_int = -1;

                    // if the current thread is handling a non nesting interrupt
                    // we should set handling_int[curr_trhead] to MAX_IRQ
                    hint = handling_int[curr_thread];
                    if(hint != MAX_IRQ && (int_handlers[hint].int_flags & INT_FLAG_NESTING) == 0)
                    {
                        bprintf("SARTORIS: push_int: handling_int reset.\n");
                        handling_int[curr_thread] = MAX_IRQ;
                        int_handlers[hint].int_flags &= ~INT_FLAG_ACTIVE;
                    }

                    /* run the first int on the stack */
                    task = GET_PTR(thread->task_num,tsk);
			        curr_thread = int_handlers[number].thr_id;
			        curr_task = thread->task_num;
			        curr_base = task->mem_adr;
			        curr_priv = task->priv_level;
			        arch_run_thread(h);
                }
                else
                {
                    set_error(SERR_INTERRUPT_NOT_ACTIVE);
                }
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

/* This will continue execution for the first interrupt on the int stack
or the stack_first_thread if there was no int.
NOTE: This function should be called only from a non nesting interrupt. */ 
int resume_int() 
{         
    int x, hint;         
    int result;         
    struct thread *thread;         
    struct task *task;      
    result = FAILURE;          
    
    x = mk_enter();    
    
    if(int_stack_count > 0)         
    {
        set_error(SERR_OK);

        // if the current thread is handling a non nesting interrupt
        // we should set handling_int[curr_trhead] to MAX_IRQ
        hint = handling_int[curr_thread];
        if(hint != MAX_IRQ && (int_handlers[hint].int_flags & INT_FLAG_NESTING) == 0)
        {
            bprintf("SARTORIS: push_int: handling_int reset.\n");
            handling_int[curr_thread] = MAX_IRQ;
            int_handlers[hint].int_flags &= ~INT_FLAG_ACTIVE;
        }

        /* run the first int on the stack */
        curr_thread = int_handlers[int_stack_first].thr_id;

        thread = GET_PTR(curr_thread, thr);
        task = GET_PTR(thread->task_num,tsk);

		curr_task = thread->task_num;
		curr_base = task->mem_adr;
		curr_priv = task->priv_level;
		result = arch_run_thread(curr_thread);
    }
    else if(stack_first_thread != -1)
    {
        set_error(SERR_OK);

        curr_thread = stack_first_thread;
        stack_first_thread = -1;

        thread = GET_PTR(curr_thread, thr);
        task = GET_PTR(thread->task_num,tsk);

		curr_task = thread->task_num;
		curr_base = task->mem_adr;
		curr_priv = task->priv_level;
		result = arch_run_thread(curr_thread);
    }
    else
    {
        set_error(SERR_NO_INTERRUPT);
    }
    
    mk_leave(x);
    
    return result;
} 
