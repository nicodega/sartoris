
#include "types.h"
#include "pman_print.h"
#include "interrupts.h"
#include "exception.h"
#include "scheduler.h"
#include "vm.h"
#include "kmalloc.h"
#include <services/pmanager/signals.h>
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "signals.h"
#include "layout.h"

/* Container for signals waiting for a given interrupt. */
struct interrupt_signals_container interrupt_signals[MAX_INTERRUPT];

void int_common_handler();

void int_init()
{
	struct thread hdl;
	struct pm_thread *pmthr;
	UINT32 i;

	/* Create generic exceptions handler. */
	hdl.task_num = PMAN_TASK;
	hdl.invoke_mode = PRIV_LEVEL_ONLY;
	hdl.invoke_level = 0;
	hdl.ep = &gen_ex_handler;
	hdl.stack = (ADDR)STACK_ADDR(PMAN_EXSTACK_ADDR);

	if(create_thread(EXC_HANDLER_THR, &hdl))
		pman_print_and_stop("INT: FAILED To create Exception handler Thread");

	create_int_handler(DIV_ZERO, EXC_HANDLER_THR, false, 0);
	create_int_handler(OVERFLOW, EXC_HANDLER_THR, false, 0);
	create_int_handler(BOUND, EXC_HANDLER_THR, false, 0);
	create_int_handler(INV_OPC, EXC_HANDLER_THR, false, 0);
	create_int_handler(DEV_NOT_AV, EXC_HANDLER_THR, false, 0);
	create_int_handler(STACK_FAULT, EXC_HANDLER_THR, false, 0);
	create_int_handler(GEN_PROT, EXC_HANDLER_THR, false, 0);
	create_int_handler(FP_ERROR, EXC_HANDLER_THR, false, 0);
	create_int_handler(ALIG_CHK, EXC_HANDLER_THR, false, 0);
	create_int_handler(SIMD_FAULT, EXC_HANDLER_THR, false, 0);

	pmthr = thr_get(EXC_HANDLER_THR);
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	// ehm... well... it IS an interrupt handler :D

	/* Create Page Fault Handler */
	hdl.task_num = PMAN_TASK;
	hdl.invoke_mode = PRIV_LEVEL_ONLY;
	hdl.invoke_level = 0;
	hdl.ep = &vmm_paging_interrupt_handler;
	hdl.stack = (ADDR)STACK_ADDR(PMAN_PGSTACK_ADDR);

	if(create_thread(PGF_HANDLER_THR, &hdl))
		pman_print_and_stop("INT: FAILED To create PF handler Thread");

	create_int_handler(PAGE_FAULT, PGF_HANDLER_THR, FALSE, 0);

	pmthr = thr_get(PGF_HANDLER_THR);
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	

	/* Create generic interrupt stub (for threads wanting to handle the interrupt using signals) */
	hdl.task_num = PMAN_TASK;
	hdl.invoke_mode = PRIV_LEVEL_ONLY;
	hdl.invoke_level = 0;
	hdl.ep = &int_common_handler;
	hdl.stack = (ADDR)STACK_ADDR(PMAN_INTCOMMON_STACK_ADDR);

	if(create_thread(INT_HANDLER_THR, &hdl))
		pman_print_and_stop("INT: FAILED To create Interrupt handler Thread");

	for(i = IA32FIRST_INT; i < MAX_INTERRUPT; i++)
	{
		//if(i != 32) create_int_handler(i, INT_HANDLER_THR, TRUE, 10);
		interrupt_signals[i].first = NULL;
		interrupt_signals[i].total = 0;
	}

	pmthr = thr_get(INT_HANDLER_THR);
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	
}


	
/* Generic Exceptions Handler */
void gen_ex_handler()
{
	int prog_id, exception, rval = 0;
	struct pm_thread *thr;
	struct pm_task *tsk;

	for(;;) 
	{
		/* Get last exception raised */
        exception = get_last_int();

		/* Get running program */
		prog_id = sch_running();

		switch(exception) 
		{
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

		if(prog_id > MAX_TSK || sch_running() == 0xFFFF)
			pman_print_and_stop("Exception on process manager, invalid running id ex=%i, id=%i", exception, prog_id);
		
		thr = thr_get(prog_id);
		tsk = tsk_get(thr->task_id);

		if(tsk == NULL)
		{
			pman_print_and_stop("EXCEPTION: NULL TASK");
			run_thread(SCHED_THR);
		}
		
		if(!(tsk->flags & TSK_FLAG_SYS_SERVICE))
		{
			pman_print_and_stop("PROCESS EXCEPTION");
			tsk->command_inf.ret_value = rval;
			tsk->command_inf.command_sender_id = 0;
			tsk->state = TSK_KILLING;
			tsk->command_inf.command_req_id = -1;
			tsk->command_inf.command_ret_port = -1;

			fatal_exception( tsk->id, rval );

			run_thread(SCHED_THR);
		}
		else
		{
			//pman_print_and_stop("System Service exception tsk=%i, thr=%i, ex=%i ", tsk->id, thr->id, exception);
			pman_print_and_stop("System Service exception tsk=%i, thr=%i, ex=%i ", tsk->id, thr->id, exception);
			
			thr->state = THR_EXEPTION;

			run_thread(SCHED_THR);
		}
	}
}
  
BOOL is_int0_active()
{
	int res = 0;
	// 0xB = 1011 meaning we will read the ISR (In Service Reg) from the first PIC
	__asm__ __volatile__ ("xor %%eax,%%eax;movb $0x0B, %%al;outb %%al, $0x20;inb $0x20, %%al;andl $0x1, %%eax;movl %%eax, %0" : "=r" (res) :: "eax");
	return res;
}

/* Check if this thread can attach to this interrupt */
BOOL int_can_attach(struct pm_thread *thr, UINT32 interrupt)
{
	/* Check interrupt is not already being handled directly. */
	struct pm_thread *thread;
	UINT16 i = 0;

	/* Check interrupt is not being signaled. */
	if(interrupt_signals[interrupt].total != 0 || interrupt == 32) return FALSE; // 32 is the scheduler timer interrupt

	while(i < MAX_THR)
	{
		thread = thr_get(i);
		if(thread != NULL && thread->state == THR_INTHNDL && thread->interrupt == interrupt)
			return FALSE;
		i++;
	}

	return TRUE;
}

/* create the interrupt handler */
BOOL int_attach(struct pm_thread *thr, UINT32 interrupt, int priority)
{
	//if(IA32FIRST_INT <= interrupt && interrupt != 32 && interrupt < MAX_INTERRUPT)
	//	destroy_int_handler(interrupt, INT_HANDLER_THR);

	if(create_int_handler(interrupt, thr->id, true, priority) != SUCCESS)
		return FALSE;
	return TRUE;
}

BOOL int_dettach(struct pm_thread *thr)
{
	destroy_int_handler(thr->interrupt, thr->id);
	if(create_int_handler(thr->interrupt, INT_HANDLER_THR, 1, 10) != SUCCESS)
		return FALSE;
	return TRUE;
}

UINT32 int_clear()
{
	__asm__("cli" ::);
	return 0;
}

void int_set(UINT32 x)
{
	if(x != 1) __asm__("sti" ::);
}

extern int debug_cont;
/*
Generic Interrupt handler.
This handler will send a signal to each process waiting for the interrupt.
*/
void int_common_handler()
{
	pman_print_and_stop("INT COMMON HANDLER!");
	
	INT32 interrupt, i = 0;
	struct event_cmd evt;
	struct thr_signal *isignal = NULL, *last_isignal = NULL, *next_signal = NULL;

	int_clear();
	
	evt.command = EVENT;
	evt.event_type = PMAN_INTR;
	evt.task = PMAN_GLOBAL_EVENT;
	evt.event_res0 = 0;
	evt.event_res1 = 0;

	for(;;)
	{
		interrupt = get_last_int();

		/* Find tasks waiting for an interrupt signal, and send it. */
		if(interrupt_signals[interrupt].total != 0)
		{
			evt.param1 = interrupt;

			isignal = interrupt_signals[interrupt].first;

			while(isignal != NULL)
			{
				/* Send the signal */
				send_signal(isignal, &evt, SIGNAL_OK);

				/* Reschedule thread so it executes before other threads. */
				sch_reschedule(isignal->thread, i);
				
				next_signal = isignal->inext;

				/* Discard signal if not repeating */
				if(isignal->timeout != PMAN_SIGNAL_REPEATING)
				{
					remove_signal(isignal, isignal->thread->id);

					/* Remove from interrupt signals */
					if(last_isignal == NULL)
						interrupt_signals[interrupt].first = isignal->inext; 	/* Signal is first on the list */					
					else
						last_isignal->inext = isignal->inext;
					
					interrupt_signals[interrupt].total--;

					/* Free signals */
					kfree(isignal);					
				}
				else
				{
					last_isignal = isignal;
				}

				isignal = next_signal;

				i++;
			}
		}

		/* FIX: Ack PICs if int is from hardware */

		/* Tell Scheduler to move our current executing thread */
		//sch_force_complete();

		// FIX: Nesting tasks must ret_from_int();
		//run_thread(SCHED_THR);
		ret_from_int();
	}
}


BOOL int_signal(struct pm_thread *thread, struct thr_signal *signal, INT32 interrupt)
{
	if(interrupt_signals[interrupt].total > 0 || int_can_attach(thread, interrupt))
	{
		/* Add signal to the list */
		if(interrupt_signals[interrupt].first == NULL)
		{
			interrupt_signals[interrupt].first = signal;
			signal->inext = NULL;
		}
		else
		{
			signal->inext = interrupt_signals[interrupt].first;
			interrupt_signals[interrupt].first = signal;
		}

		interrupt_signals[interrupt].total++;

		return TRUE;
	}
	return FALSE;
}
