/*  
 *   Sartoris microkernel interrupt handling arch-neutral functions
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

/* interrupt management implementation */

int create_int_handler(int number, int thread, int nesting, int priority) 
{
    int x;
    int result;
    
    result = FAILURE;
    
    x = arch_cli(); /* enter critical block */
    
    if (0 <= number && number < MAX_IRQ && threads[thread].task_num >= 0 && int_handlers[number] < 0) 
	{
		if (arch_create_int_handler(number) == 0) 
		{
			result = SUCCESS;
	
			if (nesting) 
				int_nesting[number] = 1;
			else
				int_nesting[number] = 0;
		
			int_handlers[number] = thread;
			int_active[thread] = 0;
		}
    }
    
    arch_sti(x); /* exit critical block */
    
    return result;
}

int destroy_int_handler(int number, int thread) {
    int x;
    int result;
    
    result = FAILURE;
    
    x = arch_cli(); /* enter critical block */

    if (number < MAX_IRQ && int_handlers[number] == thread) {
      if (arch_destroy_int_handler(number) == 0) {
	result = SUCCESS;
	int_handlers[number] = -1;
      }
    }
    
    arch_sti(x); /* exit critical block */
    
    return result;
}

/* note: the following function is not invoked from userland, 
   only from the arch-kernel section */

/* atomicity is assumed */

void handle_int(int number) 
{
    int h;

#ifdef PAGING
    
    if (IS_PAGE_FAULT(number)) 
	{
		/* IS_PAGE_FAULT comes from kernel-arch.h */

		threads[curr_thread].page_faulted = 1;

		last_page_fault.task_id = curr_task;
		last_page_fault.thread_id = curr_thread;
		last_page_fault.linear = arch_get_page_fault();      
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
			curr_thread = h;
			curr_task = threads[h].task_num;
			curr_base = tasks[curr_task].mem_adr;
			curr_priv = tasks[curr_task].priv_level;
			last_int = number;
			arch_run_thread(h);
		}
    }
}

int ret_from_int(void) 
{
    int x;
    int result;

    result = FAILURE;
    
    x = arch_cli(); /* enter critical block */
    
    if (int_stack_pointer > 0) 
	{
		result = SUCCESS;

		int_active[curr_thread] = false;
		curr_thread = int_stack[--int_stack_pointer];
		curr_task = threads[curr_thread].task_num;
		curr_base = tasks[curr_task].mem_adr;
		curr_priv = tasks[curr_task].priv_level;
		arch_run_thread(curr_thread);
    }
    
    arch_sti(x); /* exit critical block */
    
    return result;
}

int get_last_int(void) 
{
    return last_int;
}

/* remove a nested int from the stack, but keep it active.
NOTE: This function should be called only from a non nesting interrupt. */
int pop_int()
{
	int x;
	int result;

    result = FAILURE;
    
    x = arch_cli(); /* enter critical block */

	if (int_stack_pointer > 0) {
      
      result = SUCCESS;
      
	  // we will leave the interrupt active
      int_stack_pointer--;
    }

	arch_sti(x); /* exit critical block */
    
    return result;
}

/* Insert a nested int on the stack.
NOTE: This function should be called only from a non nesting interrupt.*/
int push_int(int number)
{
	int x, h;
	int result;

    result = FAILURE;
    
    x = arch_cli(); /* enter critical block */

	if ((h = int_handlers[number]) >= 0) {
		if (h != curr_thread && int_active[h]) {
			if (int_nesting[number]) {
				if (int_stack_pointer == MAX_NESTED_INT) {
					arch_sti(x); /* exit critical block */
					return result;
				}
				result = SUCCESS;
				int_stack[int_stack_pointer++] = h;
				last_int = number;
			}
		}
    }

	arch_sti(x); /* exit critical block */
    
    return result;
}

/* This will continue execution for the first interrupt on the int stack.
NOTE: This function should be called only from a non nesting interrupt. */
int resume_int()
{
	int x;
	int result;

    result = FAILURE;
    
    x = arch_cli(); /* enter critical block */

	if(int_stack_pointer > 0)
	{
		curr_thread = int_stack[int_stack_pointer-1];
		curr_task = threads[curr_thread].task_num;
		curr_base = tasks[curr_task].mem_adr;
		curr_priv = tasks[curr_task].priv_level;
		arch_run_thread(curr_thread);

		result = SUCCESS;
	}

	arch_sti(x); /* exit critical block */
    
    return result;
}





