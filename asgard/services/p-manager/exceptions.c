/*
*	Process and Memory Manager Service
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/


#include <os/layout.h>
#include "pmanager_internals.h"
#include "exception.h"

extern int running;
extern struct pm_task   task_info[MAX_TSK];
extern struct pm_thread thread_info[MAX_THR];

void pm_spawn_handlers(void) 
{
  struct thread hdl;
    
  hdl.task_num = PMAN_TASK;
  hdl.invoke_mode = PRIV_LEVEL_ONLY;
  hdl.invoke_level = 0;
  hdl.ep = &pm_handler;
  hdl.stack = (void*)STACK_ADDR(PMAN_EXSTACK_ADDR);
  
  create_thread(EXC_HANDLER_THR, &hdl);
  
  create_int_handler(DIV_ZERO, EXC_HANDLER_THR, false, 0);
  create_int_handler(OVERFLOW, EXC_HANDLER_THR, false, 0);
  create_int_handler(BOUND, EXC_HANDLER_THR, false, 0);
  create_int_handler(INV_OPC, EXC_HANDLER_THR, false, 0);
  create_int_handler(DEV_NOT_AV, EXC_HANDLER_THR, false, 0);
  create_int_handler(STACK_FAULT, EXC_HANDLER_THR, false, 0);
  create_int_handler(GEN_PROT, EXC_HANDLER_THR, false, 0);
  create_int_handler(PAGE_FAULT, EXC_HANDLER_THR, false, 0);
  create_int_handler(FP_ERROR, EXC_HANDLER_THR, false, 0);
  create_int_handler(ALIG_CHK, EXC_HANDLER_THR, false, 0);
  create_int_handler(SIMD_FAULT, EXC_HANDLER_THR, false, 0);
}

void pm_handler(void) {
  int prog_id, j;
  int exception;
  int rval = 0;
  char done;
  
  for(;;) {

	exception = get_last_int();

	if(running == PAGEAGING_THR)
	{
		pman_print_and_stop("Page aging exception ex=%i", exception);
	}
    else if (exception == PAGE_FAULT) 
	{ 
		int thread_id;

		if (pm_handle_page_fault(&thread_id, 0)) 
		{

#ifdef INTRET_EXPERIMENTAL
			/* Page fault interrupt is not nesting. So pop_int should put
			last int being executed on hold. */
			pop_int();

			// just in case there was a nested int, resume. 
			// if resume fails, run the scheduler.
			if(!resume_int())
#else 
				run_thread(SCHED_THR);
#endif

		} else {
		
#ifdef INTRET_EXPERIMENTAL
			/* Page was allocated without reading from disk. nice :) */
			if(thread_info[thread_id].state == THR_INTHNDL)
			{
				resume_int();
			}
			else
			{
				run_thread(running);
			}
#else
			run_thread(running);
#endif
		}

    } 
	else 
	{    
		prog_id = running;

		switch(exception) {
			case DIV_ZERO:
				rval = DIV_ZERO_RVAL;			
				break;
			case OVERFLOW:
				rval = OVERFLOW_RVAL;
				break;
			case BOUND:
				rval = BOUND_RVAL;
				break;
			case INV_OPC:
				rval = INV_OPC_RVAL;
				break;
			case DEV_NOT_AV:
				rval = DEV_NOT_AV_RVAL;
				break;
			case STACK_FAULT:
				rval = STACK_FAULT_RVAL;
				break;
			case GEN_PROT:
				rval = GEN_PROT_RVAL;
				break;
			case PAGE_FAULT:
				rval = PAGE_FAULT_RVAL;
				break;
			case FP_ERROR:
				rval = FP_ERROR_RVAL;
				break;
			case ALIG_CHK:
				rval = ALIG_CHK_RVAL;
				break;
			case SIMD_FAULT:
				rval = SIMD_FAULT_RVAL;
				break;
		}

		if(running > MAX_TSK || running < 0)
		{
			pman_print_and_stop("Exception on process manager, invalid running id ex=%i", exception);
		}

		if(!task_info[thread_info[running].task_id].flags & TSK_FLAG_SYS_SERVICE)
		{
			task_info[thread_info[running].task_id].ret_value = rval;
			task_info[thread_info[running].task_id].destroy_sender_id = -1;
			task_info[thread_info[running].task_id].state = TSK_KILLED;
			task_info[thread_info[running].task_id].destroy_req_id = -1;
			task_info[thread_info[running].task_id].destroy_ret_port = -1;

			pm_request_close( thread_info[running].task_id );

			run_thread(SCHED_THR);
		}
		else
		{
			pman_print_and_stop("System Service exception tsk=%i, thr=%i", thread_info[running].task_id, running);
			
			thread_info[running].state = THR_EXEPTION;

			run_thread(SCHED_THR);
		}
    }
  }
  
}

